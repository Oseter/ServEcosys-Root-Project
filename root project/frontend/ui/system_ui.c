/**
 * 概念OS (Concept OS) - UID System UI
 *   - 顶部导航栏（常驻）：品牌 / 域 / 实时权限级 / SELinux / 时钟 / 通知数
 *   - 下方交互终端：可输入命令，上下键历史，基础命令集
 *
 * 概念OS 是 ServEcosys 系操作系统标准的概念化呈现：底层一切技术标准
 * 由 ServEcosys 定义（socket 路径、uid_dom_t 域、0-11 权限级、SELinux 强制），
 * 概念OS 以可理解、可交互的形态把整套标准的核心概念表达给使用者。
 *
 * 运行在 uid_dom_t 域
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>

#define SUI_VERSION          "0.1.0"
#define OS_BRAND             "\xe6\xa6\x82\xe5\xbf\xb5OS"        /* 概念OS */
#define OS_BRAND_EN          "Concept OS"
#define PLATFORM             "ServEcosys"
#define PERM_SOCK_PATH       "/var/run/servecosys_perm.sock"
#define PID_FILE             "/var/run/system_ui.pid"
#define MAX_NOTIFICATIONS    32
#define MAX_HISTORY          64
#define MAX_LINE             1024
#define PROMPT               "conceptos> "

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

static const char *level_name(int level)
{
    switch (level) {
        case 0:  return "readonly";
        case 1:  return "sandbox";
        case 2:  return "user";
        case 3:  return "debug";
        case 4:  return "bl_unlock";
        case 5:  return "root_split";
        case 6:  return "module_root";
        case 7:  return "kernel_root";
        case 8:  return "selinux_control";
        case 9:  return "kmod_load";
        case 10: return "custom_kernel";
        case 11: return "bootloader";
        default: return "unknown";
    }
}

typedef struct {
    int     id;
    char    app_name[64];
    char    title[128];
    char    message[256];
    time_t  timestamp;
    int     read;
    int     priority;  /* 0=low, 1=normal, 2=high, 3=critical */
} notification_t;

static notification_t notifications[MAX_NOTIFICATIONS];
static int notification_count = 0;
static int next_notif_id = 1;
static volatile sig_atomic_t running = 1;

static char history[MAX_HISTORY][MAX_LINE];
static int history_count = 0;
static int history_pos = 0;

static void sig_int(int sig) { (void)sig; }

static int add_notification(const char *app, const char *title,
                             const char *msg, int priority)
{
    if (notification_count >= MAX_NOTIFICATIONS) {
        memmove(&notifications[0], &notifications[1],
                (MAX_NOTIFICATIONS - 1) * sizeof(notification_t));
        notification_count--;
    }

    notification_t *n = &notifications[notification_count];
    n->id = next_notif_id++;
    strncpy(n->app_name, app, sizeof(n->app_name) - 1);
    n->app_name[sizeof(n->app_name) - 1] = 0;
    strncpy(n->title, title, sizeof(n->title) - 1);
    n->title[sizeof(n->title) - 1] = 0;
    strncpy(n->message, msg, sizeof(n->message) - 1);
    n->message[sizeof(n->message) - 1] = 0;
    n->timestamp = time(NULL);
    n->read = 0;
    n->priority = priority;
    notification_count++;
    return n->id;
}

static void dismiss_notification(int id)
{
    for (int i = 0; i < notification_count; i++) {
        if (notifications[i].id == id) {
            memmove(&notifications[i], &notifications[i + 1],
                    (notification_count - i - 1) * sizeof(notification_t));
            notification_count--;
            break;
        }
    }
}

static void clear_all_notifications(void)
{
    notification_count = 0;
}

static char *format_time(void)
{
    static char buf[32];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return buf;
}

/* 向权限仲裁器查询自身权限级；失败返回 -1 */
static int query_level(pid_t me)
{
    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, PERM_SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    struct {
        int type;
        pid_t pid;
        int target_level;
        char capability[64];
        char resource[256];
        int request_id;
    } req;
    struct {
        int status;
        int current_level;
        int request_id;
        char reason[128];
    } resp;

    memset(&req, 0, sizeof(req));
    req.type = 1;              /* REQ_GET_LEVEL */
    req.pid = me;
    req.request_id = 1;

    if (write(fd, &req, sizeof(req)) != (ssize_t)sizeof(req)) {
        close(fd);
        return -1;
    }
    memset(&resp, 0, sizeof(resp));
    if (read(fd, &resp, sizeof(resp)) != (ssize_t)sizeof(resp)) {
        close(fd);
        return -1;
    }
    close(fd);
    return resp.current_level;
}

/* 读取 SELinux 状态：1=Enforcing, 0=Permissive, -1=未知 */
static int query_selinux(void)
{
    int fd = open("/sys/fs/selinux/enforce", O_RDONLY);
    if (fd < 0)
        return -1;
    char buf[4] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return -1;
    return buf[0] == '1' ? 1 : 0;
}

