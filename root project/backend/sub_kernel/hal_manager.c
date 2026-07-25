/**
 * ServEcosys SED - Hardware Abstraction Layer Manager
 *
 * 职责：
 * - 硬件设备抽象与统一访问接口
 * - 设备热插拔监控
 * - 硬件资源分配与隔离
 * - 向 UID 域提供设备访问代理
 *
 * 运行在 sys_dom_t 域
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <linux/limits.h>
#include <signal.h>

#define HAL_VERSION     "0.1.0"
#define DEVICE_DB       "/system/backend/data/hal/devices.db"
#define PID_FILE        "/var/run/hal_manager.pid"

typedef enum {
    DEVICE_STORAGE,
    DEVICE_NETWORK,
    DEVICE_INPUT,
    DEVICE_DISPLAY,
    DEVICE_AUDIO,
    DEVICE_USB,
    DEVICE_PCI,
    DEVICE_UNKNOWN
} device_class_t;

typedef struct {
    char        devpath[PATH_MAX];
    char        driver[64];
    device_class_t class;
    int         major;
    int         minor;
    int         claimed;    /* 0=unclaimed, 1=claimed by SED, 2=assigned to UID */
    char        owner[64];  /* SELinux context of owner */
} hal_device_t;

#define MAX_DEVICES 256
static hal_device_t device_table[MAX_DEVICES];
static int device_count = 0;
static volatile sig_atomic_t running = 1;

static const char *device_class_name(device_class_t cls)
{
    switch (cls) {
        case DEVICE_STORAGE: return "storage";
        case DEVICE_NETWORK: return "network";
        case DEVICE_INPUT:   return "input";
        case DEVICE_DISPLAY: return "display";
        case DEVICE_AUDIO:   return "audio";
        case DEVICE_USB:     return "usb";
        case DEVICE_PCI:     return "pci";
        default:             return "unknown";
    }
}

static device_class_t classify_device(const char *devpath, const char *subsystem)
{
    if (!subsystem) return DEVICE_UNKNOWN;

    if (strcmp(subsystem, "block") == 0)        return DEVICE_STORAGE;
    if (strcmp(subsystem, "net") == 0)           return DEVICE_NETWORK;
    if (strcmp(subsystem, "input") == 0)         return DEVICE_INPUT;
    if (strcmp(subsystem, "drm") == 0)           return DEVICE_DISPLAY;
    if (strcmp(subsystem, "sound") == 0)         return DEVICE_AUDIO;
    if (strcmp(subsystem, "usb") == 0)           return DEVICE_USB;
    if (strcmp(subsystem, "pci") == 0)           return DEVICE_PCI;

    return DEVICE_UNKNOWN;
}

static int scan_device(const char *sysfs_path)
{
    char devpath[PATH_MAX];
    char subsystem[64];
    char uevent_path[PATH_MAX];
    FILE *f;
    int major = 0, minor = 0;

    snprintf(uevent_path, sizeof(uevent_path), "%s/uevent", sysfs_path);
    f = fopen(uevent_path, "r");
    if (!f) return -1;

    while (fgets(devpath, sizeof(devpath), f)) {
        devpath[strcspn(devpath, "\n")] = 0;
        if (sscanf(devpath, "MAJOR=%d", &major) == 1) continue;
        if (sscanf(devpath, "MINOR=%d", &minor) == 1) continue;
        if (strncmp(devpath, "DEVNAME=", 8) == 0) {
            snprintf(devpath, sizeof(devpath), "/dev/%s", devpath + 8);
        }
    }
    fclose(f);

    snprintf(subsystem, sizeof(subsystem), "%s/subsystem", sysfs_path);
    char subsys_link[PATH_MAX];
    ssize_t len = readlink(subsystem, subsys_link, sizeof(subsys_link) - 1);
    if (len > 0) {
        subsys_link[len] = 0;
        char *s = strrchr(subsys_link, '/');
        if (s) {
            strncpy(subsystem, s + 1, sizeof(subsystem) - 1);
            subsystem[sizeof(subsystem) - 1] = 0;
        }
    } else {
        strcpy(subsystem, "unknown");
    }

    if (device_count >= MAX_DEVICES) return -1;

    hal_device_t *dev = &device_table[device_count];
    strncpy(dev->devpath, devpath, sizeof(dev->devpath) - 1);
    strncpy(dev->driver, subsystem, sizeof(dev->driver) - 1);
    dev->class = classify_device(devpath, subsystem);
    dev->major = major;
    dev->minor = minor;
    dev->claimed = 0;
    strcpy(dev->owner, "unclaimed");

    device_count++;
    return 0;
}

static int scan_all_devices(void)
{
    const char *sysfs_classes[] = {
        "/sys/class/block", "/sys/class/net",
        "/sys/class/input", "/sys/class/drm",
        "/sys/class/sound", "/sys/class/usb_device",
        "/sys/class/tty", NULL
    };

    device_count = 0;
    fprintf(stdout, "[HAL] Scanning hardware...\n");

    for (int c = 0; sysfs_classes[c]; c++) {
        DIR *dir = opendir(sysfs_classes[c]);
        if (!dir) continue;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;

            char dev_path[PATH_MAX];
            snprintf(dev_path, sizeof(dev_path), "%s/%s", sysfs_classes[c], entry->d_name);
            scan_device(dev_path);
        }
        closedir(dir);
    }

    fprintf(stdout, "[HAL] Found %d devices\n", device_count);
    return device_count;
}

static int claim_device(int index, const char *owner_context)
{
    if (index < 0 || index >= device_count)
        return -1;

    hal_device_t *dev = &device_table[index];

    if (dev->claimed) {
        fprintf(stderr, "[HAL] Device %s already claimed by %s\n",
                dev->devpath, dev->owner);
        return -1;
    }

    dev->claimed = 1;
    strncpy(dev->owner, owner_context, sizeof(dev->owner) - 1);
    fprintf(stdout, "[HAL] Device claimed: %s -> %s\n", dev->devpath, owner_context);
    return 0;
}

static void print_device_table(void)
{
    fprintf(stdout, "\n[HAL] Device Table:\n");
    fprintf(stdout, "  %-4s %-30s %-12s %-10s %-8s %s\n",
            "Idx", "DevPath", "Driver", "Class", "Claimed", "Owner");

    for (int i = 0; i < device_count; i++) {
        hal_device_t *dev = &device_table[i];
        fprintf(stdout, "  %-4d %-30s %-12s %-10s %-8s %s\n",
                i, dev->devpath, dev->driver,
                device_class_name(dev->class),
                dev->claimed ? "yes" : "no",
                dev->claimed ? dev->owner : "-");
    }
    fprintf(stdout, "  Total: %d devices\n\n", device_count);
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "ServEcosys HAL Manager v%s\n", HAL_VERSION);
    fprintf(stdout, "Running in sys_dom_t security domain\n\n");

    signal(SIGTERM, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    scan_all_devices();
    print_device_table();

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    fprintf(stdout, "[HAL] Running (PID: %d)\n", getpid());

    while (running) {
        sleep(5);
    }

    unlink(PID_FILE);
    return 0;
}
