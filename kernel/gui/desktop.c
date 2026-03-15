// PolandOS — Pulpit (styl Windows 3.0)
// Srodowisko graficzne z paskiem zadan, ikonami i oknami
#include "desktop.h"
#include "window.h"
#include "widget.h"
#include "../drivers/fb.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"
#include "../drivers/timer.h"
#include "../arch/x86_64/mm/pmm.h"
#include "../acpi/acpi.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../../include/io.h"

// ─── Font reference ───────────────────────────────────────────────────────────
extern const u8 font_data[128][16];
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

// ─── Desktop icons ────────────────────────────────────────────────────────────
#define MAX_ICONS 8

typedef struct {
    const char *label;
    i32 x, y;
    i32 icon_w, icon_h;
    void (*on_click)(void);
} DesktopIcon;

static DesktopIcon icons[MAX_ICONS];
static int icon_count = 0;

// ─── Forward declarations ─────────────────────────────────────────────────────
static void open_program_manager(void);
static void open_about_window(void);
static void open_info_window(void);
static void open_shell_window(void);

// ─── Mouse cursor backup ─────────────────────────────────────────────────────
#define CURSOR_W 12
#define CURSOR_H 16
static i32 cursor_old_x = -1, cursor_old_y = -1;

// Standard arrow cursor bitmap (1 = white, 2 = black, 0 = transparent)
static const u8 cursor_bitmap[CURSOR_H][CURSOR_W] = {
    {2,0,0,0,0,0,0,0,0,0,0,0},
    {2,2,0,0,0,0,0,0,0,0,0,0},
    {2,1,2,0,0,0,0,0,0,0,0,0},
    {2,1,1,2,0,0,0,0,0,0,0,0},
    {2,1,1,1,2,0,0,0,0,0,0,0},
    {2,1,1,1,1,2,0,0,0,0,0,0},
    {2,1,1,1,1,1,2,0,0,0,0,0},
    {2,1,1,1,1,1,1,2,0,0,0,0},
    {2,1,1,1,1,1,1,1,2,0,0,0},
    {2,1,1,1,1,1,2,2,2,2,0,0},
    {2,1,1,2,1,1,2,0,0,0,0,0},
    {2,1,2,0,2,1,1,2,0,0,0,0},
    {2,2,0,0,2,1,1,2,0,0,0,0},
    {2,0,0,0,0,2,1,1,2,0,0,0},
    {0,0,0,0,0,2,1,1,2,0,0,0},
    {0,0,0,0,0,0,2,2,0,0,0,0},
};

// ─── Draw icon (simple folder/app rectangle with label) ───────────────────────
static void draw_icon_bitmap(i32 x, i32 y, i32 w, i32 h, u32 color) {
    // Simple folder icon: rectangle with a tab
    fb_fill_rect((u32)x, (u32)(y + 4), (u32)w, (u32)(h - 4), color);
    fb_fill_rect((u32)x, (u32)y, (u32)(w / 2), 4, color);
    // Border
    for (i32 i = 0; i < w; i++) {
        fb_putpixel((u32)(x + i), (u32)(y + 4), WIN_COLOR_BUTTON_DARK);
        fb_putpixel((u32)(x + i), (u32)(y + h - 1), WIN_COLOR_BUTTON_DARK);
    }
    for (i32 i = 4; i < h; i++) {
        fb_putpixel((u32)x, (u32)(y + i), WIN_COLOR_BUTTON_DARK);
        fb_putpixel((u32)(x + w - 1), (u32)(y + i), WIN_COLOR_BUTTON_DARK);
    }
}

static void draw_desktop_icon(DesktopIcon *icon) {
    // Draw icon graphic
    draw_icon_bitmap(icon->x + 8, icon->y, 32, 28, WIN_COLOR_BUTTON_FACE);
    // Draw label centered below icon
    i32 label_len = (i32)strlen(icon->label);
    i32 label_x = icon->x + (icon->icon_w - label_len * FONT_W) / 2;
    i32 label_y = icon->y + 32;
    draw_text_at(label_x, label_y, icon->label, WIN_COLOR_CURSOR, WIN_COLOR_DESKTOP);
}

// ─── Register icon ────────────────────────────────────────────────────────────
static void add_icon(const char *label, i32 x, i32 y, void (*on_click)(void)) {
    if (icon_count >= MAX_ICONS) return;
    DesktopIcon *ic = &icons[icon_count++];
    ic->label = label;
    ic->x = x;
    ic->y = y;
    ic->icon_w = 48;
    ic->icon_h = 48;
    ic->on_click = on_click;
}

