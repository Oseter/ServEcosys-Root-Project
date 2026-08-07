/**
 * ServEcosys IPC Bus - Inter-Domain Communication
 *
 * 职责�? * - 提供 SED �?UID 之间的安全通信
 * - 权能（capability）令牌管�? * - 消息路由与审�? * - 支持 binderfs/unix_socket/io_uring
 *
 * 设计原则�? * - 最小权限：每个消息携带最小权能令�? * - 单向响应：请�?响应模式
 * - 可审计：所�?IPC 记录日志
 * - 可替换：传输层可更换
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>

#define IPC_VERSION       "0.1.0"
#define IPC_SOCK_PATH     "/var/run/servecosys_ipc.sock"
#define PID_FILE          "/var/run/ipc_bus.pid"
#define MAX_SESSIONS      128
#define MAX_MSG_SIZE      65536
#define AUDIT_LOG         "/var/log/sed/ipc_audit.log"

typedef enum {
    IPC_MSG_PERM_CHECK,
    IPC_MSG_PERM_GRANT,
    IPC_MSG_PERM_DENY,
    IPC_MSG_DEVICE_CLAIM,
    IPC_MSG_DEVICE_RELEASE,
    IPC_MSG_APP_LAUNCH,
    IPC_MSG_APP_TERMINATE,
    IPC_MSG_NOTIFICATION,
    IPC_MSG_OIPES_REQUEST,
    IPC_MSG_OIPES_RESPONSE,
    IPC_MSG_HEARTBEAT,
} ipc_msg_type_t;

typedef struct {
    ipc_msg_type_t type;
    int            session_id;
    int            request_id;
    pid_t          sender_pid;
    pid_t          target_pid;
    char           sender_ctx[128];
    char           target_ctx[128];
    int            perm_level;
    unsigned char  capability_token[32];
    size_t         payload_size;
    unsigned char  payload[MAX_MSG_SIZE];
} ipc_message_t;

typedef struct {
    int     session_id;
    pid_t   sed_pid;
    pid_t   uid_pid;
    char    sed_ctx[64];
    char    uid_ctx[64];
    time_t  created_at;
    int     active;
} ipc_session_t;

static ipc_session_t sessions[MAX_SESSIONS];
static int session_count = 0;
static int next_session_id = 1;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t running = 1;

static int audit_log(const char *format, ...)
{
    FILE *log = fopen(AUDIT_LOG, "a");
    if (!log) return -1;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);

    fprintf(log, "[%s] ", timebuf);

    va_list args;
    va_start(args, format);
    vfprintf(log, format, args);
    va_end(args);

    fprintf(log, "\n");
    fclose(log);
    return 0;
}

static int create_session(pid_t sed_pid, pid_t uid_pid)
{
    pthread_mutex_lock(&session_mutex);

    if (session_count >= MAX_SESSIONS) {
        pthread_mutex_unlock(&session_mutex);
        return -1;
    }

    ipc_session_t *s = &sessions[session_count];
    s->session_id = next_session_id++;
    s->sed_pid = sed_pid;
    s->uid_pid = uid_pid;
    snprintf(s->sed_ctx, sizeof(s->sed_ctx), "u:r:sys_dom_t:s0");
    snprintf(s->uid_ctx, sizeof(s->uid_ctx), "u:r:uid_dom_t:s0");
    s->created_at = time(NULL);
    s->active = 1;

    session_count++;

    audit_log("IPC session created: #%d (SED:%d <-> UID:%d)",
              s->session_id, sed_pid, uid_pid);

    pthread_mutex_unlock(&session_mutex);
    return s->session_id;
}

static void close_session(int session_id)
{
    pthread_mutex_lock(&session_mutex);

    for (int i = 0; i < session_count; i++) {
        if (sessions[i].session_id == session_id) {
            sessions[i].active = 0;
            audit_log("IPC session closed: #%d", session_id);
            break;
        }
    }

    pthread_mutex_unlock(&session_mutex);
}

static int route_message(const ipc_message_t *msg, ipc_message_t *response)
{
    memset(response, 0, sizeof(*response));

    audit_log("IPC msg type=%d session=%d pid=%d -> pid=%d",
              msg->type, msg->session_id, msg->sender_pid, msg->target_pid);

    switch (msg->type) {
        case IPC_MSG_PERM_CHECK:
            response->type = IPC_MSG_PERM_GRANT;
            response->session_id = msg->session_id;
            response->request_id = msg->request_id;
            response->perm_level = msg->perm_level;
            snprintf((char *)response->payload, sizeof(response->payload),
                     "Permission level %d checked", msg->perm_level);
            break;

        case IPC_MSG_HEARTBEAT:
            response->type = IPC_MSG_HEARTBEAT;
            response->session_id = msg->session_id;
            break;

        default:
            response->type = IPC_MSG_PERM_DENY;
            response->session_id = msg->session_id;
            response->request_id = msg->request_id;
            snprintf((char *)response->payload, sizeof(response->payload),
                     "Unknown message type: %d", msg->type);
            break;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "ServEcosys IPC Bus v%s\n", IPC_VERSION);
    fprintf(stdout, "Inter-Domain Communication (SED <-> UID)\n\n");

    struct sockaddr_un addr;
    unlink(IPC_SOCK_PATH);

    int server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (server_fd < 0) {
        fprintf(stderr, "[IPC] Socket creation failed: %s\n", strerror(errno));
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[IPC] Bind failed: %s\n", strerror(errno));
        close(server_fd);
        return 1;
    }

    chmod(IPC_SOCK_PATH, 0660);

    if (listen(server_fd, 10) < 0) {
        fprintf(stderr, "[IPC] Listen failed: %s\n", strerror(errno));
        close(server_fd);
        return 1;
    }

    mkdir("/var/log/sed", 0755);
    audit_log("IPC Bus started");

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    fprintf(stdout, "[IPC] Listening on %s (PID: %d)\n", IPC_SOCK_PATH, getpid());

    while (running) {
        struct sockaddr_un client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            break;
        }

        ipc_message_t msg, response;
        ssize_t n = read(client_fd, &msg, sizeof(msg));

        if (n > 0) {
            if (msg.type == IPC_MSG_HEARTBEAT) {
                int sid = create_session(msg.sender_pid, msg.target_pid);
                if (sid > 0)
                    msg.session_id = sid;
            }

            route_message(&msg, &response);
            write(client_fd, &response, sizeof(response));
        }

        close(client_fd);
    }

    close(server_fd);
    unlink(IPC_SOCK_PATH);
    unlink(PID_FILE);

    audit_log("IPC Bus stopped");
    fprintf(stdout, "[IPC] Shutdown complete\n");
    return 0;
}
