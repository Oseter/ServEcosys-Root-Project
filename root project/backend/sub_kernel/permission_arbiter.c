/**
 * ServEcosys SED - Permission Adjudication Service
 *
 * 职责：
 * - 接收来自 UID 的权限请求
 * - 根据权限阶梯（0-11）裁决请求
 * - 维护进程权限级别标记
 * - 向 UID 返回裁决结果
 *
 * 安全模型：
 * - 提权必须由用户显式授权（用户指定的应用/进程）
 * - 进程只能管理自己（经 SO_PEERCRED 校验真实发起者）
 * - 未授权进程一律不得提权
 *
 * 运行在 sys_dom_t 域
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include "selinux_wrap.h"

#define ARBITER_VERSION   "0.1.0"
#define IPC_SOCK_PATH     "/var/run/servecosys_perm.sock"
#define PID_FILE          "/var/run/permission_arbiter.pid"
#define MAX_REQUESTS      1024
#define MAX_MSG_SIZE      4096

typedef enum {
    PERM_LEVEL_READONLY       = 0,
    PERM_LEVEL_SANDBOX        = 1,
    PERM_LEVEL_USER           = 2,
    PERM_LEVEL_DEBUG          = 3,
    PERM_LEVEL_BL_UNLOCK      = 4,
    PERM_LEVEL_ROOT_SPLIT     = 5,
    PERM_LEVEL_MODULE_ROOT    = 6,
    PERM_LEVEL_KERNEL_ROOT    = 7,
    PERM_LEVEL_SELINUX        = 8,
    PERM_LEVEL_KMOD_LOAD      = 9,
    PERM_LEVEL_CUSTOM_KERNEL  = 10,
    PERM_LEVEL_BOOTLOADER     = 11,
} perm_level_t;

typedef enum {
    REQ_CHECK_PERM,
    REQ_GET_LEVEL,
    REQ_SET_LEVEL,
    REQ_QUERY_CAP,
    REQ_REGISTER_MANAGER,
    REQ_AUTHORIZE,
} request_type_t;

typedef struct {
    request_type_t type;
    pid_t          pid;
    int            target_level;
    char           capability[64];
    char           resource[256];
    int            request_id;
} perm_request_t;

typedef struct {
    int     status;      /* 0=granted, -1=denied */
    int     current_level;
    int     request_id;
    char    reason[128];
} perm_response_t;

/*
 * user_authorized_cap = 用户明确授权该进程可达到的最高权限级。
 * 未授权（0）的进程一律不得提权。授权只能由系统管理器下达。
 */
typedef struct {
    pid_t           pid;
    perm_level_t    level;
    char            selinux_context[256];
    int             is_signed;
    int             is_self_signed;
    perm_level_t    user_authorized_cap;
} proc_cred_t;

static proc_cred_t proc_table[MAX_REQUESTS];
static int proc_count = 0;
static volatile sig_atomic_t running = 1;
static pid_t manager_pid = -1;   /* 已注册的系统管理器 PID */

static const char *level_name(perm_level_t level)
{
    switch (level) {
        case PERM_LEVEL_READONLY:      return "readonly";
        case PERM_LEVEL_SANDBOX:       return "sandbox";
        case PERM_LEVEL_USER:          return "user";
        case PERM_LEVEL_DEBUG:         return "debug";
        case PERM_LEVEL_BL_UNLOCK:     return "bl_unlock";
        case PERM_LEVEL_ROOT_SPLIT:    return "root_split";
        case PERM_LEVEL_MODULE_ROOT:   return "module_root";
        case PERM_LEVEL_KERNEL_ROOT:   return "kernel_root";
        case PERM_LEVEL_SELINUX:       return "selinux_control";
        case PERM_LEVEL_KMOD_LOAD:     return "kmod_load";
        case PERM_LEVEL_CUSTOM_KERNEL: return "custom_kernel";
        case PERM_LEVEL_BOOTLOADER:    return "bootloader";
        default:                       return "unknown";
    }
}

static int register_process(pid_t pid, perm_level_t level)
{
    for (int i = 0; i < proc_count; i++) {
        if (proc_table[i].pid == pid) {
            proc_table[i].level = level;
            return i;
        }
    }

    if (proc_count >= MAX_REQUESTS)
        return -1;

    proc_table[proc_count].pid = pid;
    proc_table[proc_count].level = level;
    proc_table[proc_count].is_signed = 0;
    proc_table[proc_count].is_self_signed = 0;
    proc_table[proc_count].user_authorized_cap = PERM_LEVEL_READONLY;
    if (selinux_get_context(pid, proc_table[proc_count].selinux_context,
                            sizeof(proc_table[proc_count].selinux_context)) != 0) {
        snprintf(proc_table[proc_count].selinux_context,
                 sizeof(proc_table[proc_count].selinux_context),
                 "u:r:app_sandbox_t:s0");
    }

    return proc_count++;
}

