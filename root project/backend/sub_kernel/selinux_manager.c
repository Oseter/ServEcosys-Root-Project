/**
 * ServEcosys SED - SELinux Policy Manager
 *
 * Responsibilities
 * - Load and manage the SELinux policy
 * - Hot-reload interface
 * - Audit log management
 *
 * Runs in the sys_dom_t domain.
 *
 * Uses the self-contained reference monitor (deps/selinux/selinux_wrap)
 * so no system libselinux dependency is required.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include "selinux_wrap.h"

#define SED_VERSION   "0.1.0"
#define POLICY_DIR    "/system/backend/etc/selinux"
#define POLICY_BIN    POLICY_DIR "/servecosys.pp"
#define AUDIT_LOG     "/var/log/sed/audit.log"
#define PID_FILE      "/var/run/selinux_manager.pid"

static volatile sig_atomic_t running = 1;
static selinux_state_t policy_state;

static void handle_signal(int sig)
{
    running = 0;
}

static int init_audit_log(void)
{
    const char *log_dir = "/var/log/sed";
    mkdir(log_dir, 0755);

    int fd = open(AUDIT_LOG, O_WRONLY | O_CREAT | O_APPEND, 0640);
    if (fd < 0) {
        fprintf(stderr, "[SED] Failed to open audit log: %s\n", strerror(errno));
        return -1;
    }

    FILE *log = fdopen(fd, "a");
    if (!log) {
        close(fd);
        return -1;
    }

    fprintf(log, "=== SELinux Manager v%s started at %ld ===\n",
            SED_VERSION, (long)time(NULL));
    fclose(log);
    return 0;
}

static int load_selinux_policy(void)
{
    struct stat st;

    if (stat(POLICY_BIN, &st) != 0) {
        fprintf(stderr, "[SED] Policy file not found: %s\n", POLICY_BIN);
        return -1;
    }

    fprintf(stdout, "[SED] Loading SELinux policy: %s (%ld bytes)\n",
            POLICY_BIN, (long)st.st_size);

    if (selinux_load_policy(POLICY_BIN) != 0) {
        fprintf(stderr, "[SED] Failed to load policy: %s\n", strerror(errno));
        return -1;
    }

    fprintf(stdout, "[SED] Policy loaded successfully\n");
    return 0;
}

static int reload_policy(void)
{
    fprintf(stdout, "[SED] Reloading SELinux policy...\n");

    if (selinux_reload_policy() != 0)
        return -1;

    fprintf(stdout, "[SED] Policy reloaded\n");
    return 0;
}

static int check_permission(const char *source_context, const char *target_context,
                            const char *tclass, const char *perm)
{
    int ret = selinux_check(source_context, target_context, tclass, perm);

    if (ret < 0) {
        fprintf(stderr, "[SED] selinux_check error: %s <-> %s : %s {%s}\n",
                source_context, target_context, tclass, perm);
        return -1;
    }

    if (ret == 0) {
        fprintf(stdout, "[SED] ALLOWED: %s -> %s : %s {%s}\n",
                source_context, target_context, tclass, perm);
        return 0;
    }

    fprintf(stdout, "[SED] DENIED: %s -> %s : %s {%s}\n",
            source_context, target_context, tclass, perm);
    return 1;
}

static void print_policy_info(void)
{
    fprintf(stdout, "[SED] SELinux mode: %s\n",
            policy_state.enforcing ? "enforcing" : "permissive");
}

int main(int argc, char *argv[])
{
    fprintf(stdout, "ServEcosys SELinux Manager v%s\n", SED_VERSION);
    fprintf(stdout, "Running in sys_dom_t security domain\n\n");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    if (selinux_init(&policy_state) != 0) {
        fprintf(stderr, "[SED] selinux_init failed\n");
        return 1;
    }

    if (init_audit_log() != 0) {
        fprintf(stderr, "[SED] Audit log initialization failed\n");
        return 1;
    }

    if (load_selinux_policy() != 0) {
        fprintf(stderr, "[SED] Policy loading failed\n");
        return 1;
    }

    print_policy_info();

    FILE *pid_fd = fopen(PID_FILE, "w");
    if (pid_fd) {
        fprintf(pid_fd, "%d\n", getpid());
        fclose(pid_fd);
    }

    fprintf(stdout, "[SED] Entering main loop (PID: %d)\n", getpid());
    fprintf(stdout, "[SED] Ready to serve permission requests\n");

    while (running) {
        pause();
    }

    fprintf(stdout, "[SED] Shutting down...\n");
    unlink(PID_FILE);
    return 0;
}