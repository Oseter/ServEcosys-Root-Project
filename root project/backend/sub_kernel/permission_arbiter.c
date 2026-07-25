/**
 * ServEcosys SED - Permission Adjudication Service
 *
 * 职责：
 * - 接收来自 UID 的权限请求
 * - 根据权限阶梯（0-11）裁决请求
 * - 维护进程权限级别标记
 * - 向 UID 返回裁决结果
 *
 * 运行在 sys_dom_t 域
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

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

typedef struct {
    pid_t           pid;
    perm_level_t    level;
    char            selinux_context[256];
    int             is_signed;
    int             is_self_signed;
} proc_cred_t;

static proc_cred_t proc_table[MAX_REQUESTS];
static int proc_count = 0;
static volatile sig_atomic_t running = 1;

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
    snprintf(proc_table[proc_count].selinux_context,
             sizeof(proc_table[proc_count].selinux_context),
             "u:r:app_sandbox_t:s0");

    return proc_count++;
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

static int set_process_level(pid_t pid, perm_level_t new_level)
{
    if (new_level > PERM_LEVEL_BOOTLOADER || new_level < PERM_LEVEL_READONLY)
        return -1;

    switch (new_level) {
        case PERM_LEVEL_SELINUX:
        case PERM_LEVEL_BOOTLOADER:
            break;
        default:
            if (new_level > PERM_LEVEL_DEBUG && new_level < PERM_LEVEL_SELINUX) {
                if (!is_signed_by_official(pid))
                    return -1;
            }
            break;
    }

    int idx = register_process(pid, new_level);
    if (idx >= 0) {
        proc_table[idx].level = new_level;
        fprintf(stdout, "[ARBITER] PID %d level set to %s (%d)\n",
                pid, level_name(new_level), new_level);
        return 0;
    }

    return -1;
}

static int is_signed_by_official(pid_t pid)
{
    for (int i = 0; i < proc_count; i++)
        if (proc_table[i].pid == pid)
            return proc_table[i].is_signed && !proc_table[i].is_self_signed;
    return 0;
}

static perm_response_t handle_request(const perm_request_t *req)
{
    perm_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.request_id = req->request_id;

    switch (req->type) {
        case REQ_CHECK_PERM: {
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
            resp.status = 0;
            resp.current_level = get_process_level(req->pid);
            snprintf(resp.reason, sizeof(resp.reason), "current level: %d",
                     resp.current_level);
            break;

        case REQ_SET_LEVEL: {
            int result = set_process_level(req->pid, req->target_level);
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
            register_process(req.pid, PERM_LEVEL_USER);

            perm_response_t resp = handle_request(&req);
            write(client, &resp, sizeof(resp));

            fprintf(stdout, "[ARBITER] Request #%d from PID %d: %s\n",
                    req.request_id, req.pid, resp.reason);
        }

        close(client);
    }

    close(sock);
    unlink(IPC_SOCK_PATH);
    unlink(PID_FILE);

    fprintf(stdout, "[ARBITER] Shutdown complete\n");
    return 0;
}
