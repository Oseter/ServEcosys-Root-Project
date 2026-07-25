#ifndef _SERVECOSYS_SELINUX_WRAP_H_
#define _SERVECOSYS_SELINUX_WRAP_H_

typedef struct {
    char context[256];
    int  enforcing;
} selinux_state_t;

int  selinux_init(selinux_state_t *state);
int  selinux_load_policy(const char *policy_path);
int  selinux_reload_policy(void);
int  selinux_set_enforcing(int enforcing);
int  selinux_get_enforcing(void);
int  selinux_check(const char *src_ctx, const char *tgt_ctx,
                   const char *tclass, const char *perm);
int  selinux_get_context(pid_t pid, char *ctx, size_t ctx_size);
int  selinux_set_context(const char *ctx);

#endif
