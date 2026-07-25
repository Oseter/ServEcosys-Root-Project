#ifndef _SERVECOSYS_INPUT_H_
#define _SERVECOSYS_INPUT_H_

#include <sys/types.h>
#include <stdint.h>

#define INPUT_MAX_DEVICES 32
#define INPUT_NAME_MAX    64

typedef enum {
    INPUT_KEYBOARD,
    INPUT_MOUSE,
    INPUT_TOUCH,
    INPUT_STYLUS,
    INPUT_GAMEPAD,
    INPUT_UNKNOWN,
} input_dev_type_t;

typedef struct {
    int     type;
    int     device_id;
    int     code;
    int     value;
    int     abs_x;
    int     abs_y;
    int     pressure;
    uint64_t timestamp_ms;
} input_event_t;

typedef struct {
    int     fd;
    char    name[INPUT_NAME_MAX];
    char    devpath[256];
    input_dev_type_t type;
    int     active;
} input_device_t;

typedef struct {
    input_device_t devices[INPUT_MAX_DEVICES];
    int count;
} input_devtable_t;

int  input_discover(input_devtable_t *table);
int  input_open_device(input_device_t *dev, const char *devpath);
void input_close_device(input_device_t *dev);
int  input_read_event(input_device_t *dev, input_event_t *ev);
input_dev_type_t input_classify(const char *name, unsigned long ev_bits);

#endif
