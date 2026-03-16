// PolandOS — Sterownik UART COM1 (115200 baud, 8N1)
// Comprehensive serial driver with full UART functionality
#include "serial.h"
#include "../../include/io.h"

#define COM1_BASE   0x3F8
#define COM1_DATA   (COM1_BASE + 0)  // Data register (R/W)
#define COM1_IER    (COM1_BASE + 1)  // Interrupt Enable Register
#define COM1_IIR    (COM1_BASE + 2)  // Interrupt Identification Register (R)
#define COM1_FCR    (COM1_BASE + 2)  // FIFO Control Register (W)
#define COM1_LCR    (COM1_BASE + 3)  // Line Control Register
#define COM1_MCR    (COM1_BASE + 4)  // Modem Control Register
#define COM1_LSR    (COM1_BASE + 5)  // Line Status Register
#define COM1_MSR    (COM1_BASE + 6)  // Modem Status Register
#define COM1_SCRATCH (COM1_BASE + 7) // Scratch Register

// Line Status Register bits
#define LSR_DATA_READY  (1 << 0)  // Data available to read
#define LSR_OVERRUN_ERR (1 << 1)  // Overrun error
#define LSR_PARITY_ERR  (1 << 2)  // Parity error
#define LSR_FRAMING_ERR (1 << 3)  // Framing error
#define LSR_BREAK_INT   (1 << 4)  // Break interrupt
#define LSR_THRE        (1 << 5)  // Transmitter Holding Register Empty
#define LSR_TEMT        (1 << 6)  // Transmitter Empty
#define LSR_FIFO_ERR    (1 << 7)  // FIFO error

// Line Control Register bits
#define LCR_DLAB        (1 << 7)  // Divisor Latch Access Bit
#define LCR_BREAK       (1 << 6)  // Set break enable
#define LCR_PARITY_NONE 0x00
#define LCR_PARITY_ODD  0x08
#define LCR_PARITY_EVEN 0x18
#define LCR_STOP_1      0x00
#define LCR_STOP_2      0x04
#define LCR_BITS_5      0x00
#define LCR_BITS_6      0x01
#define LCR_BITS_7      0x02
#define LCR_BITS_8      0x03

// Modem Control Register bits
#define MCR_DTR         (1 << 0)  // Data Terminal Ready
#define MCR_RTS         (1 << 1)  // Request To Send
#define MCR_OUT1        (1 << 2)  // Auxiliary output 1
#define MCR_OUT2        (1 << 3)  // Auxiliary output 2 (enables IRQs)
#define MCR_LOOPBACK    (1 << 4)  // Loopback mode

// FIFO Control Register bits
#define FCR_ENABLE      (1 << 0)  // Enable FIFO
#define FCR_CLEAR_RX    (1 << 1)  // Clear receive FIFO
#define FCR_CLEAR_TX    (1 << 2)  // Clear transmit FIFO
#define FCR_DMA_MODE    (1 << 3)  // DMA mode select
#define FCR_TRIGGER_1   0x00      // 1-byte trigger
#define FCR_TRIGGER_4   0x40      // 4-byte trigger
#define FCR_TRIGGER_8   0x80      // 8-byte trigger
#define FCR_TRIGGER_14  0xC0      // 14-byte trigger

// Interrupt Enable Register bits
#define IER_DATA_AVAIL  (1 << 0)  // Data available
#define IER_TX_EMPTY    (1 << 1)  // Transmitter empty
#define IER_LINE_STATUS (1 << 2)  // Line status change
#define IER_MODEM_STATUS (1 << 3) // Modem status change

// Serial port statistics
typedef struct {
    u64 bytes_sent;
    u64 bytes_received;
    u64 overrun_errors;
    u64 parity_errors;
    u64 framing_errors;
} SerialStats;

static SerialStats serial_stats = {0};
static bool serial_initialized = false;

// ═══════════════════════════════════════════════════════════════════════════
// Low-Level UART Operations
// ═══════════════════════════════════════════════════════════════════════════

static inline void serial_wait_tx(void) {
    int timeout = 100000;
    while (!(inb(COM1_LSR) & LSR_THRE) && timeout-- > 0) {
        cpu_relax();
    }
}

static inline bool serial_data_available(void) {
    return (inb(COM1_LSR) & LSR_DATA_READY) != 0;
}

static inline bool serial_tx_empty(void) {
    return (inb(COM1_LSR) & LSR_TEMT) != 0;
}

