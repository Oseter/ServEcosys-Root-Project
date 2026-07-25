/**
 * ServEcosys SED - SELinux Policy Manager
 *
 * 职责：
 * - 加载和管理 SELinux 策略
 * - 策略热更新接口
 * - 策略编译与验证
 * - 审计日志管理
 *
 * 运行在 sys_dom_t 域
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <selinux/selinux.h>
#include <selinux/context.h>
#include <signal.h>

#define SED_VERSION   "0.1.0"
#define POLICY_DIR    "/system/backend/etc/selinux"
#define POLICY_BIN    POLICY_DIR "/servecosys.pp"
#define AUDIT_LOG     "/var/log/sed/audit.log"
#define PID_FILE      "/var/run/selinux_manager.pid"

static volatile sig_atomic_t running = 1;

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

    if (security_load_policy(POLICY_BIN, st.st_size) < 0) {
        fprintf(stderr, "[SED] Failed to load policy: %s\n", strerror(errno));
        return -1;
    }

    fprintf(stdout, "[SED] Policy loaded successfully\n");
    return 0;
}

static int reload_policy(void)
{
    fprintf(stdout, "[SED] Reloading SELinux policy...\n");

    if (load_selinux_policy() != 0)
        return -1;

    fprintf(stdout, "[SED] Policy reloaded\n");
    return 0;
}

static int check_permission(const char *source_context, const char *target_context,
                            const char *tclass, const char *perm)
{
    security_id_t ssid, tsid;
    security_class_t class;
    access_vector_t perms;
    int ret;

    ret = security_compute_av(source_context, target_context, tclass, 0, &perms);
    if (ret < 0) {
        fprintf(stderr, "[SED] Permission check failed: %s -> %s : %s {%s}\n",
                source_context, target_context, tclass, perm);
        return -1;
    }

    return 0;
}

static void print_policy_info(void)
{
    char *enforce;

    enforce = security_getenforce();
    if (enforce) {
        fprintf(stdout, "[SED] SELinux mode: %s\n", enforce);
        free(enforce);
    }

    char **policy_names = NULL;
    int policy_count = security_get_policy_names(&policy_names);
    if (policy_count > 0 && policy_names) {
        fprintf(stdout, "[SED] Loaded policies:\n");
        for (int i = 0; i < policy_count; i++) {
            if (policy_names[i])
                fprintf(stdout, "  - %s\n", policy_names[i]);
        }
        security_free_policy_names(policy_names, policy_count);
    }
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