// ─── Taskbar ──────────────────────────────────────────────────────────────────
static void draw_taskbar(void) {
    i32 sw = fb_getwidth();
    i32 sh = fb_getheight();
    i32 ty = sh - WIN_TASKBAR_H;

    // Taskbar background
    fb_fill_rect(0, (u32)ty, (u32)sw, WIN_TASKBAR_H, WIN_COLOR_TASKBAR);
    // Top highlight (3D effect)
    fb_fill_rect(0, (u32)ty, (u32)sw, 1, WIN_COLOR_BUTTON_HI);
    fb_fill_rect(0, (u32)(ty + 1), (u32)sw, 1, WIN_COLOR_BUTTON_FACE);

    // "Start" button (raised 3D)
    fb_fill_rect(2, (u32)(ty + 3), 60, (u32)(WIN_TASKBAR_H - 6), WIN_COLOR_BUTTON_FACE);
    // Top/left highlight
    fb_fill_rect(2, (u32)(ty + 3), 60, 1, WIN_COLOR_BUTTON_HI);
    fb_fill_rect(2, (u32)(ty + 3), 1, (u32)(WIN_TASKBAR_H - 6), WIN_COLOR_BUTTON_HI);
    // Bottom/right shadow
    fb_fill_rect(2, (u32)(ty + WIN_TASKBAR_H - 4), 60, 1, WIN_COLOR_BUTTON_SHAD);
    fb_fill_rect(61, (u32)(ty + 3), 1, (u32)(WIN_TASKBAR_H - 6), WIN_COLOR_BUTTON_SHAD);
    // Text
    draw_text_at(10, ty + (WIN_TASKBAR_H - FONT_H) / 2, "Start", WIN_COLOR_TEXT, WIN_COLOR_BUTTON_FACE);

    // Clock on right side
    RTCTime t = rtc_read();
    char clock_str[16];
    // Manual formatting to avoid printf
    clock_str[0] = '0' + t.hours / 10;
    clock_str[1] = '0' + t.hours % 10;
    clock_str[2] = ':';
    clock_str[3] = '0' + t.minutes / 10;
    clock_str[4] = '0' + t.minutes % 10;
    clock_str[5] = '\0';
    i32 clock_x = sw - (5 * FONT_W) - 8;
    // Sunken clock area
    fb_fill_rect((u32)(clock_x - 4), (u32)(ty + 4),
                 (u32)(5 * FONT_W + 8), (u32)(WIN_TASKBAR_H - 8),
                 WIN_COLOR_BUTTON_FACE);
    fb_fill_rect((u32)(clock_x - 4), (u32)(ty + 4),
                 (u32)(5 * FONT_W + 8), 1, WIN_COLOR_BUTTON_SHAD);
    fb_fill_rect((u32)(clock_x - 4), (u32)(ty + 4),
                 1, (u32)(WIN_TASKBAR_H - 8), WIN_COLOR_BUTTON_SHAD);
    draw_text_at(clock_x, ty + (WIN_TASKBAR_H - FONT_H) / 2, clock_str,
                 WIN_COLOR_TEXT, WIN_COLOR_BUTTON_FACE);
}

// ─── Cursor drawing ───────────────────────────────────────────────────────────
static void save_cursor_bg(i32 x, i32 y) {
    // We don't have direct pixel read, so we skip save/restore
    // and rely on full redraws
    cursor_old_x = x;
    cursor_old_y = y;
}

static void draw_cursor(i32 mx, i32 my) {
    save_cursor_bg(mx, my);
    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            u8 p = cursor_bitmap[row][col];
            if (p == 0) continue;
            u32 color = (p == 1) ? WIN_COLOR_CURSOR : WIN_COLOR_BUTTON_DARK;
            fb_putpixel((u32)(mx + col), (u32)(my + row), color);
        }
    }
}

// ─── Start menu ───────────────────────────────────────────────────────────────
static int start_menu_open = 0;

