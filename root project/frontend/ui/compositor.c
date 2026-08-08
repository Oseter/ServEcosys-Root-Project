/**
 * ServEcosys UID - Compositor (Window Manager)
 *
 * 职责：
 * - 窗口合成与管理
 * - 应用窗口布局
 * - 窗口焦点管理
 * - 渲染到显示服务器
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
#include <pthread.h>
#include <signal.h>

#define COMPOSITOR_VERSION  "0.1.0"
#define DISPLAY_IPC         "/var/run/servecosys_display.sock"
#define INPUT_IPC           "/var/run/servecosys_input.sock"
#define PID_FILE            "/var/run/compositor.pid"
#define MAX_WINDOWS         64

typedef struct {
    int  id;
    char title[128];
    int  x, y;
    int  width, height;
    int  visible;
    int  has_focus;
    int  app_pid;
} window_t;

static window_t windows[MAX_WINDOWS];
static int window_count = 0;
static int next_window_id = 0;
static int focus_window = -1;
static int display_fd = -1;
static volatile sig_atomic_t running = 1;

static int connect_ipc(const char *sock_path)
{
    struct sockaddr_un addr;

    int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (fd < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int send_display_cmd(const char *cmd)
{
    if (display_fd < 0) return -1;
    return write(display_fd, cmd, strlen(cmd));
}

static int create_window(const char *title, int x, int y, int w, int h)
{
    if (window_count >= MAX_WINDOWS) return -1;

    window_t *win = &windows[window_count];
    win->id = next_window_id++;
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = 0;
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->visible = 1;
    win->has_focus = 0;
    win->app_pid = 0;

    window_count++;
    return win->id;
}

static void remove_window(int id)
{
    for (int i = 0; i < window_count; i++) {
        if (windows[i].id == id) {
            if (focus_window == id)
                focus_window = -1;

            memmove(&windows[i], &windows[i+1],
                    (window_count - i - 1) * sizeof(window_t));
            window_count--;
            break;
        }
    }
}

static void set_focus(int id)
{
    for (int i = 0; i < window_count; i++)
        windows[i].has_focus = (windows[i].id == id);

    focus_window = id;
}

static void render_all_windows(void)
{
    char cmd[256];

    send_display_cmd("CLEAR 1A1A2E");

    for (int i = 0; i < window_count; i++) {
        window_t *win = &windows[i];
        if (!win->visible) continue;

        unsigned int border_color = win->has_focus ? 0x4A90D9 : 0x404040;
        unsigned int bg_color = win->has_focus ? 0x2A2A3E : 0x1E1E2E;
        unsigned int title_color = 0xCCCCCC;

        snprintf(cmd, sizeof(cmd), "FILL_RECT %d %d %d %d %x",
                 win->x - 1, win->y - 1, win->width + 2, win->height + 2,
                 border_color);
        send_display_cmd(cmd);

        snprintf(cmd, sizeof(cmd), "FILL_RECT %d %d %d %d %x",
                 win->x, win->y, win->width, 24, title_color);
        send_display_cmd(cmd);

        snprintf(cmd, sizeof(cmd), "FILL_RECT %d %d %d %d %x",
                 win->x, win->y + 24, win->width, win->height - 24,
                 bg_color);
        send_display_cmd(cmd);
    }
}

static void init_desktop(void)
{
    fprintf(stdout, "[COMPOSITOR] Initializing desktop...\n");
    create_window("Settings", 50, 50, 400, 300);
    create_window("File Manager", 100, 80, 600, 400);
    create_window("Terminal", 200, 120, 500, 350);

    set_focus(1);
    render_all_windows();
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "ServEcosys Compositor v%s\n", COMPOSITOR_VERSION);
    fprintf(stdout, "Running in uid_dom_t domain\n\n");

    display_fd = connect_ipc(DISPLAY_IPC);
    if (display_fd < 0) {
        fprintf(stderr, "[COMPOSITOR] Display server not available\n");
        fprintf(stderr, "[COMPOSITOR] Starting in headless mode\n");
    } else {
        fprintf(stdout, "[COMPOSITOR] Connected to display server\n");
        init_desktop();
    }

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    fprintf(stdout, "[COMPOSITOR] Running (PID: %d)\n", getpid());
    fprintf(stdout, "[COMPOSITOR] Managing %d windows\n", window_count);

    while (running) {
        sleep(2);
    }

    if (display_fd >= 0)
        close(display_fd);

    unlink(PID_FILE);
    fprintf(stdout, "[COMPOSITOR] Shutdown complete\n");
    return 0;
}
