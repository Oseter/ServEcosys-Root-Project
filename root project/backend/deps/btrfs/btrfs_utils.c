#include "btrfs_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>

/*
 * 防御命令注入：所有经由 system() 拼接进 shell 的参数必须先过此校验，
 * 拒绝含 shell 元字符的输入。
 */
static int btrfs_shell_safe(const char *s)
{
    if (!s || !s[0]) return 0;
    if (strchr(s, ';') || strchr(s, '&') || strchr(s, '|') ||
        strchr(s, '$') || strchr(s, '`') || strchr(s, '<') ||
        strchr(s, '>') || strchr(s, '(') || strchr(s, ')') ||
        strchr(s, '\n') || strchr(s, '\r') || strchr(s, '\\') ||
        strchr(s, '\''))
        return 0;
    return 1;
}

int btrfs_detect(const char *device) {
    if (!device || !btrfs_shell_safe(device)) return -1;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "blkid -t TYPE=btrfs \"%s\" >/dev/null 2>&1", device);
    return (system(cmd) == 0) ? 0 : -1;
}

int btrfs_mount_subvol(const char *device, const char *subvol,
                       const char *mountpoint, int readonly)
{
    if (!device || !mountpoint) return -1;
    if (!btrfs_shell_safe(device) || !btrfs_shell_safe(mountpoint) ||
        (subvol && subvol[0] && !btrfs_shell_safe(subvol)))
        return -1;

    mkdir(mountpoint, 0755);

    char cmd[1024];
    if (subvol && subvol[0])
        snprintf(cmd, sizeof(cmd), "mount -t btrfs -o subvol=%s%s \"%s\" \"%s\" >/dev/null 2>&1",
                 subvol, readonly ? ",ro" : "", device, mountpoint);
    else
        snprintf(cmd, sizeof(cmd), "mount -t btrfs%s \"%s\" \"%s\" >/dev/null 2>&1",
                 readonly ? " -o ro" : "", device, mountpoint);

    return (system(cmd) == 0) ? 0 : -1;
}

int btrfs_create_snapshot(const char *source, const char *dest, int readonly) {
    if (!source || !dest) return -1;
    if (!btrfs_shell_safe(source) || !btrfs_shell_safe(dest)) return -1;

    char parent[BTRFS_PATH_MAX];
    strncpy(parent, dest, BTRFS_PATH_MAX - 1);
    parent[BTRFS_PATH_MAX - 1] = 0;
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = 0;
        mkdir(parent, 0755);
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "btrfs subvolume snapshot%s \"%s\" \"%s\" >/dev/null 2>&1",
             readonly ? " -r" : "", source, dest);

    return (system(cmd) == 0) ? 0 : -1;
}

int btrfs_delete_snapshot(const char *path) {
    if (!path || !btrfs_shell_safe(path)) return -1;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "btrfs subvolume delete \"%s\" >/dev/null 2>&1", path);
    return (system(cmd) == 0) ? 0 : -1;
}

int btrfs_list_snapshots(const char *snapshots_dir, btrfs_snaplist_t *list) {
    if (!snapshots_dir || !list) return -1;
    list->count = 0;

    DIR *dir = opendir(snapshots_dir);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && list->count < BTRFS_MAX_SNAPSHOTS) {
        if (entry->d_name[0] == '.') continue;

        char full_path[BTRFS_PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", snapshots_dir, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;

        btrfs_snapshot_t *s = &list->snapshots[list->count];
        strncpy(s->name, entry->d_name, BTRFS_NAME_MAX - 1);
        strncpy(s->path, full_path, BTRFS_PATH_MAX - 1);
        s->is_boot = 0;
        list->count++;
    }

    closedir(dir);
    return list->count;
}

int btrfs_rollback(const char *snapshot_path, const char *target) {
    if (!snapshot_path || !target) return -1;
    if (!btrfs_shell_safe(snapshot_path) || !btrfs_shell_safe(target)) return -1;

    char timestamp[32];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm);

    char backup[BTRFS_PATH_MAX];
    snprintf(backup, sizeof(backup), "%s/.snapshots/pre_rollback_%s", target, timestamp);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "btrfs subvolume snapshot \"%s\" \"%s\" >/dev/null 2>&1 && "
             "btrfs subvolume delete \"%s\" >/dev/null 2>&1 && "
             "btrfs subvolume snapshot \"%s\" \"%s\" >/dev/null 2>&1",
             target, backup, target, snapshot_path, target);

    return (system(cmd) == 0) ? 0 : -1;
}

int btrfs_get_usage(const char *mountpoint, uint64_t *total, uint64_t *used) {
    if (!mountpoint) return -1;

    struct statvfs vfs;
    if (statvfs(mountpoint, &vfs) != 0) return -1;

    if (total) *total = (uint64_t)vfs.f_blocks * vfs.f_frsize;
    if (used)  *used  = (uint64_t)(vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize;

    return 0;
}