static void draw_start_menu(void) {
    i32 sh = fb_getheight();
    i32 menu_x = 2;
    i32 menu_y = sh - WIN_TASKBAR_H - 140;
    i32 menu_w = 160;
    i32 menu_h = 140;

    fb_fill_rect((u32)menu_x, (u32)menu_y, (u32)menu_w, (u32)menu_h, WIN_COLOR_BUTTON_FACE);
    // 3D border
    fb_fill_rect((u32)menu_x, (u32)menu_y, (u32)menu_w, 1, WIN_COLOR_BUTTON_HI);
    fb_fill_rect((u32)menu_x, (u32)menu_y, 1, (u32)menu_h, WIN_COLOR_BUTTON_HI);
    fb_fill_rect((u32)menu_x, (u32)(menu_y + menu_h - 1), (u32)menu_w, 1, WIN_COLOR_BUTTON_SHAD);
    fb_fill_rect((u32)(menu_x + menu_w - 1), (u32)menu_y, 1, (u32)menu_h, WIN_COLOR_BUTTON_SHAD);

    // Blue side bar (Windows 3.0 style)
    fb_fill_rect((u32)(menu_x + 2), (u32)(menu_y + 2), 24, (u32)(menu_h - 4), WIN_COLOR_TITLEBAR);

    // Menu items
    i32 item_x = menu_x + 30;
    i32 item_y = menu_y + 8;
    draw_text_at(item_x, item_y, "Programy",      WIN_COLOR_TEXT, WIN_COLOR_BUTTON_FACE);
    item_y += 24;
    draw_text_at(item_x, item_y, "Informacje",    WIN_COLOR_TEXT, WIN_COLOR_BUTTON_FACE);
    item_y += 24;
    draw_text_at(item_x, item_y, "Powloka",       WIN_COLOR_TEXT, WIN_COLOR_BUTTON_FACE);
    item_y += 24;
    // Separator
    fb_fill_rect((u32)(menu_x + 4), (u32)(item_y + 4), (u32)(menu_w - 8), 1, WIN_COLOR_BUTTON_SHAD);
    fb_fill_rect((u32)(menu_x + 4), (u32)(item_y + 5), (u32)(menu_w - 8), 1, WIN_COLOR_BUTTON_HI);
    item_y += 14;
    draw_text_at(item_x, item_y, "Wylacz...",     WIN_COLOR_TEXT, WIN_COLOR_BUTTON_FACE);
}

static int start_menu_hit(i32 mx, i32 my) {
    i32 sh = fb_getheight();
    i32 menu_x = 2;
    i32 menu_y = sh - WIN_TASKBAR_H - 140;
    i32 menu_w = 160;

    if (mx < menu_x || mx >= menu_x + menu_w) return -1;

    i32 item_y = menu_y + 8;
    if (my >= item_y && my < item_y + 20) return 0; // Programy
    item_y += 24;
    if (my >= item_y && my < item_y + 20) return 1; // Informacje
    item_y += 24;
    if (my >= item_y && my < item_y + 20) return 2; // Powloka
    item_y += 38;
    if (my >= item_y && my < item_y + 20) return 3; // Wylacz

    return -1;
}

// ─── Program Manager ──────────────────────────────────────────────────────────
static void pm_paint(GUIWindow *win) {
    i32 cx = win->x + WIN_BORDER_W + 1;
    i32 cy = win->y + WIN_BORDER_W + WIN_TITLEBAR_H + 1;

    // Draw program icons inside the window
    draw_icon_bitmap(cx + 20, cy + 10, 32, 28, 0xFFFF00);
    draw_text_at(cx + 12, cy + 42, "O syst.", WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);

    draw_icon_bitmap(cx + 90, cy + 10, 32, 28, 0x00AAFF);
    draw_text_at(cx + 82, cy + 42, "Pamiec", WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);

    draw_icon_bitmap(cx + 160, cy + 10, 32, 28, 0x00CC00);
    draw_text_at(cx + 148, cy + 42, "Powloka", WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);
}

static void pm_click(GUIWindow *win, i32 lx, i32 ly) {
    // Detect which icon was clicked
    if (lx >= 20 && lx < 52 && ly >= 10 && ly < 58) {
        open_about_window();
    } else if (lx >= 90 && lx < 122 && ly >= 10 && ly < 58) {
        open_info_window();
    } else if (lx >= 160 && lx < 192 && ly >= 10 && ly < 58) {
        open_shell_window();
    }
}

static void open_program_manager(void) {
    GUIWindow *pm = wm_create_window("Program Manager", 50, 50, 280, 120,
        WIN_FLAG_MOVABLE | WIN_FLAG_CLOSEABLE);
    if (pm) {
        pm->on_paint = pm_paint;
        pm->on_click = pm_click;
    }
}

// ─── About window ─────────────────────────────────────────────────────────────
static void about_paint(GUIWindow *win) {
    i32 cx = win->x + WIN_BORDER_W + 4;
    i32 cy = win->y + WIN_BORDER_W + WIN_TITLEBAR_H + 4;
    draw_text_at(cx, cy,      "PolandOS v0.0.1",      WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);
    draw_text_at(cx, cy + 18, "Jadro: Orzel",          WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);
    draw_text_at(cx, cy + 36, "Architektura: x86_64",  WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);
    draw_text_at(cx, cy + 54, "Pulpit: Windows 3.0",   WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);
    draw_text_at(cx, cy + 72, "Dla Polski. Dla kodu.", WIN_COLOR_TITLEBAR, WIN_COLOR_WINDOW_BG);
}

