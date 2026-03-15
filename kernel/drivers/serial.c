// PolandOS — Sterownik UART COM1 (115200 baud, 8N1)
#include "serial.h"
#include "../../include/io.h"

#define COM1_BASE   0x3F8
#define COM1_DATA   (COM1_BASE + 0)
#define COM1_IER    (COM1_BASE + 1)
#define COM1_FCR    (COM1_BASE + 2)
#define COM1_LCR    (COM1_BASE + 3)
#define COM1_MCR    (COM1_BASE + 4)
#define COM1_LSR    (COM1_BASE + 5)
#define COM1_MSR    (COM1_BASE + 6)
#define COM1_SCRATCH (COM1_BASE + 7)

#define LSR_THRE    (1 << 5)  // Transmitter Holding Register Empty
#define LCR_DLAB    (1 << 7)  // Divisor Latch Access Bit

void serial_init(void) {
    // Disable all interrupts
    outb(COM1_IER, 0x00);
    // Enable DLAB to set baud rate divisor
    outb(COM1_LCR, LCR_DLAB);
    // Set divisor = 1 → 115200 baud (base clock 1.8432 MHz / 16 / 1)
    outb(COM1_DATA, 0x01); // divisor low byte
    outb(COM1_IER,  0x00); // divisor high byte
    // Disable DLAB, set 8 data bits, no parity, 1 stop bit (8N1)
    outb(COM1_LCR, 0x03);
    // Enable FIFO, clear TX/RX queues, 14-byte threshold
    outb(COM1_FCR, 0xC7);
    // RTS/DSR set
    outb(COM1_MCR, 0x0B);
}

static inline void serial_wait_tx(void) {
    while (!(inb(COM1_LSR) & LSR_THRE))
        cpu_relax();
}

void serial_write_char(char c) {
    if (c == '\n') {
        serial_wait_tx();
        outb(COM1_DATA, '\r');
    }
    serial_wait_tx();
    outb(COM1_DATA, (u8)c);
}

void serial_write(const char *s) {
    while (*s) serial_write_char(*s++);
}

void serial_write_hex(u64 v) {
    const char hex[] = "0123456789abcdef";
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++)
        buf[2 + i] = hex[(v >> (60 - i * 4)) & 0xF];
    buf[18] = '\0';
    serial_write(buf);
}
