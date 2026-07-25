/**
 * ServEcosys UID - Display Server
 *
 * 职责：
 * - 显示服务管理
 * - 帧缓冲管理
 * - 多显示器支持
 * - 合成器输出接口
 *
 * 运行在 uid_dom_t 域
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <signal.h>
#include <pthread.h>

#define DS_VERSION         "0.1.0"
#define FB_DEVICE          "/dev/fb0"
#define IPC_SOCK_PATH      "/var/run/servecosys_display.sock"
#define PID_FILE           "/var/run/display_server.pid"

typedef struct {
    int           fd;
    char          devpath[256];
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    size_t        screensize;
    unsigned char *buffer;
} display_t;

typedef struct {
    int x, y;
    int width, height;
    int stride;
    int bpp;
} display_rect_t;

static display_t display;
static pthread_mutex_t flip_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t running = 1;

static int open_display(const char *devpath)
{
    display.fd = open(devpath, O_RDWR);
    if (display.fd < 0) {
        fprintf(stderr, "[DS] Failed to open %s: %s\n", devpath, strerror(errno));
        return -1;
    }

    if (ioctl(display.fd, FBIOGET_VSCREENINFO, &display.vinfo) < 0) {
        fprintf(stderr, "[DS] Failed to get vinfo: %s\n", strerror(errno));
        close(display.fd);
        return -1;
    }

    if (ioctl(display.fd, FBIOGET_FSCREENINFO, &display.finfo) < 0) {
        fprintf(stderr, "[DS] Failed to get finfo: %s\n", strerror(errno));
        close(display.fd);
        return -1;
    }

    display.screensize = display.finfo.smem_len;
    display.buffer = mmap(NULL, display.screensize,
                           PROT_READ | PROT_WRITE, MAP_SHARED,
                           display.fd, 0);

    if (display.buffer == MAP_FAILED) {
        fprintf(stderr, "[DS] Failed to mmap: %s\n", strerror(errno));
        close(display.fd);
        return -1;
    }

    strncpy(display.devpath, devpath, sizeof(display.devpath) - 1);

    fprintf(stdout, "[DS] Display opened: %dx%d, %dbpp\n",
            display.vinfo.xres, display.vinfo.yres,
            display.vinfo.bits_per_pixel);

    return 0;
}

static void display_fill_rect(int x, int y, int w, int h, unsigned int color)
{
    pthread_mutex_lock(&flip_mutex);

    int bpp = display.vinfo.bits_per_pixel / 8;
    int stride = display.finfo.line_length;

    for (int row = y; row < y + h && row < display.vinfo.yres; row++) {
        unsigned char *line = display.buffer + row * stride + x * bpp;
        for (int col = 0; col < w && (x + col) < display.vinfo.xres; col++) {
            line[col * bpp]     = color & 0xFF;
            line[col * bpp + 1] = (color >> 8) & 0xFF;
            line[col * bpp + 2] = (color >> 16) & 0xFF;
            if (bpp == 4)
                line[col * bpp + 3] = (color >> 24) & 0xFF;
        }
    }

    pthread_mutex_unlock(&flip_mutex);
}

static void display_clear(unsigned int color)
{
    display_fill_rect(0, 0, display.vinfo.xres, display.vinfo.yres, color);
}

static void render_status_bar(void)
{
    unsigned int bar_color = 0x202020;
    unsigned int text_color = 0xFFFFFF;
    int bar_height = 32;

    display_fill_rect(0, 0, display.vinfo.xres, bar_height, bar_color);

    int bpp = display.vinfo.bits_per_pixel / 8;
    int stride = display.finfo.line_length;

    for (int x = 10; x < display.vinfo.xres - 10; x += 8) {
        for (int y = 8; y < bar_height - 8; y += 2) {
            int pos = y * stride + x * bpp;
            display.buffer[pos]     = text_color & 0xFF;
            display.buffer[pos + 1] = (text_color >> 8) & 0xFF;
            display.buffer[pos + 2] = (text_color >> 16) & 0xFF;
        }
    }
}

static void *ipc_listener(void *arg)
{
    struct sockaddr_un addr;
    int server_fd;

    unlink(IPC_SOCK_PATH);
    server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (server_fd < 0) return NULL;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);

    fprintf(stdout, "[DS] IPC listener ready on %s\n", IPC_SOCK_PATH);

    while (running) {
        struct sockaddr_un client;
        socklen_t len = sizeof(client);
        int client_fd = accept(server_fd, (struct sockaddr *)&client, &len);
        if (client_fd < 0) continue;

        char cmd[256];
        ssize_t n = read(client_fd, cmd, sizeof(cmd) - 1);
        if (n > 0) {
            cmd[n] = 0;

            if (strncmp(cmd, "FILL_RECT", 9) == 0) {
                int x, y, w, h;
                unsigned int color;
                sscanf(cmd + 10, "%d %d %d %d %x", &x, &y, &w, &h, &color);
                display_fill_rect(x, y, w, h, color);
            }
            else if (strncmp(cmd, "CLEAR", 5) == 0) {
                unsigned int color;
                sscanf(cmd + 6, "%x", &color);
                display_clear(color);
            }
        }

        close(client_fd);
    }

    close(server_fd);
    unlink(IPC_SOCK_PATH);
    return NULL;
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "ServEcosys Display Server v%s\n", DS_VERSION);
    fprintf(stdout, "Running in uid_dom_t domain\n\n");

    const char *fb_dev = argc > 1 ? argv[1] : FB_DEVICE;

    if (open_display(fb_dev) != 0) {
        fprintf(stderr, "[DS] Failed to initialize display\n");
        return 1;
    }

    display_clear(0x1A1A2E);
    render_status_bar();

    pthread_t ipc_thread;
    pthread_create(&ipc_thread, NULL, ipc_listener, NULL);

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    fprintf(stdout, "[DS] Running (PID: %d)\n", getpid());
    fprintf(stdout, "[DS] Resolution: %dx%d @ %dbpp\n",
            display.vinfo.xres, display.vinfo.yres,
            display.vinfo.bits_per_pixel);

    while (running) {
        sleep(1);
    }

    pthread_join(ipc_thread, NULL);

    munmap(display.buffer, display.screensize);
    close(display.fd);
    unlink(PID_FILE);

    fprintf(stdout, "[DS] Shutdown complete\n");
    return 0;
}