static void open_about_window(void) {
    wm_create_window("O systemie PolandOS", 100, 80, 240, 130,
        WIN_FLAG_MOVABLE | WIN_FLAG_CLOSEABLE);
    GUIWindow *w = wm_get_active();
    if (w) w->on_paint = about_paint;
}

// ─── Info (memory) window ─────────────────────────────────────────────────────
static void info_paint(GUIWindow *win) {
    i32 cx = win->x + WIN_BORDER_W + 4;
    i32 cy = win->y + WIN_BORDER_W + WIN_TITLEBAR_H + 4;

    u64 total = pmm_total_bytes() / (1024 * 1024);
    u64 used  = pmm_used_bytes()  / (1024 * 1024);
    u64 free  = pmm_free_bytes()  / (1024 * 1024);
    u64 uptime = kernel_ticks / 1000;

    char buf[64];

    draw_text_at(cx, cy, "Informacje o systemie:", WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);

    strcpy(buf, "Pamiec: ");
    char num[16];
    utoa(total, num, 10);
    strcat(buf, num);
    strcat(buf, " MB");
    draw_text_at(cx, cy + 20, buf, WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);

    strcpy(buf, "Uzyte:  ");
    utoa(used, num, 10);
    strcat(buf, num);
    strcat(buf, " MB");
    draw_text_at(cx, cy + 38, buf, WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);

    strcpy(buf, "Wolne:  ");
    utoa(free, num, 10);
    strcat(buf, num);
    strcat(buf, " MB");
    draw_text_at(cx, cy + 56, buf, WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);

    strcpy(buf, "Uptime: ");
    utoa(uptime, num, 10);
    strcat(buf, num);
    strcat(buf, " sek.");
    draw_text_at(cx, cy + 74, buf, WIN_COLOR_TEXT, WIN_COLOR_WINDOW_BG);
}

static void open_info_window(void) {
    wm_create_window("Informacje", 120, 100, 260, 130,
        WIN_FLAG_MOVABLE | WIN_FLAG_CLOSEABLE);
    GUIWindow *w = wm_get_active();
    if (w) w->on_paint = info_paint;
}

// ─── Shell window (placeholder) ───────────────────────────────────────────────
static void shell_paint(GUIWindow *win) {
    i32 cx = win->x + WIN_BORDER_W + 4;
    i32 cy = win->y + WIN_BORDER_W + WIN_TITLEBAR_H + 4;

    // Black terminal background
    fb_fill_rect((u32)(win->x + WIN_BORDER_W + 1),
                 (u32)(win->y + WIN_BORDER_W + WIN_TITLEBAR_H + 1),
                 (u32)(win->width - 2 * WIN_BORDER_W - 2),
                 (u32)(win->height - 2 * WIN_BORDER_W - WIN_TITLEBAR_H - 2),
                 0x000000);
    draw_text_at(cx, cy,      "PolandOS Powloka v0.0.1", 0x00CC00, 0x000000);
    draw_text_at(cx, cy + 18, "Wpisz 'pomoc' aby uzyskac", 0x00CC00, 0x000000);
    draw_text_at(cx, cy + 36, "pomoc.", 0x00CC00, 0x000000);
    draw_text_at(cx, cy + 56, "PolandOS> _", 0xFFFF00, 0x000000);
}

static void open_shell_window(void) {
    wm_create_window("Powloka", 80, 60, 320, 150,
        WIN_FLAG_MOVABLE | WIN_FLAG_CLOSEABLE);
    GUIWindow *w = wm_get_active();
    if (w) w->on_paint = shell_paint;
}

// ─── Full desktop redraw ─────────────────────────────────────────────────────
static void desktop_redraw(void) {
    i32 sw = fb_getwidth();
    i32 sh = fb_getheight();

    // Desktop background (teal — Windows 3.0)
    fb_fill_rect(0, 0, (u32)sw, (u32)(sh - WIN_TASKBAR_H), WIN_COLOR_DESKTOP);

    // Desktop icons
    for (int i = 0; i < icon_count; i++) {
        draw_desktop_icon(&icons[i]);
    }

    // Windows
    wm_draw_all();

    // Taskbar
    draw_taskbar();

    // Start menu if open
    if (start_menu_open) draw_start_menu();
}

// ─── Desktop click handling ───────────────────────────────────────────────────
static int dragging = 0;
static i32 drag_off_x = 0, drag_off_y = 0;
static GUIWindow *drag_win = NULL;

