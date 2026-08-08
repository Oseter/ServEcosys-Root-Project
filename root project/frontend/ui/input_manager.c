/**
 * ServEcosys UID - Input Manager
 *
 * 职责：
 * - 统一输入管理（键盘/鼠标/触控/手写笔）
 * - 输入事件分发
 * - 多输入源协调
 * - 手势识别
 *
 * 运行在 uid_dom_t 域
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <signal.h>

#define IM_VERSION          "0.1.0"
#define INPUT_DEV_DIR       "/dev/input"
#define IPC_SOCK_PATH       "/var/run/servecosys_input.sock"
#define PID_FILE            "/var/run/input_manager.pid"
#define MAX_DEVICES         32
#define MAX_EVENTS          256

typedef enum {
    EVENT_KEYBOARD,
    EVENT_MOUSE,
    EVENT_TOUCH,
    EVENT_STYLUS,
    EVENT_GAMEPAD,
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    int                device_id;
    int                code;
    int                value;
    int                absolute_x;
    int                absolute_y;
    int                pressure;
    unsigned long      timestamp_ms;
} servecosys_input_event_t;

typedef struct {
    int     fd;
    char    name[256];
    char    devpath[256];
    input_event_type_t type;
    int     active;
} input_device_t;

static input_device_t devices[MAX_DEVICES];
static int device_count = 0;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;
static servecosys_input_event_t event_queue[MAX_EVENTS];
static int event_head = 0, event_tail = 0;
static volatile sig_atomic_t running = 1;

static input_event_type_t classify_device(const char *name, unsigned long ev_bits)
{
    if (strstr(name, "keyboard") || strstr(name, "Keyboard") ||
        strstr(name, "kbd"))
        return EVENT_KEYBOARD;

    if (strstr(name, "mouse") || strstr(name, "Mouse"))
        return EVENT_MOUSE;

    if (strstr(name, "touch") || strstr(name, "Touch") ||
        strstr(name, "ts"))
        return EVENT_TOUCH;

    if (strstr(name, "pen") || strstr(name, "wacom") ||
        strstr(name, "stylus"))
        return EVENT_STYLUS;

    if (strstr(name, "gamepad") || strstr(name, "joystick"))
        return EVENT_GAMEPAD;

    if (ev_bits & (1UL << EV_ABS)) return EVENT_TOUCH;
    if (ev_bits & (1UL << EV_KEY)) return EVENT_KEYBOARD;
    if (ev_bits & (1UL << EV_REL)) return EVENT_MOUSE;

    return EVENT_KEYBOARD;
}

static int discover_devices(void)
{
    DIR *dir = opendir(INPUT_DEV_DIR);
    if (!dir) {
        fprintf(stderr, "[IM] Cannot open %s: %s\n", INPUT_DEV_DIR, strerror(errno));
        return -1;
    }

    device_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL && device_count < MAX_DEVICES) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        char devpath[256];
        snprintf(devpath, sizeof(devpath), "%s/%s", INPUT_DEV_DIR, entry->d_name);

        int fd = open(devpath, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        input_device_t *dev = &devices[device_count];
        dev->fd = fd;
        strncpy(dev->devpath, devpath, sizeof(dev->devpath) - 1);

        if (ioctl(fd, EVIOCGNAME(sizeof(dev->name)), dev->name) < 0)
            snprintf(dev->name, sizeof(dev->name), "unknown-%s", entry->d_name);

        unsigned long ev_bits = 0;
        ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), &ev_bits);

        dev->type = classify_device(dev->name, ev_bits);
        dev->active = 1;
        device_count++;

        fprintf(stdout, "[IM] Device #%d: %-30s %-10s fd=%d\n",
                device_count, dev->name,
                dev->type == EVENT_KEYBOARD ? "keyboard" :
                dev->type == EVENT_MOUSE    ? "mouse"    :
                dev->type == EVENT_TOUCH    ? "touch"    :
                dev->type == EVENT_STYLUS   ? "stylus"   :
                dev->type == EVENT_GAMEPAD  ? "gamepad"  : "unknown",
                fd);
    }

    closedir(dir);
    return device_count;
}

static void enqueue_event(const servecosys_input_event_t *ev)
{
    pthread_mutex_lock(&event_mutex);

    int next = (event_tail + 1) % MAX_EVENTS;
    if (next != event_head) {
        event_queue[event_tail] = *ev;
        event_tail = next;
    }

    pthread_mutex_unlock(&event_mutex);
}

static int dequeue_event(servecosys_input_event_t *ev)
{
    pthread_mutex_lock(&event_mutex);

    if (event_head == event_tail) {
        pthread_mutex_unlock(&event_mutex);
        return -1;
    }

    *ev = event_queue[event_head];
    event_head = (event_head + 1) % MAX_EVENTS;

    pthread_mutex_unlock(&event_mutex);
    return 0;
}

static void *event_reader(void *arg)
{
    struct input_event ev;
    servecosys_input_event_t sev;

    while (running) {
        int any_read = 0;

        for (int i = 0; i < device_count; i++) {
            if (!devices[i].active) continue;

            ssize_t n = read(devices[i].fd, &ev, sizeof(ev));
            if (n == sizeof(ev)) {
                any_read = 1;

                sev.type = devices[i].type;
                sev.device_id = i;
                sev.code = ev.code;
                sev.value = ev.value;
                sev.timestamp_ms = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000;

                if (ev.type == EV_KEY)
                    enqueue_event(&sev);
                else if (ev.type == EV_ABS && ev.code == ABS_X)
                    sev.absolute_x = ev.value;
                else if (ev.type == EV_ABS && ev.code == ABS_Y)
                    sev.absolute_y = ev.value;
                else if (ev.type == EV_REL && ev.code == REL_X)
                    sev.absolute_x += ev.value;
                else if (ev.type == EV_REL && ev.code == REL_Y)
                    sev.absolute_y += ev.value;
            }
        }

        if (!any_read)
            usleep(1000);
    }

    return NULL;
}

static void *ipc_dispatcher(void *arg)
{
    struct sockaddr_un addr;
    unlink(IPC_SOCK_PATH);

    int server_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (server_fd < 0) return NULL;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCK_PATH, sizeof(addr.sun_path) - 1);

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);

    while (running) {
        struct sockaddr_un client;
        socklen_t len = sizeof(client);
        int client_fd = accept(server_fd, (struct sockaddr *)&client, &len);
        if (client_fd < 0) continue;

        servecosys_input_event_t ev;
        if (dequeue_event(&ev) == 0) {
            write(client_fd, &ev, sizeof(ev));
        }

        close(client_fd);
    }

    close(server_fd);
    unlink(IPC_SOCK_PATH);
    return NULL;
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "ServEcosys Input Manager v%s\n", IM_VERSION);
    fprintf(stdout, "Running in uid_dom_t domain\n\n");

    discover_devices();

    pthread_t reader_thread, ipc_thread;
    pthread_create(&reader_thread, NULL, event_reader, NULL);
    pthread_create(&ipc_thread, NULL, ipc_dispatcher, NULL);

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    fprintf(stdout, "[IM] Running (PID: %d)\n", getpid());
    fprintf(stdout, "[IM] Monitoring %d input devices\n", device_count);

    while (running) {
        sleep(5);
    }

    running = 0;
    pthread_join(reader_thread, NULL);
    pthread_join(ipc_thread, NULL);

    for (int i = 0; i < device_count; i++)
        close(devices[i].fd);

    unlink(PID_FILE);
    fprintf(stdout, "[IM] Shutdown complete\n");
    return 0;
}
