// PolandOS — Zestaw widgetow GUI (styl Windows 3.0)
#pragma once
#include "../../include/types.h"

// Widget types
#define WIDGET_BUTTON  0x01
#define WIDGET_LABEL   0x02

void widget_draw_button(i32 x, i32 y, i32 w, i32 h, const char *text, bool pressed);
void widget_draw_label(i32 x, i32 y, const char *text, u32 color, u32 bg);
void widget_draw_raised_panel(i32 x, i32 y, i32 w, i32 h);
void widget_draw_sunken_panel(i32 x, i32 y, i32 w, i32 h);
