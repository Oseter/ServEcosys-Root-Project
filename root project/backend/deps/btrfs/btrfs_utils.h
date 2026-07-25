#ifndef _SERVECOSYS_BTRFS_UTILS_H_
#define _SERVECOSYS_BTRFS_UTILS_H_

#define BTRFS_MAX_SNAPSHOTS 64
#define BTRFS_NAME_MAX      128
#define BTRFS_PATH_MAX      512

typedef struct {
    char name[BTRFS_NAME_MAX];
    char created_at[32];
    char path[BTRFS_PATH_MAX];
    int  is_boot;
} btrfs_snapshot_t;

typedef struct {
    btrfs_snapshot_t snapshots[BTRFS_MAX_SNAPSHOTS];
    int count;
} btrfs_snaplist_t;

int  btrfs_detect(const char *device);
int  btrfs_mount_subvol(const char *device, const char *subvol,
                        const char *mountpoint, int readonly);
int  btrfs_create_snapshot(const char *source, const char *dest, int readonly);
int  btrfs_delete_snapshot(const char *path);
int  btrfs_list_snapshots(const char *snapshots_dir, btrfs_snaplist_t *list);
int  btrfs_rollback(const char *snapshot_path, const char *target);
int  btrfs_get_usage(const char *mountpoint, uint64_t *total, uint64_t *used);

#endif
