// PolandOS — Sterownik klawiatury PS/2 (Scancode Set 1)
#include "keyboard.h"
#include "../../include/io.h"
#include "../arch/x86_64/cpu/apic.h"
#include "../arch/x86_64/cpu/idt.h"

#define KB_DATA_PORT   0x60
#define KB_STATUS_PORT 0x64
#define KB_IRQ_VECTOR  33   // IRQ1 → vector 33 (32 + 1)

// ─── Scancode Set 1 → ASCII translation tables ───────────────────────────────
static const char scan_to_ascii[128] = {
    0,    0x1B, '1',  '2',  '3',  '4',  '5',  '6',   // 0x00–0x07
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',   // 0x08–0x0F
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',    // 0x10–0x17
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',    // 0x18–0x1F
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',    // 0x20–0x27
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',    // 0x28–0x2F
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',    // 0x30–0x37
    0,    ' ',  0,    0,    0,    0,    0,    0,      // 0x38–0x3F
    0,    0,    0,    0,    0,    0,    0,    '7',    // 0x40–0x47
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',    // 0x48–0x4F
    '2',  '3',  '0',  '.',  0,    0,    0,    0,      // 0x50–0x57
    0,    0,    0,    0,    0,    0,    0,    0,      // 0x58–0x5F
    0,    0,    0,    0,    0,    0,    0,    0,      // 0x60–0x67
    0,    0,    0,    0,    0,    0,    0,    0,      // 0x68–0x6F
    0,    0,    0,    0,    0,    0,    0,    0,      // 0x70–0x77
    0,    0,    0,    0,    0,    0,    0,    0,      // 0x78–0x7F
};

static const char scan_to_ascii_shift[128] = {
    0,    0x1B, '!',  '@',  '#',  '$',  '%',  '^',   // 0x00–0x07
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',   // 0x08–0x0F
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',    // 0x10–0x17
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',    // 0x18–0x1F
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',    // 0x20–0x27
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',    // 0x28–0x2F
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',    // 0x30–0x37
    0,    ' ',  0,    0,    0,    0,    0,    0,      // 0x38–0x3F
    0,    0,    0,    0,    0,    0,    0,    '7',    // 0x40–0x47
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',    // 0x48–0x4F
    '2',  '3',  '0',  '.',  0,    0,    0,    0,      // 0x50–0x57
    0,    0,    0,    0,    0,    0,    0,    0,      // 0x58–0x7F
    [0x58 ... 0x7F] = 0
};

// ─── Ring buffer ──────────────────────────────────────────────────────────────
static volatile char kb_buf[KB_BUFFER_SIZE];
static volatile u32  kb_head = 0;
static volatile u32  kb_tail = 0;

static int  shift_held  = 0;
static int  caps_lock   = 0;
static int  e0_prefix   = 0; // extended scancode prefix

static void kb_push(char c) {
    u32 next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_tail) {
        kb_buf[kb_head] = c;
        kb_head = next;
    }
}

static int kb_pop(char *c) {
    if (kb_tail == kb_head) return 0;
    *c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return 1;
}

// ─── IRQ Handler (called from IDT common stub, which handles EOI) ─────────────
void kb_irq_handler(InterruptFrame *frame) {
    (void)frame;
    u8 sc = inb(KB_DATA_PORT);

    // Extended prefix
    if (sc == 0xE0) { e0_prefix = 1; return; }

    int released = (sc & 0x80) != 0;
    u8  key = sc & 0x7F;

    if (e0_prefix) {
        e0_prefix = 0;
        if (!released) {
            // Arrow keys (extended)
            if (key == 0x48) { kb_push('\x1B'); kb_push('['); kb_push('A'); } // Up
            if (key == 0x50) { kb_push('\x1B'); kb_push('['); kb_push('B'); } // Down
            if (key == 0x4D) { kb_push('\x1B'); kb_push('['); kb_push('C'); } // Right
            if (key == 0x4B) { kb_push('\x1B'); kb_push('['); kb_push('D'); } // Left
            if (key == 0x1C) kb_push('\n'); // numpad enter
        }
        return;
    }

    // Shift keys: left=0x2A, right=0x36
    if (key == 0x2A || key == 0x36) {
        shift_held = !released;
        return;
    }

    // Caps Lock: 0x3A (toggle on press)
    if (key == 0x3A && !released) {
        caps_lock = !caps_lock;
        return;
    }

    // Only handle key-press events for printable chars
    if (!released && key < 128) {
        char c;
        if (shift_held) {
            c = scan_to_ascii_shift[key];
        } else {
            c = scan_to_ascii[key];
            // Apply caps lock to letters
            if (caps_lock && c >= 'a' && c <= 'z') c -= 32;
        }
        if (c) kb_push(c);
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────
void kb_init(void) {
    // Disable PS/2 devices while configuring
    while (inb(KB_STATUS_PORT) & 0x02) io_wait();
    outb(KB_STATUS_PORT, 0xAD); // disable first PS/2 port
    io_wait();
    outb(KB_STATUS_PORT, 0xA7); // disable second PS/2 port (if present)
    io_wait();

    // Flush any stale bytes in the output buffer
    while (inb(KB_STATUS_PORT) & 0x01) inb(KB_DATA_PORT);

    // Re-enable first PS/2 port (keyboard)
    while (inb(KB_STATUS_PORT) & 0x02) io_wait();
    outb(KB_STATUS_PORT, 0xAE);
    io_wait();

    // Register IRQ handler in IDT (vector 33 = IRQ1 + 32)
    idt_register_handler(KB_IRQ_VECTOR, kb_irq_handler);
    // Enable keyboard IRQ1 via I/O APIC
    ioapic_unmask_irq(1);
}

char kb_getchar(void) {
    char c;
    while (!kb_pop(&c)) cpu_relax();
    return c;
}

int kb_poll(char *c) {
    return kb_pop(c);
}
