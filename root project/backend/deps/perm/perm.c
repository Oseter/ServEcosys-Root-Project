#include "perm.h"
#include <string.h>
#include <stdio.h>

static const char *level_names[] = {
    [PERM_READONLY]      = "readonly",
    [PERM_SANDBOX]       = "sandbox",
    [PERM_USER]          = "user",
    [PERM_DEBUG]         = "debug",
    [PERM_BL_UNLOCK]     = "bl_unlock",
    [PERM_ROOT_SPLIT]    = "root_split",
    [PERM_MODULE_ROOT]   = "module_root",
    [PERM_KERNEL_ROOT]   = "kernel_root",
    [PERM_SELINUX]       = "selinux_control",
    [PERM_KMOD_LOAD]     = "kmod_load",
    [PERM_CUSTOM_KERNEL] = "custom_kernel",
    [PERM_BOOTLOADER]    = "bootloader",
};

const char *perm_level_name(perm_level_t level) {
    if (level < 0 || level > PERM_BOOTLOADER)
        return "unknown";
    return level_names[level];
}

int perm_check(perm_level_t current, perm_level_t required) {
    return (current >= required) ? 0 : -1;
}

int perm_cred_init(perm_cred_t *cred, pid_t pid, perm_level_t level) {
    if (!cred) return -1;
    cred->pid = pid;
    cred->level = level;
    cred->is_signed = 0;
    cred->is_self_signed = 0;
    return 0;
}

int perm_cred_set_level(perm_cred_t *cred, perm_level_t level) {
    if (!cred || level > PERM_BOOTLOADER) return -1;
    if (!perm_can_set_level(cred->level, level, cred->is_self_signed))
        return -1;
    cred->level = level;
    return 0;
}

perm_level_t perm_cred_get_level(const perm_cred_t *cred) {
    return cred ? cred->level : PERM_SANDBOX;
}

int perm_can_set_level(perm_level_t current, perm_level_t target, int is_self_signed) {
    if (target <= current) return 1;
    if (target == PERM_SELINUX || target == PERM_BOOTLOADER) return 1;
    if (target <= PERM_DEBUG) return 1;
    if (target <= PERM_ROOT_SPLIT && is_self_signed) return 1;
    return 0;
}