/* 经 SO_PEERCRED 获取真实发起者 PID，杜绝 pid 伪造 */
static pid_t get_peer_pid(int client_fd)
{
    struct ucred cred;
    socklen_t len = sizeof(cred);

    if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0)
        return -1;
    if (cred.pid == 0)
        return -1;
    return cred.pid;
}

static int find_process(pid_t pid)
{
    for (int i = 0; i < proc_count; i++)
        if (proc_table[i].pid == pid)
            return i;
    return -1;
}

static perm_level_t get_process_level(pid_t pid)
{
    for (int i = 0; i < proc_count; i++)
        if (proc_table[i].pid == pid)
            return proc_table[i].level;

    return PERM_LEVEL_SANDBOX;
}

static int check_permission(pid_t pid, perm_level_t required)
{
    perm_level_t current = get_process_level(pid);

    if (current >= required)
        return 0;

    return -1;
}

/*
 * 设置进程权限级。安全约束：
 *  - 只能操作自己（peer_pid 必须等于被操作 pid），禁止替他人提权
 *  - 降级/同级始终允许
 *  - 提权必须落在用户授权上限 (user_authorized_cap) 之内
 */
static int set_process_level(pid_t pid, perm_level_t new_level, pid_t peer_pid)
{
    if (new_level > PERM_LEVEL_BOOTLOADER || new_level < PERM_LEVEL_READONLY)
        return -1;

    if (peer_pid != pid)
        return -1;

    perm_level_t current = get_process_level(pid);

    if (new_level <= current)
        goto apply;

    int idx = find_process(pid);
    if (idx < 0 || new_level > proc_table[idx].user_authorized_cap) {
        fprintf(stdout, "[ARBITER] Denied: PID %d escalation to %d unauthorised "
                        "(cap %d)\n", pid, new_level,
                idx >= 0 ? proc_table[idx].user_authorized_cap : 0);
        return -1;
    }

    if (new_level > PERM_LEVEL_DEBUG && new_level < PERM_LEVEL_SELINUX) {
        if (!is_signed_by_official(pid))
            return -1;
    }

    /* SELinux admission: the process's domain must be allowed to control
     * itself, otherwise escalation is refused (deny-by-default). */
    if (selinux_check(proc_table[idx].selinux_context,
                      proc_table[idx].selinux_context,
                      "process", "signal") != 0) {
        fprintf(stdout, "[ARBITER] Denied: PID %d domain %s lacks process control\n",
                pid, proc_table[idx].selinux_context);
        return -1;
    }

apply:
    int idx2 = register_process(pid, new_level);
    if (idx2 >= 0) {
        proc_table[idx2].level = new_level;
        fprintf(stdout, "[ARBITER] PID %d level set to %s (%d)\n",
                pid, level_name(new_level), new_level);
        return 0;
    }

    return -1;
}

/*
 * 注册系统管理器。只有首个声明者可成为 manager，后续声明一律拒绝，
 * 但若已注册的 manager 已退出（崩溃/重启），允许新实例接管管理权。
 */
static int register_manager(pid_t peer_pid)
{
    if (manager_pid >= 0) {
        /* 原 manager 仍存活则拒绝；已死亡则允许新管理器接管 */
        if (kill(manager_pid, 0) == 0 || errno == EPERM)
            return -1;
        fprintf(stdout, "[ARBITER] Previous manager (PID %d) gone; adopting %d\n",
                manager_pid, peer_pid);
    }
    manager_pid = peer_pid;
    fprintf(stdout, "[ARBITER] System Manager registered (PID %d)\n", peer_pid);
    return 0;
}

/*
 * 用户授权：仅系统管理器可调用，指定某进程可达到的最高权限级。
 * 非 manager 的请求一律拒绝（防止自授权提权）。
 */
static int authorize_process(pid_t target_pid, perm_level_t cap, pid_t peer_pid)
{
    if (peer_pid != manager_pid) {
        fprintf(stdout, "[ARBITER] Auth rejected: caller %d is not manager (%d)\n",
                peer_pid, manager_pid);
        return -1;
    }
    if (cap > PERM_LEVEL_BOOTLOADER || cap < PERM_LEVEL_READONLY)
        return -1;

    int idx = find_process(target_pid);
    if (idx < 0) {
        idx = register_process(target_pid, PERM_LEVEL_USER);
        if (idx < 0)
            return -1;
    }

    proc_table[idx].user_authorized_cap = cap;
    fprintf(stdout, "[ARBITER] User authorized PID %d up to level %d (%s)\n",
            target_pid, cap, level_name(cap));
    return 0;
}

static int is_signed_by_official(pid_t pid)
{
    for (int i = 0; i < proc_count; i++)
        if (proc_table[i].pid == pid)
            return proc_table[i].is_signed && !proc_table[i].is_self_signed;
    return 0;
}

