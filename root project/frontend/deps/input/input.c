#include "input.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/ioctl.h>

static const char *INPUT_DEV_DIR = "/dev/input";

input_dev_type_t input_classify(const char *name, unsigned long ev_bits) {
    if (!name) return INPUT_UNKNOWN;
    if (strstr(name, "keyboard") || strstr(name, "Keyboard") || strstr(name, "kbd"))
        return INPUT_KEYBOARD;
    if (strstr(name, "mouse") || strstr(name, "Mouse"))
        return INPUT_MOUSE;
    if (strstr(name, "touch") || strstr(name, "Touch") || strstr(name, "ts"))
        return INPUT_TOUCH;
    if (strstr(name, "pen") || strstr(name, "wacom") || strstr(name, "stylus"))
        return INPUT_STYLUS;
    if (strstr(name, "gamepad") || strstr(name, "joystick"))
        return INPUT_GAMEPAD;
    if (ev_bits & (1UL << EV_ABS)) return INPUT_TOUCH;
    if (ev_bits & (1UL << EV_KEY)) return INPUT_KEYBOARD;
    if (ev_bits & (1UL << EV_REL)) return INPUT_MOUSE;
    return INPUT_UNKNOWN;
}

int input_open_device(input_device_t *dev, const char *devpath) {
    if (!dev || !devpath) return -1;
    memset(dev, 0, sizeof(*dev));

    dev->fd = open(devpath, O_RDONLY | O_NONBLOCK);
    if (dev->fd < 0) return -1;

    strncpy(dev->devpath, devpath, sizeof(dev->devpath) - 1);

    if (ioctl(dev->fd, EVIOCGNAME(sizeof(dev->name) - 1), dev->name) < 0)
        snprintf(dev->name, sizeof(dev->name), "unknown");

    unsigned long ev_bits = 0;
    ioctl(dev->fd, EVIOCGBIT(0, sizeof(ev_bits)), &ev_bits);
    dev->type = input_classify(dev->name, ev_bits);
    dev->active = 1;

    return 0;
}

void input_close_device(input_device_t *dev) {
    if (!dev) return;
    if (dev->fd >= 0) close(dev->fd);
    memset(dev, 0, sizeof(*dev));
    dev->fd = -1;
}

int input_discover(input_devtable_t *table) {
    if (!table) return -1;
    table->count = 0;

    DIR *dir = opendir(INPUT_DEV_DIR);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && table->count < INPUT_MAX_DEVICES) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        char devpath[256];
        snprintf(devpath, sizeof(devpath), "%s/%s", INPUT_DEV_DIR, entry->d_name);

        input_device_t *dev = &table->devices[table->count];
        if (input_open_device(dev, devpath) == 0)
            table->count++;
    }

    closedir(dir);
    return table->count;
}

int input_read_event(input_device_t *dev, input_event_t *ev) {
    if (!dev || !ev || dev->fd < 0) return -1;

    struct input_event raw;
    ssize_t n = read(dev->fd, &raw, sizeof(raw));
    if (n != sizeof(raw)) return -1;

    memset(ev, 0, sizeof(*ev));
    ev->device_id = dev - (input_device_t *)0;
    ev->code = raw.code;
    ev->value = raw.value;
    ev->timestamp_ms = (uint64_t)raw.time.tv_sec * 1000 +
                        (uint64_t)raw.time.tv_usec / 1000;

    switch (dev->type) {
        case INPUT_KEYBOARD: ev->type = INPUT_KEYBOARD; break;
        case INPUT_MOUSE:    ev->type = INPUT_MOUSE;    break;
        case INPUT_TOUCH:    ev->type = INPUT_TOUCH;    break;
        default:             ev->type = INPUT_UNKNOWN;   break;
    }

    return 0;
}