static int term_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80;
}

/* 顶部导航栏：单行常驻，反白显示 */
static void draw_topbar(void)
{
    int width = term_width();
    int my_level = query_level(getpid());
    int selinux = query_selinux();

    char left[64];
    snprintf(left, sizeof(left), " %s | %s", OS_BRAND, OS_BRAND_EN);

    char right[192];
    char lvl[32];
    if (my_level >= 0)
        snprintf(lvl, sizeof(lvl), "perm:%s(%d)", level_name(my_level), my_level);
    else
        snprintf(lvl, sizeof(lvl), "perm:n/a");

    snprintf(right, sizeof(right),
             " dom:uid_dom_t | %s | selinux:%s | %s | N:%d ",
             lvl,
             selinux < 0 ? "n/a" : (selinux ? "enforcing" : "permissive"),
             format_time(), notification_count);

    char line[512];
    int llen = (int)strlen(left);
    int rlen = (int)strlen(right);
    int pad = width - llen - rlen;
    if (pad < 1)
        pad = 1;
    snprintf(line, sizeof(line), "%s%*s%s", left, pad, "", right);
    printf("\033[30;47m%-*.*s\033[0m\n", width, (int)strlen(line), line);
    fflush(stdout);
}

static void print_notifications(void)
{
    if (notification_count == 0) {
        printf("  (no notifications)\n");
        return;
    }
    printf("  Notifications (%d):\n", notification_count);
    for (int i = 0; i < notification_count; i++) {
        notification_t *n = &notifications[i];
        char time_buf[32];
        struct tm *tm = localtime(&n->timestamp);
        strftime(time_buf, sizeof(time_buf), "%H:%M", tm);
        const char *tag =
            n->priority >= 3 ? "[CRITICAL] " :
            n->priority >= 2 ? "[HIGH] " : "";
        printf("  %-4d %s%s [%s] %s\n", n->id, tag, n->app_name, time_buf, n->title);
        printf("        %s\n", n->message);
    }
}

static void cmd_help(void)
{
    printf("  概念OS commands:\n");
    printf("    help              show this help\n");
    printf("    status            system / domain / permission status\n");
    printf("    level             show my permission level\n");
    printf("    notifications     list notifications\n");
    printf("    dismiss <id>      dismiss a notification\n");
    printf("    clear             clear all notifications\n");
    printf("    clear_screen      clear the terminal\n");
    printf("    about             %s info\n", OS_BRAND);
    printf("    exit / quit       leave the UI\n");
}

static void cmd_status(void)
{
    int my_level = query_level(getpid());
    int selinux = query_selinux();
    char uptime_buf[32];
    struct tm *tm = localtime(&(time_t){time(NULL)});
    strftime(uptime_buf, sizeof(uptime_buf), "%Y-%m-%d %H:%M:%S", tm);

    printf("  %s %s (%s)\n", OS_BRAND, SUI_VERSION, OS_BRAND_EN);
    printf("  概念OS = %s 系标准概念化呈现\n", PLATFORM);
    printf("  domain      : uid_dom_t\n");
    printf("  perm level  : %s (%d)\n",
           my_level >= 0 ? level_name(my_level) : "n/a", my_level);
    printf("  selinux     : %s\n",
           selinux < 0 ? "n/a" : (selinux ? "enforcing" : "permissive"));
    printf("  now         : %s\n", uptime_buf);
    printf("  notifications: %d\n", notification_count);
    printf("  arber sock  : %s\n", PERM_SOCK_PATH);
}

static void cmd_about(void)
{
    printf("\n");
    printf("      %s  (Concept OS) v%s\n", OS_BRAND, SUI_VERSION);
    printf("      ==============================\n");
    printf("      概念OS：ServEcosys 系操作系统标准的概念化呈现\n");
    printf("      顶层为概念形态 UI（顶部导航栏 + 交互终端），\n");
    printf("      底层实现遵循 ServEcosys 标准（域 / 权限级 / SELinux）。\n");
    printf("\n");
}

static void run_command(char *line)
{
    char *save = NULL;
    char *cmd = strtok_r(line, " \t", &save);
    if (!cmd || *cmd == '\0')
        return;

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "status") == 0) {
        cmd_status();
    } else if (strcmp(cmd, "level") == 0) {
        int lv = query_level(getpid());
        if (lv >= 0)
            printf("  current perm level: %s (%d)\n", level_name(lv), lv);
        else
            printf("  cannot reach permission arbiter (is SED up?)\n");
    } else if (strcmp(cmd, "notifications") == 0 || strcmp(cmd, "noti") == 0) {
        print_notifications();
    } else if (strcmp(cmd, "dismiss") == 0) {
        char *idstr = strtok_r(NULL, " \t", &save);
        if (!idstr) {
            printf("  usage: dismiss <id>\n");
        } else {
            int id = atoi(idstr);
            dismiss_notification(id);
            printf("  dismissed %d\n", id);
        }
    } else if (strcmp(cmd, "clear") == 0) {
        clear_all_notifications();
        printf("  all notifications cleared\n");
    } else if (strcmp(cmd, "clear_screen") == 0 || strcmp(cmd, "cls") == 0) {
        system("clear 2>/dev/null || cls 2>/dev/null || true");
    } else if (strcmp(cmd, "about") == 0) {
        cmd_about();
    } else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        printf("  good bye\n");
        running = 0;
    } else {
        printf("  unknown command: %s  (try 'help')\n", cmd);
    }
}

