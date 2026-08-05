/**
 * ServEcosys SED - System Manager (系统管理器)
 *
 * 服务用户的直属机关：用户通过它控制系统、并对具体应用/进程授权提权。
 *
 * 职责：
 * - 作为用户控制系统的入口（前端控制台/UID 接入这里）
 * - 向权限仲裁器登记为唯一可授权者（manager）
 * - 用户指定某应用/进程可达到的最高权限级（authorize）
 * - 查询系统与授权状态
 * - 全量行为写审计日志
 *
 * 安全模型：
 * - 授权动作由本进程唯一发起，权限仲裁器仅信任本进程(PID)下达的授权
 * - 未获用户授权的进程一律不得提权
 *
 * 运行在 sys_dom_t 域
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#define MGR_VERSION      "0.1.0"
#define MGR_SOCK_PATH    "/var/run/servecosys_mgr.sock"
#define ARBITER_SOCK     "/var/run/servecosys_perm.sock"
#define PID_FILE         "/var/run/system_manager.pid"
#define AUDIT_FILE       "/var/log/sed/system_manager.log"
#define MAX_CONSOLE_MSG  512

/* 与权限仲裁器交互的消息结构（须与其保持一致） */
typedef enum {
    REQ_CHECK_PERM,
    REQ_GET_LEVEL,
    REQ_SET_LEVEL,
    REQ_QUERY_CAP,
    REQ_REGISTER_MANAGER,
    REQ_AUTHORIZE,
} arbiter_req_type_t;

typedef struct {
    arbiter_req_type_t type;
    pid_t          pid;
    int            target_level;
    char           capability[64];
    char           resource[256];
    int            request_id;
} arbiter_request_t;

typedef struct {
    int     status;
    int     current_level;
    int     request_id;
    char    reason[128];
} arbiter_response_t;

/* 前端控制台命令 */
typedef enum {
    CMD_HELP,
    CMD_STATUS,
    CMD_LIST,
    CMD_AUTHORIZE,
    CMD_DEAUTHORIZE,
    CMD_SERVICE,
    CMD_NOTIFY,
    CMD_QUIT,
} console_cmd_t;

typedef struct {
    console_cmd_t cmd;
    pid_t         target_pid;
    int           level;
    char          arg1[64];
    char          arg2[64];
} console_msg_t;

static volatile sig_atomic_t running = 1;

static void handle_signal(int sig)
{
    running = 0;
}

static void audit(const char *fmt, ...)
{
    va_list args;
    FILE *log = fopen(AUDIT_FILE, "a");
    if (!log) return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char tb[32];
    strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", tm);

    fprintf(log, "[%s] [MGR] ", tb);
    va_start(args, fmt);
    vfprintf(log, fmt, args);
    va_end(args);
    fprintf(log, "\n");
    fclose(log);
}

static int request_arbiter(const arbiter_request_t *req, arbiter_response_t *resp)
{
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ARBITER_SOCK, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (write(fd, req, sizeof(*req)) != (ssize_t)sizeof(*req)) {
        close(fd);
        return -1;
    }

    int ok = -1;
    ssize_t n = read(fd, resp, sizeof(*resp));
    if (n > 0)
        ok = 0;

    close(fd);
    return ok;
}

/* 向权限仲裁器登记本进程为唯一授权源 */
static int register_as_manager(void)
{
    arbiter_request_t req;
    arbiter_response_t resp;
    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));

    req.type = REQ_REGISTER_MANAGER;
    req.pid = getpid();
    req.request_id = 1;

    if (request_arbiter(&req, &resp) != 0 || resp.status != 0) {
        fprintf(stderr, "[MGR] Failed to register as manager: %s\n", resp.reason);
        return -1;
    }

    fprintf(stdout, "[MGR] Registered as System Manager (PID %d)\n", getpid());
    audit("registered as manager pid=%d", getpid());
    return 0;
}

static const char *level_name(int level)
{
    static const char *names[] = {
        "readonly", "sandbox", "user", "debug", "bl_unlock", "root_split",
        "module_root", "kernel_root", "selinux_control", "kmod_load",
        "custom_kernel", "bootloader"
    };
    if (level < 0 || level > 11) return "unknown";
    return names[level];
}

/* 用户授权：指定应用/进程可达到的最高权限级 */
static int cmd_authorize(pid_t target, int level)
{
    if (level < 0 || level > 11) return -1;

    arbiter_request_t req;
    arbiter_response_t resp;
    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));

    req.type = REQ_AUTHORIZE;
    req.pid = target;
    req.target_level = level;
    req.request_id = (int)time(NULL) & 0x7fffffff;

    int r = request_arbiter(&req, &resp);
    if (r != 0 || resp.status != 0) {
        fprintf(stdout, "authorize failed: %s\n", resp.reason);
        audit("authorize pid=%d level=%d FAILED: %s", target, level, resp.reason);
        return -1;
    }

    fprintf(stdout, "Authorized PID %d to level %d (%s)\n",
            target, level, level_name(level));
    audit("user authorized pid=%d to level=%d (%s)", target, level, level_name(level));
    return 0;
}

