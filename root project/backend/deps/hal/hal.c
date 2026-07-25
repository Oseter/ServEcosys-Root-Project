#include "hal.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

const char *hal_class_name(hal_dev_class_t cls) {
    switch (cls) {
        case HAL_STORAGE: return "storage";
        case HAL_NETWORK: return "network";
        case HAL_INPUT:   return "input";
        case HAL_DISPLAY: return "display";
        case HAL_AUDIO:   return "audio";
        case HAL_USB:     return "usb";
        case HAL_PCI:     return "pci";
        default:          return "unknown";
    }
}

hal_dev_class_t hal_classify(const char *subsystem) {
    if (!subsystem) return HAL_UNKNOWN;
    if (strcmp(subsystem, "block") == 0)   return HAL_STORAGE;
    if (strcmp(subsystem, "net") == 0)     return HAL_NETWORK;
    if (strcmp(subsystem, "input") == 0)   return HAL_INPUT;
    if (strcmp(subsystem, "drm") == 0)     return HAL_DISPLAY;
    if (strcmp(subsystem, "sound") == 0)   return HAL_AUDIO;
    if (strcmp(subsystem, "usb") == 0)     return HAL_USB;
    if (strcmp(subsystem, "pci") == 0)     return HAL_PCI;
    return HAL_UNKNOWN;
}

static int scan_sysfs_dir(const char *sysfs_path, hal_devtable_t *table) {
    DIR *dir = opendir(sysfs_path);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && table->count < HAL_MAX_DEVICES) {
        if (entry->d_name[0] == '.') continue;

        char dev_path[PATH_MAX];
        snprintf(dev_path, sizeof(dev_path), "%s/%s", sysfs_path, entry->d_name);

        char subsys_link[PATH_MAX];
        char subsystem[HAL_NAME_MAX] = "unknown";
        ssize_t len = readlink(dev_path, subsys_link, sizeof(subsys_link) - 1);
        if (len > 0) {
            subsys_link[len] = 0;
            char *s = strrchr(subsys_link, '/');
            if (s) {
                strncpy(subsystem, s + 1, HAL_NAME_MAX - 1);
                subsystem[HAL_NAME_MAX - 1] = 0;
            }
        }

        hal_device_t *dev = &table->devices[table->count];
        snprintf(dev->devpath, sizeof(dev->devpath), "%s", dev_path);
        snprintf(dev->name, sizeof(dev->name), "%s", entry->d_name);
        snprintf(dev->driver, sizeof(dev->driver), "%s", subsystem);
        dev->class = hal_classify(subsystem);
        dev->major = 0; dev->minor = 0;
        dev->claimed = 0;
        dev->owner[0] = 0;
        table->count++;
    }

    closedir(dir);
    return 0;
}

int hal_scan_all(hal_devtable_t *table) {
    if (!table) return -1;
    table->count = 0;

    const char *classes[] = {
        "/sys/class/block", "/sys/class/net",
        "/sys/class/input", "/sys/class/drm",
        "/sys/class/sound", "/sys/class/usb_device",
        "/sys/class/tty", NULL
    };

    for (int i = 0; classes[i]; i++)
        scan_sysfs_dir(classes[i], table);

    return table->count;
}

int hal_scan_class(hal_devtable_t *table, hal_dev_class_t cls) {
    if (!table) return -1;
    table->count = 0;

    const char *path = NULL;
    switch (cls) {
        case HAL_STORAGE: path = "/sys/class/block"; break;
        case HAL_NETWORK: path = "/sys/class/net"; break;
        case HAL_INPUT:   path = "/sys/class/input"; break;
        case HAL_DISPLAY: path = "/sys/class/drm"; break;
        case HAL_AUDIO:   path = "/sys/class/sound"; break;
        case HAL_PCI:     path = "/sys/class/pci"; break;
        default: return -1;
    }

    return scan_sysfs_dir(path, table);
}

int hal_claim(hal_devtable_t *table, int index, const char *owner) {
    if (!table || index < 0 || index >= table->count) return -1;
    hal_device_t *dev = &table->devices[index];
    if (dev->claimed) return -1;
    dev->claimed = 1;
    strncpy(dev->owner, owner ? owner : "unknown", HAL_NAME_MAX - 1);
    return 0;
}

int hal_release(hal_devtable_t *table, int index) {
    if (!table || index < 0 || index >= table->count) return -1;
    table->devices[index].claimed = 0;
    table->devices[index].owner[0] = 0;
    return 0;
}

int hal_find(hal_devtable_t *table, hal_dev_class_t cls, int *indices, int max) {
    if (!table || !indices) return -1;
    int found = 0;
    for (int i = 0; i < table->count && found < max; i++) {
        if (table->devices[i].class == cls)
            indices[found++] = i;
    }
    return found;
}