/* 行编辑：支持退格、左右移动、上下键历史、回车执行 */
static int read_line(char *buf, size_t size, struct termios *saved)
{
    int len = 0;
    int pos = 0;
    int done = 0;
    history_pos = history_count;

    while (!done) {
        unsigned char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n != 1)
            continue;
        if (c == 0x1b) {                       /* ESC 序列 */
            unsigned char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;
            if (seq[0] == '[') {
                if (seq[1] == 'A') {           /* Up    */
                    if (history_pos > 0) {
                        history_pos--;
                        strncpy(buf, history[history_pos], size - 1);
                        buf[size - 1] = 0;
                        len = pos = (int)strlen(buf);
                        printf("\r\x1b[2K%s%s", PROMPT, buf);
                        fflush(stdout);
                    }
                } else if (seq[1] == 'B') {    /* Down  */
                    if (history_pos < history_count) {
                        history_pos++;
                        if (history_pos < history_count) {
                            strncpy(buf, history[history_pos], size - 1);
                            buf[size - 1] = 0;
                            len = pos = (int)strlen(buf);
                        } else {
                            buf[0] = 0;
                            len = pos = 0;
                        }
                        printf("\r\x1b[2K%s%s", PROMPT, buf);
                        fflush(stdout);
                    }
                } else if (seq[1] == 'C') {    /* Right */
                    if (pos < len) {
                        pos++;
                        printf("\x1b[1C");
                        fflush(stdout);
                    }
                } else if (seq[1] == 'D') {    /* Left  */
                    if (pos > 0) {
                        pos--;
                        printf("\x1b[1D");
                        fflush(stdout);
                    }
                }
            }
            continue;
        }
        if (c == '\r' || c == '\n') {
            printf("\n");
            done = 1;
            break;
        }
        if (c == 0x7f || c == 8) {             /* Backspace */
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, (size_t)(len - pos) + 1);
                pos--;
                len--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        if (c == 0x03) {                       /* Ctrl-C */
            printf("^C\n");
            buf[0] = 0;
            len = pos = 0;
            done = 1;
            break;
        }
        if (c >= 0x20 && c < 0x7f && len < (int)size - 1) {
            memmove(buf + pos + 1, buf + pos, (size_t)(len - pos) + 1);
            buf[pos] = (char)c;
            pos++;
            len++;
            printf("\r\x1b[2K%s%s", PROMPT, buf);
            fflush(stdout);
        }
    }

    buf[len] = 0;

    /* 压入历史（去重最近一条） */
    if (len > 0 && (history_count == 0 ||
        strcmp(history[history_count - 1], buf) != 0)) {
        if (history_count >= MAX_HISTORY) {
            memmove(&history[0], &history[1],
                    (MAX_HISTORY - 1) * sizeof(history[0]));
            history_count--;
        }
        strncpy(history[history_count], buf, MAX_LINE - 1);
        history[history_count][MAX_LINE - 1] = 0;
        history_count++;
    }
    (void)saved;
    return len;
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    struct termios tty, saved;
    int has_tty = 0;

    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &saved) == 0) {
            tty = saved;
            tty.c_lflag &= ~(ICANON | ECHO);
            tty.c_cc[VMIN] = 0;
            tty.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &tty);
            has_tty = 1;
        }
    }

    signal(SIGINT, sig_int);

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    add_notification("System", "Welcome to " OS_BRAND,
                     "Concept OS v0.1.0 ready (ServEcosys 标准)", 1);
    add_notification("System", "SELinux Enforcing",
                     "Security policy loaded successfully", 1);

    draw_topbar();
    printf("\n  %s %s  -  顶部导航栏 + 交互终端\n", OS_BRAND, SUI_VERSION);
    printf("  概念OS = ServEcosys 系标准概念化呈现\n");
    printf("  type 'help' for commands, 'exit' to quit\n\n");

    char line[MAX_LINE];
    while (running) {
        if (has_tty) {
            draw_topbar();
            printf("%s", PROMPT);
            fflush(stdout);
            if (read_line(line, sizeof(line), &saved) < 0)
                break;
            run_command(line);
            if (running)
                printf("\n");
        } else {
            /* 非交互（经 bootstrap 以后台方式启动）：保持常驻，
             * 供调度器通过 pidfile 存活检测，避免对 EOF 的忙转。 */
            sleep(5);
        }
    }

    if (has_tty)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    unlink(PID_FILE);
    printf("[SUI] shutdown complete\n");
    return 0;
}
