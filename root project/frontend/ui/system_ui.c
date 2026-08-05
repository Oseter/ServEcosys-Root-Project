/**
 * ServEcosys UID - System UI (Status Bar & Notifications)
 *
 * 职责：
 * - 状态栏渲染（时间、电池、网络、音量）
 * - 通知管理（显示/关闭/历史）
 * - 快速设置面板
 * - 锁屏界面
 *
 * 运行在 uid_dom_t 域
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <signal.h>

#define SUI_VERSION          "0.1.0"
#define IPC_SOCK_PATH        "/var/run/servecosys_systemui.sock"
#define PID_FILE             "/var/run/system_ui.pid"
#define MAX_NOTIFICATIONS    32

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

typedef struct {
    int battery_pct;
    int wifi_strength;
    int cellular_strength;
    int volume_pct;
    int bluetooth_active;
    int do_not_disturb;
} system_status_t;

static system_status_t status;
static volatile sig_atomic_t running = 1;

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
            memmove(&notifications[i], &notifications[i+1],
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

static void update_status(void)
{
    status.battery_pct = 85;
    status.wifi_strength = 4;
    status.volume_pct = 70;
    status.bluetooth_active = 0;
    status.do_not_disturb = 0;
}

static char *format_time(void)
{
    static char buf[32];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return buf;
}

static void draw_status_bar(void)
{
    char line[256];
    int pos = 0;

    pos += snprintf(line + pos, sizeof(line) - pos,
                    "[%s]", format_time());

    pos += snprintf(line + pos, sizeof(line) - pos,
                    " BAT:%d%%", status.battery_pct);

    pos += snprintf(line + pos, sizeof(line) - pos,
                    " VOL:%d%%", status.volume_pct);

    if (status.wifi_strength > 0)
        pos += snprintf(line + pos, sizeof(line) - pos, " WIFI:%d", status.wifi_strength);

    if (status.do_not_disturb)
        pos += snprintf(line + pos, sizeof(line) - pos, " DND");

    if (notification_count > 0)
        pos += snprintf(line + pos, sizeof(line) - pos, " N:%d", notification_count);

    printf("\033[30;47m%-80s\033[0m\n", line);
}

static void print_notifications(void)
{
    if (notification_count == 0) {
        printf("\n  No notifications\n");
        return;
    }

    printf("\n  Notifications (%d):\n", notification_count);
    for (int i = 0; i < notification_count; i++) {
        notification_t *n = &notifications[i];
        char time_buf[32];
        struct tm *tm = localtime(&n->timestamp);
        strftime(time_buf, sizeof(time_buf), "%H:%M", tm);

        const char *priority_tag =
            n->priority >= 3 ? "[CRITICAL] " :
            n->priority >= 2 ? "[HIGH] " :
            n->priority >= 1 ? " " : " ";

        printf("  %s%s [%s] %s\n", priority_tag, n->app_name, time_buf, n->title);
        printf("    %s\n", n->message);
    }
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "ServEcosys System UI v%s\n", SUI_VERSION);
    fprintf(stdout, "Running in uid_dom_t domain\n\n");

    update_status();

    add_notification("System", "Welcome to ServEcosys",
                     "Your system is ready. v0.1.0 'Genesis'", 1);
    add_notification("System", "SELinux Enforcing",
                     "Security policy loaded successfully", 1);

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    fprintf(stdout, "[SUI] Running (PID: %d)\n", getpid());

    int tick = 0;
    while (running) {
        system("clear 2>/dev/null || cls 2>/dev/null || true");
        printf("\n");
        draw_status_bar();

        printf("\n  ServEcosys System UI\n");
        printf("  %-20s %s\n", "Version:", SUI_VERSION);
        printf("  %-20s %d%%\n", "Battery:", status.battery_pct);
        printf("  %-20s %s\n", "Uptime:", format_time());

        print_notifications();
        printf("\n  [Running for %d seconds]\n", ++tick);

        sleep(2);
    }

    unlink(PID_FILE);
    fprintf(stdout, "[SUI] Shutdown complete\n");
    return 0;
}
