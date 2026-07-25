#ifndef _SERVECOSYS_PERM_H_
#define _SERVECOSYS_PERM_H_

#include <sys/types.h>
#include <stdint.h>

typedef enum {
    PERM_READONLY       = 0,
    PERM_SANDBOX        = 1,
    PERM_USER           = 2,
    PERM_DEBUG          = 3,
    PERM_BL_UNLOCK      = 4,
    PERM_ROOT_SPLIT     = 5,
    PERM_MODULE_ROOT    = 6,
    PERM_KERNEL_ROOT    = 7,
    PERM_SELINUX        = 8,
    PERM_KMOD_LOAD      = 9,
    PERM_CUSTOM_KERNEL  = 10,
    PERM_BOOTLOADER     = 11,
} perm_level_t;

typedef struct {
    pid_t    pid;
    perm_level_t level;
    int      is_signed;
    int      is_self_signed;
} perm_cred_t;

const char *perm_level_name(perm_level_t level);
int  perm_check(perm_level_t current, perm_level_t required);
int  perm_cred_init(perm_cred_t *cred, pid_t pid, perm_level_t level);
int  perm_cred_set_level(perm_cred_t *cred, perm_level_t level);
perm_level_t perm_cred_get_level(const perm_cred_t *cred);
int  perm_can_set_level(perm_level_t current, perm_level_t target, int is_self_signed);

#endif