static perm_response_t handle_request(const perm_request_t *req, pid_t peer_pid)
{
    perm_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.request_id = req->request_id;

    switch (req->type) {
        case REQ_CHECK_PERM: {
            /* 只能查询自己（或管理器代查），禁止探测其他进程的权限 */
            if (peer_pid != req->pid && peer_pid != manager_pid) {
                resp.status = -1;
                snprintf(resp.reason, sizeof(resp.reason),
                         "denied (may only query self, PID %d)", peer_pid);
                break;
            }
            int result = check_permission(req->pid, req->target_level);
            if (result == 0) {
                resp.status = 0;
                snprintf(resp.reason, sizeof(resp.reason),
                         "granted (PID %d has level %d >= %d)",
                         req->pid, get_process_level(req->pid), req->target_level);
            } else {
                resp.status = -1;
                snprintf(resp.reason, sizeof(resp.reason),
                         "denied (PID %d level %d < %d)",
                         req->pid, get_process_level(req->pid), req->target_level);
            }
            break;
        }
        case REQ_GET_LEVEL:
            if (peer_pid != req->pid && peer_pid != manager_pid) {
                resp.status = -1;
                snprintf(resp.reason, sizeof(resp.reason),
                         "denied (may only query self, PID %d)", peer_pid);
                break;
            }
            resp.status = 0;
            resp.current_level = get_process_level(req->pid);
            snprintf(resp.reason, sizeof(resp.reason), "current level: %d",
                     resp.current_level);
            break;

        case REQ_SET_LEVEL: {
            int result = set_process_level(req->pid, req->target_level, peer_pid);
            if (result == 0) {
                resp.status = 0;
                resp.current_level = req->target_level;
                snprintf(resp.reason, sizeof(resp.reason),
                         "level set to %d (%s)", req->target_level,
                         level_name(req->target_level));
            } else {
                resp.status = -1;
                snprintf(resp.reason, sizeof(resp.reason),
                         "failed to set level to %d", req->target_level);
            }
            break;
        }
        case REQ_REGISTER_MANAGER: {
            int result = register_manager(peer_pid);
            resp.status = result == 0 ? 0 : -1;
            resp.current_level = get_process_level(req->pid);
            snprintf(resp.reason, sizeof(resp.reason),
                     result == 0 ? "manager registered" : "manager already registered");
            break;
        }
        case REQ_AUTHORIZE: {
            int result = authorize_process(req->pid, req->target_level, peer_pid);
            resp.status = result == 0 ? 0 : -1;
            snprintf(resp.reason, sizeof(resp.reason),
                     result == 0 ? "process authorized up to level %d" : "authorization denied",
                     req->target_level);
            break;
        }
        default:
            resp.status = -1;
            snprintf(resp.reason, sizeof(resp.reason), "unknown request type");
            break;
    }

    return resp;
}

static int setup_ipc_socket(void)
{
    struct sockaddr_un addr;

    unlink(IPC_SOCK_PATH);

    int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock < 0) {
        fprintf(stderr, "[ARBITER] Socket creation failed: %s\n", strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[ARBITER] Bind failed: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    chmod(IPC_SOCK_PATH, 0660);

    if (listen(sock, 5) < 0) {
        fprintf(stderr, "[ARBITER] Listen failed: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    return sock;
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "ServEcosys Permission Arbiter v%s\n", ARBITER_VERSION);
    fprintf(stdout, "Running in sys_dom_t security domain\n\n");

    register_process(getpid(), PERM_LEVEL_SELINUX);

    int sock = setup_ipc_socket();
    if (sock < 0)
        return 1;

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    fprintf(stdout, "[ARBITER] Listening on %s (PID: %d)\n", IPC_SOCK_PATH, getpid());
    fprintf(stdout, "[ARBITER] Permission levels 0-11 active\n");
    fprintf(stdout, "[ARBITER] User sovereign levels: 8 (SELinux), 11 (Bootloader)\n\n");

    while (running) {
        struct sockaddr_un client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client = accept(sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client < 0) {
            if (errno == EINTR) continue;
            break;
        }

        perm_request_t req;
        ssize_t n = read(client, &req, sizeof(req));
        if (n > 0) {
            pid_t peer_pid = get_peer_pid(client);

            perm_response_t resp = handle_request(&req, peer_pid);
            write(client, &resp, sizeof(resp));

            fprintf(stdout, "[ARBITER] Request #%d from PID %d (peer %d): %s\n",
                    req.request_id, req.pid, peer_pid, resp.reason);
        }

        close(client);
    }

    close(sock);
    unlink(IPC_SOCK_PATH);
    unlink(PID_FILE);

    fprintf(stdout, "[ARBITER] Shutdown complete\n");
    return 0;
}
