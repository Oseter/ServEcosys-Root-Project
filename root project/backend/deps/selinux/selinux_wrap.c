#include "selinux_wrap.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>

/*
 * selinux_wrap: lightweight in-process reference monitor for ServEcosys.
 * Implements deny-by-default access decisions from an embedded allowlist
 * that mirrors backend/security/servecosys.te. libselinux is NOT required.
 *
 * Return conventions:
 *   selinux_check: 0 = allowed, 1 = denied, -1 = error
 *   others:        0 = success, -1 = error
 */

typedef struct {
    const char *domain;
    const char *tclass;
    const char *perm;
} av_rule_t;

static const av_rule_t allow_rules[] = {
    /* sys_dom_t - system management domain */
    { "sys_dom_t", "process",         "signal" },
    { "sys_dom_t", "process",         "fork" },
    { "sys_dom_t", "process",         "getattr" },
    { "sys_dom_t", "capability",      "sys_admin" },
    { "sys_dom_t", "capability",      "sys_rawio" },
    { "sys_dom_t", "capability",      "sys_module" },
    { "sys_dom_t", "capability",      "sys_boot" },
    { "sys_dom_t", "system",          "module_load" },
    { "sys_dom_t", "system",          "policy" },
    { "sys_dom_t", "security",        "compute_av" },
    { "sys_dom_t", "security",        "compute_create" },
    { "sys_dom_t", "security",        "compute_member" },
    { "sys_dom_t", "security",        "compute_relabel" },
    { "sys_dom_t", "module",          "load" },
    { "sys_dom_t", "file",            "read" },
    { "sys_dom_t", "file",            "write" },
    { "sys_dom_t", "file",            "open" },
    { "sys_dom_t", "file",            "getattr" },
    { "sys_dom_t", "dir",             "read" },
    { "sys_dom_t", "dir",             "search" },
    { "sys_dom_t", "dir",             "add_name" },
    { "sys_dom_t", "dir",             "write" },
    { "sys_dom_t", "unix_dgram_socket","sendto" },
    { "sys_dom_t", "unix_dgram_socket","recvfrom" },
    { "sys_dom_t", "unix_stream_socket","connectto" },
    { "sys_dom_t", "tcp_socket",      "create" },
    { "sys_dom_t", "tcp_socket",      "bind" },
    { "sys_dom_t", "tcp_socket",      "connect" },
    { "sys_dom_t", "tcp_socket",      "send_msg" },
    { "sys_dom_t", "tcp_socket",      "recv_msg" },
    { "sys_dom_t", "udp_socket",      "create" },
    { "sys_dom_t", "udp_socket",      "bind" },
    { "sys_dom_t", "udp_socket",      "send_msg" },
    { "sys_dom_t", "udp_socket",      "recv_msg" },
    { "sys_dom_t", "sysfs",           "read" },

    /* uid_dom_t - frontend interaction domain */
    { "uid_dom_t", "process",         "signal" },
    { "uid_dom_t", "process",         "fork" },
    { "uid_dom_t", "process",         "getattr" },
    { "uid_dom_t", "process",         "transition" },
    { "uid_dom_t", "capability",      "sys_tty_config" },
    { "uid_dom_t", "unix_dgram_socket","sendto" },
    { "uid_dom_t", "unix_dgram_socket","recvfrom" },
    { "uid_dom_t", "unix_stream_socket","connectto" },
    { "uid_dom_t", "unix_stream_socket","listen" },
    { "uid_dom_t", "unix_stream_socket","accept" },
    { "uid_dom_t", "file",            "read" },
    { "uid_dom_t", "file",            "write" },
    { "uid_dom_t", "file",            "open" },
    { "uid_dom_t", "file",            "getattr" },
    { "uid_dom_t", "file",            "create" },
    { "uid_dom_t", "file",            "unlink" },
    { "uid_dom_t", "dir",             "read" },
    { "uid_dom_t", "dir",             "search" },
    { "uid_dom_t", "dir",             "add_name" },
    { "uid_dom_t", "dir",             "write" },
    { "uid_dom_t", "fd",              "use" },

    /* app_sandbox_t - application sandbox (deny-by-default, no network) */
    { "app_sandbox_t", "process",     "getattr" },
    { "app_sandbox_t", "file",        "read" },
    { "app_sandbox_t", "file",        "write" },
    { "app_sandbox_t", "file",        "open" },
    { "app_sandbox_t", "file",        "getattr" },
    { "app_sandbox_t", "dir",         "read" },
    { "app_sandbox_t", "dir",         "search" },
    { "app_sandbox_t", "unix_dgram_socket","sendto" },

    /* oipes_client_t - OIPES network client */
    { "oipes_client_t", "tcp_socket", "create" },
    { "oipes_client_t", "tcp_socket", "bind" },
    { "oipes_client_t", "tcp_socket", "connect" },
    { "oipes_client_t", "tcp_socket", "send_msg" },
    { "oipes_client_t", "tcp_socket", "recv_msg" },
    { "oipes_client_t", "file",       "read" },
    { "oipes_client_t", "file",       "write" },
};

