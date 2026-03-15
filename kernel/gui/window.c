// PolandOS — Menedzer okien (styl Windows 3.0)
// Rysowanie okien, ramek, belki tytulow, przyciskow
#include "window.h"
#include "../drivers/fb.h"
#include "../lib/string.h"
#include "../lib/printf.h"

// ─── Window pool ──────────────────────────────────────────────────────────────
static GUIWindow windows[WIN_MAX_WINDOWS];
static int       win_order[WIN_MAX_WINDOWS];  // Z-order: index into windows[]
static int       win_count = 0;
static int       next_id   = 1;
static GUIWindow *active_win = NULL;

// ─── Character drawing (reuse fb's 8×16 font) ────────────────────────────────
// We need a way to draw text at arbitrary pixel positions in the GUI.
// We use fb_putpixel for per-pixel drawing.
extern const u8 font_data[128][16];  // from fb.c

#define FONT_W 8
#define FONT_H 16

static void draw_char_at(i32 px, i32 py, char c, u32 fg, u32 bg) {
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

static void draw_text_at(i32 px, i32 py, const char *text, u32 fg, u32 bg) {
    while (*text) {
        if (px + FONT_W > fb_getwidth()) break;
        draw_char_at(px, py, *text, fg, bg);
        px += FONT_W;
        text++;
    }
}

// ─── 3D-style beveled rectangle (Windows 3.0 look) ───────────────────────────
static void draw_raised_rect(i32 x, i32 y, i32 w, i32 h) {
    // Top highlight
    fb_fill_rect((u32)x, (u32)y, (u32)w, 1, WIN_COLOR_BUTTON_HI);
    // Left highlight
    fb_fill_rect((u32)x, (u32)y, 1, (u32)h, WIN_COLOR_BUTTON_HI);
    // Bottom shadow
    fb_fill_rect((u32)x, (u32)(y + h - 1), (u32)w, 1, WIN_COLOR_BUTTON_DARK);
    fb_fill_rect((u32)x + 1, (u32)(y + h - 2), (u32)(w - 1), 1, WIN_COLOR_BUTTON_SHAD);
    // Right shadow
    fb_fill_rect((u32)(x + w - 1), (u32)y, 1, (u32)h, WIN_COLOR_BUTTON_DARK);
    fb_fill_rect((u32)(x + w - 2), (u32)(y + 1), 1, (u32)(h - 1), WIN_COLOR_BUTTON_SHAD);
}

static void draw_sunken_rect(i32 x, i32 y, i32 w, i32 h) {
    // Top shadow
    fb_fill_rect((u32)x, (u32)y, (u32)w, 1, WIN_COLOR_BUTTON_SHAD);
    // Left shadow
    fb_fill_rect((u32)x, (u32)y, 1, (u32)h, WIN_COLOR_BUTTON_SHAD);
    // Bottom highlight
    fb_fill_rect((u32)x, (u32)(y + h - 1), (u32)w, 1, WIN_COLOR_BUTTON_HI);
    // Right highlight
    fb_fill_rect((u32)(x + w - 1), (u32)y, 1, (u32)h, WIN_COLOR_BUTTON_HI);
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void wm_init(void) {
    memset(windows, 0, sizeof(windows));
    win_count = 0;
    next_id   = 1;
    active_win = NULL;
    kprintf("[DOBRZE] Menedzer okien (Windows 3.0) zainicjalizowany\n");
}

// ─── Create window ────────────────────────────────────────────────────────────
GUIWindow *wm_create_window(const char *title, i32 x, i32 y, i32 w, i32 h, u32 flags) {
    if (win_count >= WIN_MAX_WINDOWS) return NULL;

    GUIWindow *win = &windows[win_count];
    memset(win, 0, sizeof(GUIWindow));
    strncpy(win->title, title, WIN_MAX_TITLE - 1);
    win->title[WIN_MAX_TITLE - 1] = '\0';
    win->x      = x;
    win->y      = y;
    win->width  = w;
    win->height = h;
    win->flags  = flags | WIN_FLAG_VISIBLE;
    win->id     = (u32)next_id++;

    win_order[win_count] = win_count;
    win_count++;
    active_win = win;

    return win;
}

// ─── Destroy window ───────────────────────────────────────────────────────────
void wm_destroy_window(GUIWindow *win) {
    if (!win) return;
    int idx = -1;
    for (int i = 0; i < win_count; i++) {
        if (&windows[i] == win) { idx = i; break; }
    }
    if (idx < 0) return;

    // Shift windows down
    for (int i = idx; i < win_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    win_count--;

    // Rebuild order
    for (int i = 0; i < win_count; i++) win_order[i] = i;

    active_win = (win_count > 0) ? &windows[win_count - 1] : NULL;
}

// ─── Z-order management ──────────────────────────────────────────────────────
void wm_bring_to_front(GUIWindow *win) {
    if (!win) return;
    int idx = -1;
    for (int i = 0; i < win_count; i++) {
        if (&windows[win_order[i]] == win) { idx = i; break; }
    }
    if (idx < 0) return;

    int saved = win_order[idx];
    for (int i = idx; i < win_count - 1; i++)
        win_order[i] = win_order[i + 1];
    win_order[win_count - 1] = saved;
    active_win = win;
}

// ─── Hit testing ──────────────────────────────────────────────────────────────
GUIWindow *wm_window_at(i32 x, i32 y) {
    // Search from top of Z-order (highest index = topmost)
    for (int i = win_count - 1; i >= 0; i--) {
        GUIWindow *w = &windows[win_order[i]];
        if (!(w->flags & WIN_FLAG_VISIBLE)) continue;
        if (x >= w->x && x < w->x + w->width &&
            y >= w->y && y < w->y + w->height) {
            return w;
        }
    }
    return NULL;
}

int wm_titlebar_hit(GUIWindow *win, i32 x, i32 y) {
    if (!win) return 0;
    return (x >= win->x + WIN_BORDER_W &&
            x < win->x + win->width - WIN_BORDER_W &&
            y >= win->y + WIN_BORDER_W &&
            y < win->y + WIN_BORDER_W + WIN_TITLEBAR_H);
}

int wm_close_btn_hit(GUIWindow *win, i32 x, i32 y) {
    if (!win || !(win->flags & WIN_FLAG_CLOSEABLE)) return 0;
    i32 bx = win->x + win->width - WIN_BORDER_W - WIN_CLOSE_BTN_W - 2;
    i32 by = win->y + WIN_BORDER_W + 2;
    return (x >= bx && x < bx + WIN_CLOSE_BTN_W &&
            y >= by && y < by + WIN_CLOSE_BTN_W);
}

// ─── Draw single window ──────────────────────────────────────────────────────
void wm_draw_window(GUIWindow *win) {
    if (!win || !(win->flags & WIN_FLAG_VISIBLE)) return;

    int is_active = (win == active_win);
    i32 x = win->x, y = win->y, w = win->width, h = win->height;

    // Outer border (raised 3D)
    fb_fill_rect((u32)x, (u32)y, (u32)w, (u32)h, WIN_COLOR_BUTTON_FACE);
    draw_raised_rect(x, y, w, h);

    // Title bar background
    u32 title_bg = is_active ? WIN_COLOR_TITLEBAR : WIN_COLOR_TITLEBAR_INV;
    fb_fill_rect((u32)(x + WIN_BORDER_W), (u32)(y + WIN_BORDER_W),
                 (u32)(w - 2 * WIN_BORDER_W), (u32)WIN_TITLEBAR_H,
                 title_bg);

    // Title text (centered vertically in title bar)
    i32 text_x = x + WIN_BORDER_W + 4;
    i32 text_y = y + WIN_BORDER_W + (WIN_TITLEBAR_H - FONT_H) / 2;
    draw_text_at(text_x, text_y, win->title, WIN_COLOR_TITLE_TEXT, title_bg);

    // Close button [X] (if closeable)
    if (win->flags & WIN_FLAG_CLOSEABLE) {
        i32 bx = x + w - WIN_BORDER_W - WIN_CLOSE_BTN_W - 2;
        i32 by = y + WIN_BORDER_W + 2;
        fb_fill_rect((u32)bx, (u32)by, WIN_CLOSE_BTN_W, WIN_CLOSE_BTN_W,
                     WIN_COLOR_BUTTON_FACE);
        draw_raised_rect(bx, by, WIN_CLOSE_BTN_W, WIN_CLOSE_BTN_W);
        // Draw X
        draw_char_at(bx + 4, by, 'x', WIN_COLOR_TEXT, WIN_COLOR_BUTTON_FACE);
    }

    // Client area (white)
    i32 cx = x + WIN_BORDER_W;
    i32 cy = y + WIN_BORDER_W + WIN_TITLEBAR_H;
    i32 cw = w - 2 * WIN_BORDER_W;
    i32 ch = h - 2 * WIN_BORDER_W - WIN_TITLEBAR_H;
    if (cw > 0 && ch > 0) {
        fb_fill_rect((u32)cx, (u32)cy, (u32)cw, (u32)ch, WIN_COLOR_WINDOW_BG);
        draw_sunken_rect(cx, cy, cw, ch);
    }

    // Call custom paint callback
    if (win->on_paint) win->on_paint(win);
}

// ─── Draw all windows in Z-order ─────────────────────────────────────────────
void wm_draw_all(void) {
    for (int i = 0; i < win_count; i++) {
        wm_draw_window(&windows[win_order[i]]);
    }
}

// ─── Queries ──────────────────────────────────────────────────────────────────
int wm_window_count(void) {
    return win_count;
}

GUIWindow *wm_get_active(void) {
    return active_win;
}

void wm_set_active(GUIWindow *win) {
    active_win = win;
}
