#ifndef _SERVECOSYS_DISPLAY_H_
#define _SERVECOSYS_DISPLAY_H_

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int   fd;
    int   width;
    int   height;
    int   bpp;
    int   stride;
    size_t screensize;
    uint8_t *buffer;
} display_t;

int  display_open(display_t *disp, const char *devpath);
void display_close(display_t *disp);
void display_fill_rect(display_t *disp, int x, int y, int w, int h, uint32_t color);
void display_clear(display_t *disp, uint32_t color);
void display_draw_pixel(display_t *disp, int x, int y, uint32_t color);
int  display_set_variable(display_t *disp, int width, int height, int bpp);

#endif