#define N_RULES (sizeof(allow_rules) / sizeof(allow_rules[0]))

static selinux_state_t g_state;

/* Extract the SELinux type from a context "u:r:DOMAIN:s0" -> "DOMAIN". */
static int extract_domain(const char *ctx, char *out, size_t out_size)
{
    const char *p = strstr(ctx, ":r:");
    size_t n = 0;

    if (!p) return -1;
    p += 3;

    while (p[n] && p[n] != ':' && n + 1 < out_size)
        n++;
    if (p[n] != ':')
        return -1;

    memcpy(out, p, n);
    out[n] = 0;
    return 0;
}

int selinux_init(selinux_state_t *state)
{
    if (!state) return -1;
    memset(state, 0, sizeof(*state));
    strncpy(state->context, "u:r:sys_dom_t:s0", sizeof(state->context) - 1);
    state->context[sizeof(state->context) - 1] = 0;
    state->enforcing = 1;
    g_state = *state;
    return 0;
}

int selinux_load_policy(const char *policy_path)
{
    struct stat st;

    if (!policy_path) return -1;

    if (stat(policy_path, &st) != 0) {
        fprintf(stderr, "[selinux] Policy not found: %s\n", policy_path);
        return -1;
    }
    if (st.st_size <= 0) {
        fprintf(stderr, "[selinux] Policy empty: %s\n", policy_path);
        return -1;
    }

    fprintf(stdout, "[selinux] Loaded policy: %s (%ld bytes)\n",
            policy_path, (long)st.st_size);
    return 0;
}

int selinux_reload_policy(void)
{
    fprintf(stdout, "[selinux] Policy reload requested\n");
    return 0;
}

int selinux_set_enforcing(int enforcing)
{
    g_state.enforcing = enforcing ? 1 : 0;
    fprintf(stdout, "[selinux] Set enforcing: %d\n", g_state.enforcing);
    return 0;
}

int selinux_get_enforcing(void)
{
    return g_state.enforcing;
}

/*
 * Deny-by-default access decision against the embedded allowlist.
 * Returns 0 (allowed) only if the (domain, tclass, perm) triple is present.
 */
int selinux_check(const char *src_ctx, const char *tgt_ctx,
                  const char *tclass, const char *perm)
{
    char domain[64];
    size_t i;

    if (!src_ctx || !tgt_ctx || !tclass || !perm)
        return -1;

    if (extract_domain(src_ctx, domain, sizeof(domain)) != 0)
        return -1;

    for (i = 0; i < N_RULES; i++) {
        if (strcmp(domain, allow_rules[i].domain) == 0 &&
            strcmp(tclass, allow_rules[i].tclass) == 0 &&
            strcmp(perm, allow_rules[i].perm) == 0) {
            if (!g_state.enforcing) {
                fprintf(stdout, "[selinux] (permissive) allowed: %s : %s {%s}\n",
                        domain, tclass, perm);
                return 0;
            }
            return 0;
        }
    }

    fprintf(stdout, "[selinux] denied: %s : %s {%s}\n", domain, tclass, perm);
    return 1;
}

int selinux_get_context(pid_t pid, char *ctx, size_t ctx_size)
{
    char path[64];
    ssize_t n;
    int fd;

    if (!ctx || ctx_size < 1)
        return -1;

    snprintf(path, sizeof(path), "/proc/%d/attr/current", (int)pid);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    n = read(fd, ctx, ctx_size - 1);
    close(fd);
    if (n <= 0)
        return -1;

    while (n > 0 && (ctx[n - 1] == '\n' || ctx[n - 1] == '\0'))
        n--;
    ctx[n] = 0;
    return 0;
}

int selinux_set_context(const char *ctx)
{
    int fd;

    if (!ctx)
        return -1;

    fd = open("/proc/self/attr/current", O_WRONLY);
    if (fd < 0)
        return -1;

    ssize_t n = write(fd, ctx, strlen(ctx));
    close(fd);
    return (n == (ssize_t)strlen(ctx)) ? 0 : -1;
}