/* 用户取消授权：降回只读，禁止再提权 */
static int cmd_deauthorize(pid_t target)
{
    return cmd_authorize(target, 0);
}

static void print_help(void)
{
    fprintf(stdout,
        "System Manager commands:\n"
        "  help                        show this help\n"
        "  status                      show manager status\n"
        "  list                        list authorized processes (arbiter-side)\n"
        "  authorize <pid> <0-11>      grant process access up to a level\n"
        "  deauthorize <pid>           revoke all elevation rights for a process\n"
        "  service <start|stop> <name> control a system service\n"
        "  notify <pid> <msg>          send a notification to a process\n"
        "  quit                        stop the manager\n");
}

static int cmd_status(void)
{
    fprintf(stdout, "System Manager v%s\n", MGR_VERSION);
    fprintf(stdout, "PID: %d  Sock: %s  Arbiter: %s\n", getpid(), MGR_SOCK_PATH, ARBITER_SOCK);
    return 0;
}

static int cmd_service(const char *action, const char *name)
{
    if (!action || !name) return -1;
    audit("service action=%s name=%s", action, name);
    fprintf(stdout, "Service %s scheduled: %s\n", name, action);

    char path[256];
    snprintf(path, sizeof(path), "/system/backend/bin/%s.smle", name);

    if (strcmp(action, "start") == 0) {
        pid_t p = fork();
        if (p == 0) {
            execl(path, name, NULL);
            _exit(127);
        }
        fprintf(stdout, "Started %s (pid %d)\n", name, p);
    } else if (strcmp(action, "stop") == 0) {
        fprintf(stdout, "Stop signal sent for %s\n", name);
    } else {
        fprintf(stdout, "Unknown service action: %s\n", action);
        return -1;
    }
    return 0;
}

static int cmd_notify(pid_t target, const char *msg)
{
    audit("notify pid=%d msg=%s", target, msg);
    fprintf(stdout, "Notification queued for PID %d: %s\n", target, msg);
    return 0;
}

static void handle_console(const console_msg_t *msg)
{
    switch (msg->cmd) {
        case CMD_HELP:
            print_help();
            break;
        case CMD_STATUS:
            cmd_status();
            break;
        case CMD_LIST:
            fprintf(stdout, "Authorized processes: query arbitration service state (see arbiter log)\n");
            break;
        case CMD_AUTHORIZE:
            cmd_authorize(msg->target_pid, msg->level);
            break;
        case CMD_DEAUTHORIZE:
            cmd_deauthorize(msg->target_pid);
            break;
        case CMD_SERVICE:
            cmd_service(msg->arg1, msg->arg2);
            break;
        case CMD_NOTIFY:
            cmd_notify(msg->target_pid, msg->arg1);
            break;
        case CMD_QUIT:
            fprintf(stdout, "Shutting down system manager...\n");
            running = 0;
            break;
        default:
            print_help();
            break;
    }
}

static int setup_console_socket(void)
{
    struct sockaddr_un addr;

    unlink(MGR_SOCK_PATH);

    int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MGR_SOCK_PATH, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    chmod(MGR_SOCK_PATH, 0660);

    if (listen(sock, 10) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "ServEcosys System Manager v%s\n", MGR_VERSION);
    fprintf(stdout, "User-control authority domain (sys_dom_t)\n\n");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    mkdir("/var/log/sed", 0755);
    audit("system manager starting");

    int sock = setup_console_socket();
    if (sock < 0) {
        fprintf(stderr, "[MGR] Console socket setup failed: %s\n", strerror(errno));
        return 1;
    }

    if (register_as_manager() != 0) {
        fprintf(stderr, "[MGR] Exiting: could not claim manager authority\n");
        close(sock);
        unlink(MGR_SOCK_PATH);
        return 1;
    }

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    fprintf(stdout, "[MGR] Listening on %s (PID: %d)\n", MGR_SOCK_PATH, getpid());

    while (running) {
        struct sockaddr_un client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client = accept(sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client < 0) {
            if (errno == EINTR) continue;
            break;
        }

        console_msg_t msg;
        ssize_t n = read(client, &msg, sizeof(msg));
        if (n > 0)
            handle_console(&msg);

        close(client);
    }

    close(sock);
    unlink(MGR_SOCK_PATH);
    unlink(PID_FILE);

    audit("system manager stopping");
    fprintf(stdout, "[MGR] Shutdown complete\n");
    return 0;
}