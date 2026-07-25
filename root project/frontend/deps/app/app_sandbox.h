#ifndef _SERVECOSYS_APP_SANDBOX_H_
#define _SERVECOSYS_APP_SANDBOX_H_

#include <sys/types.h>
#include <stdint.h>

#define APP_NAME_MAX      64
#define APP_PERM_MAX      32
#define APP_PATH_MAX      256

typedef enum {
    APP_PERM_NETWORK,
    APP_PERM_STORAGE_READ,
    APP_PERM_STORAGE_WRITE,
    APP_PERM_LOCATION,
    APP_PERM_CAMERA,
    APP_PERM_MICROPHONE,
    APP_PERM_NOTIFICATIONS,
    APP_PERM_COUNT,
} app_perm_t;

static const char *app_perm_names[APP_PERM_COUNT] = {
    [APP_PERM_NETWORK]        = "network",
    [APP_PERM_STORAGE_READ]   = "storage_read",
    [APP_PERM_STORAGE_WRITE]  = "storage_write",
    [APP_PERM_LOCATION]       = "location",
    [APP_PERM_CAMERA]         = "camera",
    [APP_PERM_MICROPHONE]     = "microphone",
    [APP_PERM_NOTIFICATIONS]  = "notifications",
};

typedef struct {
    int granted[APP_PERM_COUNT];
} app_permset_t;

typedef struct {
    char name[APP_NAME_MAX];
    char package_path[APP_PATH_MAX];
    pid_t pid;
    app_permset_t perms;
    int  perm_level;
    int  is_running;
} app_instance_t;

int  app_permset_init(app_permset_t *perms);
int  app_permset_grant(app_permset_t *perms, app_perm_t perm);
int  app_permset_revoke(app_permset_t *perms, app_perm_t perm);
int  app_permset_check(const app_permset_t *perms, app_perm_t perm);
const char *app_perm_name(app_perm_t perm);
app_perm_t app_perm_from_name(const char *name);

int  app_parse_manifest(const char *manifest_path, app_instance_t *app);
int  app_launch(app_instance_t *app, const char *ssle_path);
int  app_terminate(app_instance_t *app);
int  app_get_status(const app_instance_t *app);

#endif
