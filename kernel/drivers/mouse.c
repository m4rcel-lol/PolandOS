// PolandOS — Sterownik myszy PS/2
// Obsluga myszy przez port 0x60/0x64
#include "mouse.h"
#include "fb.h"
#include "../../include/io.h"
#include "../arch/x86_64/cpu/apic.h"
#include "../arch/x86_64/cpu/idt.h"
#include "../lib/printf.h"

#define MOUSE_DATA_PORT   0x60
#define MOUSE_STATUS_PORT 0x64
#define MOUSE_CMD_PORT    0x64
#define MOUSE_IRQ_VECTOR  44   // IRQ12 → vector 44 (32 + 12)

// ─── Mouse state ──────────────────────────────────────────────────────────────
static volatile i32  mouse_x     = 0;
static volatile i32  mouse_y     = 0;
static volatile bool mouse_left  = false;
static volatile bool mouse_right = false;
static volatile bool mouse_mid   = false;

static volatile bool mouse_event_pending = false;
static volatile MouseState mouse_last_event;

static u8  mouse_cycle = 0;
static i8  mouse_bytes[3];

static int screen_w = 0;
static int screen_h = 0;

// ─── PS/2 controller helpers ──────────────────────────────────────────────────
static void mouse_wait_write(void) {
    int timeout = 100000;
    while (timeout-- > 0) {
        if (!(inb(MOUSE_STATUS_PORT) & 0x02)) return;
        io_wait();
    }
}

static void mouse_wait_read(void) {
    int timeout = 100000;
    while (timeout-- > 0) {
        if (inb(MOUSE_STATUS_PORT) & 0x01) return;
        io_wait();
    }
}

static void mouse_write(u8 data) {
    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0xD4);  // send next byte to mouse
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, data);
}

static u8 mouse_read(void) {
    mouse_wait_read();
    return inb(MOUSE_DATA_PORT);
}

// ─── IRQ Handler ──────────────────────────────────────────────────────────────
void mouse_irq_handler(InterruptFrame *frame) {
    (void)frame;

    u8 status = inb(MOUSE_STATUS_PORT);
    if (!(status & 0x20)) return;  // not from mouse

    i8 data = (i8)inb(MOUSE_DATA_PORT);
    mouse_bytes[mouse_cycle] = data;

    switch (mouse_cycle) {
    case 0:
        // Byte 0: buttons + sign bits + overflow
        if (!(data & 0x08)) {
            // Bit 3 must be set — resynchronize
            mouse_cycle = 0;
            return;
        }
        mouse_cycle = 1;
        break;
    case 1:
        // Byte 1: X movement
        mouse_cycle = 2;
        break;
    case 2: {
        // Byte 2: Y movement — complete packet
        mouse_cycle = 0;

        u8 flags = (u8)mouse_bytes[0];
        i32 dx = mouse_bytes[1];
        i32 dy = mouse_bytes[2];

        // Apply sign extension from flags
        if (flags & 0x10) dx |= (i32)0xFFFFFF00;
        if (flags & 0x20) dy |= (i32)0xFFFFFF00;

        // Discard overflow packets
        if (flags & 0xC0) break;

        // Update position (Y is inverted in PS/2)
        mouse_x += dx;
        mouse_y -= dy;

        // Clamp to screen bounds
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (screen_w > 0 && mouse_x >= screen_w) mouse_x = screen_w - 1;
        if (screen_h > 0 && mouse_y >= screen_h) mouse_y = screen_h - 1;

        // Update button state
        mouse_left  = (flags & 0x01) != 0;
        mouse_right = (flags & 0x02) != 0;
        mouse_mid   = (flags & 0x04) != 0;

        // Store event
        mouse_last_event.x      = mouse_x;
        mouse_last_event.y      = mouse_y;
        mouse_last_event.left   = mouse_left;
        mouse_last_event.right  = mouse_right;
        mouse_last_event.middle = mouse_mid;
        mouse_event_pending = true;
        break;
    }
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────
void mouse_init(void) {
    screen_w = fb_getwidth();
    screen_h = fb_getheight();
    mouse_x = screen_w / 2;
    mouse_y = screen_h / 2;

    // Enable second PS/2 port (mouse)
    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0xA8);

    // Read controller config byte
    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0x20);
    u8 config = mouse_read();

    // Enable IRQ12 and clock for second port
    config |= 0x02;   // enable IRQ12
    config &= ~0x20;  // enable mouse clock

    mouse_wait_write();
    outb(MOUSE_CMD_PORT, 0x60);
    mouse_wait_write();
    outb(MOUSE_DATA_PORT, config);

    // Set defaults
    mouse_write(0xF6);
    mouse_read();  // ACK

    // Enable data reporting
    mouse_write(0xF4);
    mouse_read();  // ACK

    // Register IRQ handler
    idt_register_handler(MOUSE_IRQ_VECTOR, mouse_irq_handler);
    ioapic_unmask_irq(12);

    kprintf("[DOBRZE] Mysz PS/2 zainicjalizowana\n");
}

MouseState mouse_get_state(void) {
    MouseState s;
    s.x      = mouse_x;
    s.y      = mouse_y;
    s.left   = mouse_left;
    s.right  = mouse_right;
    s.middle = mouse_mid;
    return s;
}

bool mouse_poll_event(MouseState *out) {
    if (!mouse_event_pending) return false;
    mouse_event_pending = false;
    *out = mouse_last_event;
    return true;
}
