// PolandOS — Menedzer okien (styl Windows 3.0)
#pragma once
#include "../../include/types.h"

// ─── Windows 3.0 Color Palette ───────────────────────────────────────────────
#define WIN_COLOR_DESKTOP       0x008080   // Teal desktop
#define WIN_COLOR_TITLEBAR      0x000080   // Dark blue active title
#define WIN_COLOR_TITLEBAR_INV  0x808080   // Gray inactive title
#define WIN_COLOR_TITLE_TEXT    0xFFFFFF   // White title text
#define WIN_COLOR_WINDOW_BG     0xFFFFFF   // White window background
#define WIN_COLOR_BUTTON_FACE   0xC0C0C0   // Light gray button
#define WIN_COLOR_BUTTON_HI     0xFFFFFF   // White highlight
#define WIN_COLOR_BUTTON_SHAD   0x808080   // Gray shadow
#define WIN_COLOR_BUTTON_DARK   0x000000   // Black border
#define WIN_COLOR_MENU_BG       0xFFFFFF   // White menu
#define WIN_COLOR_TEXT           0x000000   // Black text
#define WIN_COLOR_TASKBAR       0xC0C0C0   // Light gray taskbar
#define WIN_COLOR_SELECTED      0x000080   // Blue selection
#define WIN_COLOR_CURSOR        0xFFFFFF   // White cursor

// ─── Dimensions ──────────────────────────────────────────────────────────────
#define WIN_TITLEBAR_H   20
#define WIN_BORDER_W     3
#define WIN_TASKBAR_H    28
#define WIN_CLOSE_BTN_W  16
#define WIN_MAX_WINDOWS  16
#define WIN_MAX_TITLE    48

// ─── Window flags ────────────────────────────────────────────────────────────
#define WIN_FLAG_VISIBLE     0x01
#define WIN_FLAG_ACTIVE      0x02
#define WIN_FLAG_MOVABLE     0x04
#define WIN_FLAG_CLOSEABLE   0x08
#define WIN_FLAG_HAS_MENU    0x10

typedef struct gui_window GUIWindow;

typedef void (*win_paint_fn)(GUIWindow *win);
typedef void (*win_click_fn)(GUIWindow *win, i32 local_x, i32 local_y);
typedef void (*win_key_fn)(GUIWindow *win, char key);

struct gui_window {
    char       title[WIN_MAX_TITLE];
    i32        x, y;
    i32        width, height;
    u32        flags;
    u32        id;
    win_paint_fn  on_paint;
    win_click_fn  on_click;
    win_key_fn    on_key;
    void          *userdata;
};

void       wm_init(void);
GUIWindow *wm_create_window(const char *title, i32 x, i32 y, i32 w, i32 h, u32 flags);
void       wm_destroy_window(GUIWindow *win);
void       wm_bring_to_front(GUIWindow *win);
GUIWindow *wm_window_at(i32 x, i32 y);
int        wm_titlebar_hit(GUIWindow *win, i32 x, i32 y);
int        wm_close_btn_hit(GUIWindow *win, i32 x, i32 y);
void       wm_draw_all(void);
void       wm_draw_window(GUIWindow *win);
int        wm_window_count(void);
GUIWindow *wm_get_active(void);
void       wm_set_active(GUIWindow *win);
