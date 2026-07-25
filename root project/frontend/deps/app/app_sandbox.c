#include "app_sandbox.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

int app_permset_init(app_permset_t *perms) {
    if (!perms) return -1;
    memset(perms, 0, sizeof(*perms));
    return 0;
}

int app_permset_grant(app_permset_t *perms, app_perm_t perm) {
    if (!perms || perm >= APP_PERM_COUNT) return -1;
    perms->granted[perm] = 1;
    return 0;
}

int app_permset_revoke(app_permset_t *perms, app_perm_t perm) {
    if (!perms || perm >= APP_PERM_COUNT) return -1;
    perms->granted[perm] = 0;
    return 0;
}

int app_permset_check(const app_permset_t *perms, app_perm_t perm) {
    if (!perms || perm >= APP_PERM_COUNT) return 0;
    return perms->granted[perm];
}

const char *app_perm_name(app_perm_t perm) {
    if (perm < APP_PERM_COUNT) return app_perm_names[perm];
    return "unknown";
}

app_perm_t app_perm_from_name(const char *name) {
    if (!name) return APP_PERM_COUNT;
    for (int i = 0; i < APP_PERM_COUNT; i++)
        if (strcmp(name, app_perm_names[i]) == 0)
            return (app_perm_t)i;
    return APP_PERM_COUNT;
}

int app_parse_manifest(const char *manifest_path, app_instance_t *app) {
    if (!manifest_path || !app) return -1;

    FILE *f = fopen(manifest_path, "r");
    if (!f) return -1;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "\"network\":"))
            app->perms.granted[APP_PERM_NETWORK] =
                (strstr(line, "true") != NULL);
        if (strstr(line, "\"storage\":"))
            app->perms.granted[APP_PERM_STORAGE_READ] =
                (strstr(line, "true") != NULL);
        if (strstr(line, "\"location\":"))
            app->perms.granted[APP_PERM_LOCATION] =
                (strstr(line, "true") != NULL);
        if (strstr(line, "\"notifications\":"))
            app->perms.granted[APP_PERM_NOTIFICATIONS] =
                (strstr(line, "true") != NULL);
        if (strstr(line, "\"min_permission_level\":")) {
            int level;
            if (sscanf(line, " \"min_permission_level\": %d,", &level) == 1)
                app->perm_level = level;
        }
    }

    fclose(f);
    return 0;
}

int app_launch(app_instance_t *app, const char *ssle_path) {
    if (!app || !ssle_path) return -1;

    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        setsid();
        strncpy(app->package_path, ssle_path, APP_PATH_MAX - 1);
        app->is_running = 1;
        app->pid = getpid();
        return 0;
    }

    app->pid = pid;
    app->is_running = 1;
    return 0;
}

int app_terminate(app_instance_t *app) {
    if (!app || !app->is_running) return -1;

    if (kill(app->pid, SIGTERM) == 0) {
        int status;
        waitpid(app->pid, &status, WNOHANG);
        app->is_running = 0;
        app->pid = 0;
        return 0;
    }

    return -1;
}

int app_get_status(const app_instance_t *app) {
    if (!app) return -1;
    if (!app->is_running) return 0;

    if (kill(app->pid, 0) == 0)
        return 1;

    return 0;
}