static inline u8 serial_check_errors(void) {
    return inb(COM1_LSR) & (LSR_OVERRUN_ERR | LSR_PARITY_ERR | LSR_FRAMING_ERR);
}

// ═══════════════════════════════════════════════════════════════════════════
// Initialization
// ═══════════════════════════════════════════════════════════════════════════

void serial_init(void) {
    // Disable all interrupts
    outb(COM1_IER, 0x00);

    // Enable DLAB to set baud rate divisor
    outb(COM1_LCR, LCR_DLAB);

    // Set divisor = 1 → 115200 baud (base clock 1.8432 MHz / 16 / 1)
    // For different baud rates:
    //   115200: divisor = 1
    //   57600:  divisor = 2
    //   38400:  divisor = 3
    //   19200:  divisor = 6
    //   9600:   divisor = 12
    outb(COM1_DATA, 0x01); // divisor low byte
    outb(COM1_IER,  0x00); // divisor high byte

    // Disable DLAB, set 8 data bits, no parity, 1 stop bit (8N1)
    outb(COM1_LCR, LCR_BITS_8 | LCR_PARITY_NONE | LCR_STOP_1);

    // Enable FIFO, clear TX/RX queues, 14-byte threshold
    outb(COM1_FCR, FCR_ENABLE | FCR_CLEAR_RX | FCR_CLEAR_TX | FCR_TRIGGER_14);

    // Set DTR, RTS, OUT2 (enables interrupts in hardware)
    outb(COM1_MCR, MCR_DTR | MCR_RTS | MCR_OUT2);

    // Test the serial chip (loopback test)
    outb(COM1_MCR, MCR_LOOPBACK | MCR_OUT1 | MCR_OUT2);
    outb(COM1_DATA, 0xAE);  // Send test byte

    if (inb(COM1_DATA) != 0xAE) {
        // Serial chip is faulty - disable loopback and continue anyway
        outb(COM1_MCR, MCR_DTR | MCR_RTS | MCR_OUT2);
        serial_initialized = false;
        return;
    }

    // Normal operation mode
    outb(COM1_MCR, MCR_DTR | MCR_RTS | MCR_OUT2);

    // Clear any pending data
    while (serial_data_available()) {
        inb(COM1_DATA);
    }

    serial_initialized = true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Character I/O
// ═══════════════════════════════════════════════════════════════════════════

void serial_write_char(char c) {
    if (!serial_initialized) return;

    if (c == '\n') {
        serial_wait_tx();
        outb(COM1_DATA, '\r');
        serial_stats.bytes_sent++;
    }

    serial_wait_tx();
    outb(COM1_DATA, (u8)c);
    serial_stats.bytes_sent++;
}

char serial_read_char(void) {
    if (!serial_initialized) return '\0';

    // Wait for data to be available
    while (!serial_data_available()) {
        cpu_relax();
    }

    // Check for errors
    u8 lsr = inb(COM1_LSR);
    if (lsr & LSR_OVERRUN_ERR) serial_stats.overrun_errors++;
    if (lsr & LSR_PARITY_ERR)  serial_stats.parity_errors++;
    if (lsr & LSR_FRAMING_ERR) serial_stats.framing_errors++;

    u8 data = inb(COM1_DATA);
    serial_stats.bytes_received++;

    return (char)data;
}

char serial_read_char_nowait(void) {
    if (!serial_initialized) return '\0';
    if (!serial_data_available()) return '\0';

    u8 data = inb(COM1_DATA);
    serial_stats.bytes_received++;

    return (char)data;
}

// ═══════════════════════════════════════════════════════════════════════════
// String I/O
// ═══════════════════════════════════════════════════════════════════════════

void serial_write(const char *s) {
    if (!s) return;
    while (*s) serial_write_char(*s++);
}

int serial_read_line(char *buf, int max_len) {
    if (!buf || max_len <= 0) return 0;

    int i = 0;
    while (i < max_len - 1) {
        char c = serial_read_char();

        if (c == '\r' || c == '\n') {
            break;
        } else if (c == '\b' || c == 127) {  // Backspace
            if (i > 0) {
                i--;
                serial_write("\b \b");  // Erase character on terminal
            }
        } else if (c >= 32 && c <= 126) {  // Printable ASCII
            buf[i++] = c;
            serial_write_char(c);  // Echo
        }
    }

    buf[i] = '\0';
    serial_write_char('\n');
    return i;
}

// ═══════════════════════════════════════════════════════════════════════════
// Numeric Output
// ═══════════════════════════════════════════════════════════════════════════

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

void serial_write_dec(i64 v) {
    if (v < 0) {
        serial_write_char('-');
        v = -v;
    }

    char buf[32];
    int i = 0;

    if (v == 0) {
        buf[i++] = '0';
    } else {
        while (v > 0) {
            buf[i++] = '0' + (v % 10);
            v /= 10;
        }
    }

    // Reverse
    for (int j = i - 1; j >= 0; j--) {
        serial_write_char(buf[j]);
    }
}

void serial_write_bin(u64 v) {
    serial_write("0b");
    for (int i = 63; i >= 0; i--) {
        serial_write_char((v & (1ULL << i)) ? '1' : '0');
        if (i % 8 == 0 && i > 0) serial_write_char('_');
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════

void serial_set_baud(u32 baud) {
    if (!serial_initialized) return;

    u16 divisor = (u16)(115200 / baud);

    outb(COM1_LCR, inb(COM1_LCR) | LCR_DLAB);
    outb(COM1_DATA, (u8)(divisor & 0xFF));
    outb(COM1_IER, (u8)((divisor >> 8) & 0xFF));
    outb(COM1_LCR, inb(COM1_LCR) & ~LCR_DLAB);
}

void serial_set_format(u8 data_bits, u8 parity, u8 stop_bits) {
    if (!serial_initialized) return;

    u8 lcr = 0;

    // Data bits (5-8)
    switch (data_bits) {
        case 5: lcr |= LCR_BITS_5; break;
        case 6: lcr |= LCR_BITS_6; break;
        case 7: lcr |= LCR_BITS_7; break;
        case 8: lcr |= LCR_BITS_8; break;
        default: lcr |= LCR_BITS_8; break;
    }

    // Parity (0=none, 1=odd, 2=even)
    switch (parity) {
        case 1: lcr |= LCR_PARITY_ODD; break;
        case 2: lcr |= LCR_PARITY_EVEN; break;
        default: lcr |= LCR_PARITY_NONE; break;
    }

    // Stop bits (1 or 2)
    if (stop_bits == 2) {
        lcr |= LCR_STOP_2;
    }

    outb(COM1_LCR, lcr);
}

void serial_enable_interrupts(bool rx, bool tx) {
    if (!serial_initialized) return;

    u8 ier = 0;
    if (rx) ier |= IER_DATA_AVAIL;
    if (tx) ier |= IER_TX_EMPTY;

    outb(COM1_IER, ier);
}

// ═══════════════════════════════════════════════════════════════════════════
// Status and Statistics
// ═══════════════════════════════════════════════════════════════════════════

bool serial_is_initialized(void) {
    return serial_initialized;
}

void serial_get_stats(u64 *tx, u64 *rx, u64 *errors) {
    if (tx) *tx = serial_stats.bytes_sent;
    if (rx) *rx = serial_stats.bytes_received;
    if (errors) *errors = serial_stats.overrun_errors +
                           serial_stats.parity_errors +
                           serial_stats.framing_errors;
}

void serial_reset_stats(void) {
    serial_stats.bytes_sent = 0;
    serial_stats.bytes_received = 0;
    serial_stats.overrun_errors = 0;
    serial_stats.parity_errors = 0;
    serial_stats.framing_errors = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Diagnostic Functions
// ═══════════════════════════════════════════════════════════════════════════

void serial_dump_status(void) {
    if (!serial_initialized) {
        serial_write("[Serial] Not initialized\n");
        return;
    }

    serial_write("[Serial Status]\n");

    u8 lsr = inb(COM1_LSR);
    u8 msr = inb(COM1_MSR);
    u8 mcr = inb(COM1_MCR);

    serial_write("  LSR: 0x");
    serial_write_hex((u64)lsr);
    serial_write_char('\n');

    serial_write("    Data Ready: ");
    serial_write((lsr & LSR_DATA_READY) ? "Yes\n" : "No\n");

    serial_write("    TX Empty: ");
    serial_write((lsr & LSR_THRE) ? "Yes\n" : "No\n");

    serial_write("  Statistics:\n");
    serial_write("    TX: ");
    serial_write_dec((i64)serial_stats.bytes_sent);
    serial_write(" bytes\n");

    serial_write("    RX: ");
    serial_write_dec((i64)serial_stats.bytes_received);
    serial_write(" bytes\n");

    serial_write("    Errors: ");
    serial_write_dec((i64)(serial_stats.overrun_errors +
                           serial_stats.parity_errors +
                           serial_stats.framing_errors));
    serial_write_char('\n');
}

