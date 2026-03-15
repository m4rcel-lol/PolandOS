// PolandOS — Zestaw widgetow GUI (styl Windows 3.0)
// Przyciski, etykiety, panele
#include "widget.h"
#include "window.h"
#include "../drivers/fb.h"
#include "../lib/string.h"

extern const u8 font_data[128][16];
#define FONT_W 8
#define FONT_H 16

static void widget_draw_char(i32 px, i32 py, char c, u32 fg, u32 bg) {
    u8 idx = (u8)c;
    if (idx > 0x7F) idx = '?';
    const u8 *glyph = font_data[idx];
    for (int row = 0; row < FONT_H; row++) {
        u8 bits = glyph[row];
        for (int col = 0; col < FONT_W; col++) {
            u32 color = (bits & (0x80 >> col)) ? fg : bg;
            fb_putpixel((u32)(px + col), (u32)(py + row), color);
        }
    }
}

static void widget_draw_text(i32 px, i32 py, const char *text, u32 fg, u32 bg) {
    while (*text) {
        widget_draw_char(px, py, *text, fg, bg);
        px += FONT_W;
        text++;
    }
}

// ─── Raised panel (3D outward effect) ─────────────────────────────────────────
void widget_draw_raised_panel(i32 x, i32 y, i32 w, i32 h) {
    fb_fill_rect((u32)x, (u32)y, (u32)w, (u32)h, WIN_COLOR_BUTTON_FACE);
    // Highlight
    fb_fill_rect((u32)x, (u32)y, (u32)w, 1, WIN_COLOR_BUTTON_HI);
    fb_fill_rect((u32)x, (u32)y, 1, (u32)h, WIN_COLOR_BUTTON_HI);
    // Shadow
    fb_fill_rect((u32)x, (u32)(y + h - 1), (u32)w, 1, WIN_COLOR_BUTTON_DARK);
    fb_fill_rect((u32)(x + w - 1), (u32)y, 1, (u32)h, WIN_COLOR_BUTTON_DARK);
    fb_fill_rect((u32)(x + 1), (u32)(y + h - 2), (u32)(w - 2), 1, WIN_COLOR_BUTTON_SHAD);
    fb_fill_rect((u32)(x + w - 2), (u32)(y + 1), 1, (u32)(h - 2), WIN_COLOR_BUTTON_SHAD);
}

// ─── Sunken panel (3D inward effect) ──────────────────────────────────────────
void widget_draw_sunken_panel(i32 x, i32 y, i32 w, i32 h) {
    fb_fill_rect((u32)x, (u32)y, (u32)w, (u32)h, WIN_COLOR_WINDOW_BG);
    fb_fill_rect((u32)x, (u32)y, (u32)w, 1, WIN_COLOR_BUTTON_SHAD);
    fb_fill_rect((u32)x, (u32)y, 1, (u32)h, WIN_COLOR_BUTTON_SHAD);
    fb_fill_rect((u32)x, (u32)(y + h - 1), (u32)w, 1, WIN_COLOR_BUTTON_HI);
    fb_fill_rect((u32)(x + w - 1), (u32)y, 1, (u32)h, WIN_COLOR_BUTTON_HI);
}

// ─── Button ───────────────────────────────────────────────────────────────────
void widget_draw_button(i32 x, i32 y, i32 w, i32 h, const char *text, bool pressed) {
    if (pressed) {
        widget_draw_sunken_panel(x, y, w, h);
    } else {
        widget_draw_raised_panel(x, y, w, h);
    }

    // Center text
    i32 text_len = (i32)strlen(text);
    i32 tx = x + (w - text_len * FONT_W) / 2;
    i32 ty = y + (h - FONT_H) / 2;
    u32 bg = pressed ? WIN_COLOR_WINDOW_BG : WIN_COLOR_BUTTON_FACE;
    widget_draw_text(tx + (pressed ? 1 : 0), ty + (pressed ? 1 : 0),
                     text, WIN_COLOR_TEXT, bg);
}

// ─── Label ────────────────────────────────────────────────────────────────────
void widget_draw_label(i32 x, i32 y, const char *text, u32 color, u32 bg) {
    widget_draw_text(x, y, text, color, bg);
}
