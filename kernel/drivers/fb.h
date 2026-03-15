// PolandOS — Sterownik framebuffera i konsoli
#pragma once
#include "../../include/types.h"

// Kolory
#define FB_BLACK   0x000000
#define FB_WHITE   0xFFFFFF
#define FB_RED     0xDC143C
#define FB_GREEN   0x00AA00
#define FB_BLUE    0x0000AA
#define FB_CYAN    0x00AAAA
#define FB_YELLOW  0xAAAA00
#define FB_GRAY    0x888888
#define FB_LGRAY   0xCCCCCC
#define FB_POLISH_WHITE 0xFFFFFF
#define FB_POLISH_RED   0xDC143C

typedef struct {
    u64 addr;
    u32 width;
    u32 height;
    u32 pitch;
    u16 bpp;
} Framebuffer;

void fb_init(u64 addr, u32 width, u32 height, u32 pitch, u16 bpp);
void fb_putpixel(u32 x, u32 y, u32 color);
void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
void fb_putchar(char c);
void fb_puts(const char *s);
void fb_set_color(u32 fg, u32 bg);
void fb_clear(u32 color);
void fb_set_panic_colors(void);
void fb_draw_boot_screen(void);
int  fb_getwidth(void);
int  fb_getheight(void);
