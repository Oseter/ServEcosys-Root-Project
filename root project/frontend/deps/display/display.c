#include "display.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

int display_open(display_t *disp, const char *devpath) {
    if (!disp || !devpath) return -1;
    memset(disp, 0, sizeof(*disp));

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    disp->fd = open(devpath, O_RDWR);
    if (disp->fd < 0) {
        fprintf(stderr, "[display] Failed to open %s: %s\n", devpath, strerror(errno));
        return -1;
    }

    if (ioctl(disp->fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(disp->fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        close(disp->fd);
        return -1;
    }

    disp->width      = vinfo.xres;
    disp->height     = vinfo.yres;
    disp->bpp        = vinfo.bits_per_pixel;
    disp->stride     = finfo.line_length;
    disp->screensize = finfo.smem_len;

    disp->buffer = mmap(NULL, disp->screensize,
                        PROT_READ | PROT_WRITE, MAP_SHARED,
                        disp->fd, 0);
    if (disp->buffer == MAP_FAILED) {
        close(disp->fd);
        disp->buffer = NULL;
        return -1;
    }

    return 0;
}

void display_close(display_t *disp) {
    if (!disp) return;
    if (disp->buffer) munmap(disp->buffer, disp->screensize);
    if (disp->fd >= 0) close(disp->fd);
    memset(disp, 0, sizeof(*disp));
}

void display_fill_rect(display_t *disp, int x, int y, int w, int h, uint32_t color) {
    if (!disp || !disp->buffer) return;
    int bpp = disp->bpp / 8;

    for (int row = y; row < y + h && row < disp->height; row++) {
        uint8_t *line = disp->buffer + row * disp->stride + x * bpp;
        for (int col = 0; col < w && (x + col) < disp->width; col++) {
            line[col * bpp]     = color & 0xFF;
            line[col * bpp + 1] = (color >> 8) & 0xFF;
            line[col * bpp + 2] = (color >> 16) & 0xFF;
            if (bpp == 4)
                line[col * bpp + 3] = (color >> 24) & 0xFF;
        }
    }
}

void display_clear(display_t *disp, uint32_t color) {
    if (disp)
        display_fill_rect(disp, 0, 0, disp->width, disp->height, color);
}

void display_draw_pixel(display_t *disp, int x, int y, uint32_t color) {
    if (!disp || !disp->buffer) return;
    if (x < 0 || x >= disp->width || y < 0 || y >= disp->height) return;
    int bpp = disp->bpp / 8;
    uint8_t *p = disp->buffer + y * disp->stride + x * bpp;
    p[0] = color & 0xFF; p[1] = (color >> 8) & 0xFF;
    p[2] = (color >> 16) & 0xFF;
    if (bpp == 4) p[3] = (color >> 24) & 0xFF;
}

int display_set_variable(display_t *disp, int width, int height, int bpp) {
    if (!disp || disp->fd < 0) return -1;
    struct fb_var_screeninfo vinfo;
    if (ioctl(disp->fd, FBIOGET_VSCREENINFO, &vinfo) < 0) return -1;
    vinfo.xres = width; vinfo.yres = height;
    vinfo.bits_per_pixel = bpp;
    if (ioctl(disp->fd, FBIOPUT_VSCREENINFO, &vinfo) < 0) return -1;
    return display_open(disp, "/dev/fb0");
}
