#ifndef _SERVECOSYS_HAL_H_
#define _SERVECOSYS_HAL_H_

#include <sys/types.h>
#include <linux/limits.h>

#define HAL_MAX_DEVICES 256
#define HAL_NAME_MAX    64

typedef enum {
    HAL_STORAGE,
    HAL_NETWORK,
    HAL_INPUT,
    HAL_DISPLAY,
    HAL_AUDIO,
    HAL_USB,
    HAL_PCI,
    HAL_UNKNOWN,
} hal_dev_class_t;

typedef struct {
    char  devpath[PATH_MAX];
    char  name[HAL_NAME_MAX];
    char  driver[HAL_NAME_MAX];
    hal_dev_class_t class;
    int   major, minor;
    int   claimed;
    char  owner[HAL_NAME_MAX];
} hal_device_t;

typedef struct {
    hal_device_t devices[HAL_MAX_DEVICES];
    int count;
} hal_devtable_t;

const char *hal_class_name(hal_dev_class_t cls);
hal_dev_class_t hal_classify(const char *subsystem);
int  hal_scan_all(hal_devtable_t *table);
int  hal_scan_class(hal_devtable_t *table, hal_dev_class_t cls);
int  hal_claim(hal_devtable_t *table, int index, const char *owner);
int  hal_release(hal_devtable_t *table, int index);
int  hal_find(hal_devtable_t *table, hal_dev_class_t cls, int *indices, int max);

#endif
