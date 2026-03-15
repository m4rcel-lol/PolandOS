// PolandOS — Sterownik myszy PS/2
#pragma once
#include "../../include/types.h"
#include "../arch/x86_64/cpu/idt.h"

typedef struct {
    i32  x;
    i32  y;
    bool left;
    bool right;
    bool middle;
} MouseState;

void mouse_init(void);
void mouse_irq_handler(InterruptFrame *frame);
MouseState mouse_get_state(void);
bool mouse_poll_event(MouseState *out);