static void handle_click(i32 mx, i32 my, bool left_pressed) {
    i32 sh = fb_getheight();

    if (!left_pressed) {
        dragging = 0;
        drag_win = NULL;
        return;
    }

    // Start menu handling
    if (start_menu_open) {
        int item = start_menu_hit(mx, my);
        if (item >= 0) {
            start_menu_open = 0;
            switch (item) {
            case 0: open_program_manager(); break;
            case 1: open_about_window(); break;
            case 2: open_shell_window(); break;
            case 3: acpi_power_off(); break;
            }
            return;
        }
        // Click outside menu closes it
        start_menu_open = 0;
        return;
    }

    // Start button click
    if (my >= sh - WIN_TASKBAR_H && mx >= 2 && mx < 62) {
        start_menu_open = !start_menu_open;
        return;
    }

    // Taskbar click (ignore)
    if (my >= sh - WIN_TASKBAR_H) return;

    // Window click
    GUIWindow *hit = wm_window_at(mx, my);
    if (hit) {
        wm_bring_to_front(hit);

        // Close button
        if (wm_close_btn_hit(hit, mx, my)) {
            wm_destroy_window(hit);
            return;
        }

        // Title bar drag
        if (wm_titlebar_hit(hit, mx, my) && (hit->flags & WIN_FLAG_MOVABLE)) {
            dragging = 1;
            drag_win = hit;
            drag_off_x = mx - hit->x;
            drag_off_y = my - hit->y;
            return;
        }

        // Client area click
        if (hit->on_click) {
            i32 local_x = mx - hit->x - WIN_BORDER_W;
            i32 local_y = my - hit->y - WIN_BORDER_W - WIN_TITLEBAR_H;
            hit->on_click(hit, local_x, local_y);
        }
        return;
    }

    // Desktop icon click
    for (int i = 0; i < icon_count; i++) {
        DesktopIcon *ic = &icons[i];
        if (mx >= ic->x && mx < ic->x + ic->icon_w &&
            my >= ic->y && my < ic->y + ic->icon_h) {
            if (ic->on_click) ic->on_click();
            return;
        }
    }
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void desktop_init(void) {
    wm_init();

    // Register desktop icons
    add_icon("Programy", 10, 20, open_program_manager);
    add_icon("O syst.", 10, 90, open_about_window);
    add_icon("Powloka", 10, 160, open_shell_window);
    add_icon("System", 10, 230, open_info_window);

    kprintf("[DOBRZE] Pulpit Windows 3.0 zainicjalizowany\n");
}

// ─── Main GUI event loop ──────────────────────────────────────────────────────
void desktop_run(void) {
    desktop_init();

    // Open Program Manager by default (like Windows 3.0)
    open_program_manager();

    MouseState ms;
    MouseState prev_ms = mouse_get_state();
    u64 last_redraw = 0;
    bool needs_redraw = true;

    while (1) {
        // Poll mouse events
        if (mouse_poll_event(&ms)) {
            // Handle dragging
            if (dragging && drag_win) {
                drag_win->x = ms.x - drag_off_x;
                drag_win->y = ms.y - drag_off_y;
                needs_redraw = true;
            }

            // Handle click (left button edge detection)
            if (ms.left && !prev_ms.left) {
                handle_click(ms.x, ms.y, true);
                needs_redraw = true;
            }
            if (!ms.left && prev_ms.left) {
                handle_click(ms.x, ms.y, false);
            }

            prev_ms = ms;
            needs_redraw = true;  // mouse moved
        }

        // Check for keyboard input (Escape to close active window)
        char key;
        if (kb_poll(&key)) {
            if (key == 0x1B) {
                // Escape — close active window
                GUIWindow *aw = wm_get_active();
                if (aw && (aw->flags & WIN_FLAG_CLOSEABLE)) {
                    wm_destroy_window(aw);
                    needs_redraw = true;
                }
            } else {
                GUIWindow *aw = wm_get_active();
                if (aw && aw->on_key) {
                    aw->on_key(aw, key);
                    needs_redraw = true;
                }
            }
        }

        // Periodic redraw for clock update (every ~1 second)
        u64 now = timer_get_ticks();
        if (now - last_redraw >= 1000) {
            needs_redraw = true;
            last_redraw = now;
        }

        // Redraw if needed
        if (needs_redraw) {
            desktop_redraw();
            ms = mouse_get_state();
            draw_cursor(ms.x, ms.y);
            needs_redraw = false;
            last_redraw = now;
        }

        cpu_relax();
    }
}
