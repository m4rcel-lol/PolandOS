// PolandOS — Panika jądra — Gdy wszystko się posypie
// Comprehensive kernel panic handler with stack trace and diagnostics
#include "panic.h"
#include "printf.h"
#include "../../include/io.h"

extern void fb_set_panic_colors(void);

// Panic context structure for debugging
typedef struct {
    u64 rip;
    u64 rsp;
    u64 rbp;
    u64 cr0;
    u64 cr2;
    u64 cr3;
    u64 cr4;
} PanicContext;

static PanicContext panic_ctx;
static bool panic_in_progress = false;
static int panic_count = 0;

// ═══════════════════════════════════════════════════════════════════════════
// Register Dumping — Zrzut rejestrow procesora
// ═══════════════════════════════════════════════════════════════════════════

static inline u64 read_cr0(void) {
    u64 val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline u64 read_cr4(void) {
    u64 val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline u64 read_rsp(void) {
    u64 val;
    __asm__ volatile("mov %%rsp, %0" : "=r"(val));
    return val;
}

static inline u64 read_rbp(void) {
    u64 val;
    __asm__ volatile("mov %%rbp, %0" : "=r"(val));
    return val;
}

// ═══════════════════════════════════════════════════════════════════════════
// Stack Trace — Sledzenie stosu
// ═══════════════════════════════════════════════════════════════════════════

static void dump_stack_trace(void) {
    kprintf("\n[Slad stosu]\n");

    u64 *rbp = (u64 *)read_rbp();

    for (int i = 0; i < 16 && rbp != NULL; i++) {
        // Validate pointer is in kernel space
        if ((u64)rbp < 0xFFFF800000000000ULL) {
            kprintf("  #%d: <invalid frame pointer>\n", i);
            break;
        }

        u64 rip = *(rbp + 1);
        kprintf("  #%d: 0x%016llx\n", i, rip);

        rbp = (u64 *)*rbp;

        // Detect stack loops
        if (rbp == (u64 *)*rbp) break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Register Dump — Zrzut rejestrow
// ═══════════════════════════════════════════════════════════════════════════

static void dump_registers(void) {
    kprintf("\n[Rejestry kontrolne]\n");
    kprintf("  CR0: 0x%016llx  CR2: 0x%016llx\n", panic_ctx.cr0, panic_ctx.cr2);
    kprintf("  CR3: 0x%016llx  CR4: 0x%016llx\n", panic_ctx.cr3, panic_ctx.cr4);
    kprintf("\n[Wskazniki]\n");
    kprintf("  RIP: 0x%016llx\n", panic_ctx.rip);
    kprintf("  RSP: 0x%016llx\n", panic_ctx.rsp);
    kprintf("  RBP: 0x%016llx\n", panic_ctx.rbp);
}

// ═══════════════════════════════════════════════════════════════════════════
// Memory Dump — Zrzut pamieci
// ═══════════════════════════════════════════════════════════════════════════

static void dump_memory_at(u64 addr, int lines) {
    kprintf("\n[Zawartosc pamieci pod adresem 0x%016llx]\n", addr);

    // Validate address is in kernel space
    if (addr < 0xFFFF800000000000ULL) {
        kprintf("  <adres poza przestrzenia jadra>\n");
        return;
    }

    u8 *ptr = (u8 *)addr;
    for (int i = 0; i < lines; i++) {
        kprintf("  %016llx: ", (u64)(ptr + i * 16));

        // Hex dump
        for (int j = 0; j < 16; j++) {
            kprintf("%02x ", ptr[i * 16 + j]);
        }

        kprintf(" | ");

        // ASCII dump
        for (int j = 0; j < 16; j++) {
            u8 c = ptr[i * 16 + j];
            if (c >= 32 && c <= 126) {
                kprintf("%c", c);
            } else {
                kprintf(".");
            }
        }

        kprintf("\n");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Panic Banner — Banner z informacja o panice
// ═══════════════════════════════════════════════════════════════════════════

static void print_panic_banner(void) {
    kprintf("\n\n\n");
    kprintf("╔══════════════════════════════════════════════════════════════╗\n");
    kprintf("║                                                              ║\n");
    kprintf("║           PANIKA JADRA — ORZEL KERNEL CRASH                 ║\n");
    kprintf("║           PolandOS v0.0.1 — Kernel Stopped                  ║\n");
    kprintf("║                                                              ║\n");
    kprintf("║    System zatrzymany ze wzgledu na powazny blad jadra       ║\n");
    kprintf("║    Kernel stopped due to critical error                     ║\n");
    kprintf("║                                                              ║\n");
    kprintf("╚══════════════════════════════════════════════════════════════╝\n\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// Panic Footer — Stopka z instrukcjami
// ═══════════════════════════════════════════════════════════════════════════

static void print_panic_footer(void) {
    kprintf("\n");
    kprintf("╔══════════════════════════════════════════════════════════════╗\n");
    kprintf("║                     CO DALEJ?                                ║\n");
    kprintf("╚══════════════════════════════════════════════════════════════╝\n");
    kprintf("\nPolska nigdy nie zginie — ale ten kernel wlasnie tak.\n");
    kprintf("Restart wymagany. Przepraszamy za utrudnienia.\n");
    kprintf("\nDiagnostyka zapisana powyzej. Prosimy o:\n");
    kprintf("  1. Zrobienie zrzutu ekranu lub zapisanie logow (serial)\n");
    kprintf("  2. Zgloszenie bledu z informacja o kontekscie\n");
    kprintf("  3. Restart systemu\n");
    kprintf("\nSystem jest teraz zablokowany. Nacisnij reset.\n\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// Main Panic Handler
// ═══════════════════════════════════════════════════════════════════════════

__attribute__((noreturn)) void kpanic(const char *fmt, ...) {
    cli();

    // Prevent recursive panics
    if (panic_in_progress) {
        panic_count++;
        if (panic_count > 3) {
            // Triple fault - just halt
            for(;;) { cli(); hlt(); }
        }
    }

    panic_in_progress = true;

    // Capture register state
    panic_ctx.cr0 = read_cr0();
    panic_ctx.cr2 = read_cr2();
    panic_ctx.cr3 = read_cr3();
    panic_ctx.cr4 = read_cr4();
    panic_ctx.rsp = read_rsp();
    panic_ctx.rbp = read_rbp();

    // Get return address (caller's RIP)
    u64 *stack = (u64 *)panic_ctx.rsp;
    panic_ctx.rip = stack[0];

    fb_set_panic_colors();
    print_panic_banner();

    // Print panic message
    kprintf("[PANIKA] ");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf("\n");

    // Dump diagnostic information
    dump_registers();
    dump_stack_trace();
    dump_memory_at(panic_ctx.rip, 8);
    dump_memory_at(panic_ctx.rsp, 8);

    print_panic_footer();

    // Halt forever
    for(;;) {
        cli();
        hlt();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Panic with Location Information
// ═══════════════════════════════════════════════════════════════════════════

__attribute__((noreturn)) void kpanic_at(const char *file, int line, const char *fmt, ...) {
    cli();

    // Prevent recursive panics
    if (panic_in_progress) {
        panic_count++;
        if (panic_count > 3) {
            for(;;) { cli(); hlt(); }
        }
    }

    panic_in_progress = true;

    // Capture register state
    panic_ctx.cr0 = read_cr0();
    panic_ctx.cr2 = read_cr2();
    panic_ctx.cr3 = read_cr3();
    panic_ctx.cr4 = read_cr4();
    panic_ctx.rsp = read_rsp();
    panic_ctx.rbp = read_rbp();

    // Get return address
    u64 *stack = (u64 *)panic_ctx.rsp;
    panic_ctx.rip = stack[0];

    fb_set_panic_colors();
    print_panic_banner();

    // Print panic message with source location
    kprintf("[PANIKA] w pliku %s linia %d\n", file, line);
    kprintf("[WIADOMOSC] ");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf("\n");

    // Dump diagnostic information
    dump_registers();
    dump_stack_trace();
    dump_memory_at(panic_ctx.rip, 8);
    dump_memory_at(panic_ctx.rsp, 8);

    print_panic_footer();

    // Halt forever
    for(;;) {
        cli();
        hlt();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Assertion Helper — Asercje z pelna diagnostyka
// ═══════════════════════════════════════════════════════════════════════════

void kassert_failed(const char *expr, const char *file, int line, const char *func) {
    kpanic_at(file, line, "Asercja nie powiodla sie: %s w funkcji %s", expr, func);
}

// ═══════════════════════════════════════════════════════════════════════════
// Early Panic (before framebuffer is ready)
// ═══════════════════════════════════════════════════════════════════════════

__attribute__((noreturn)) void early_panic(const char *msg) {
    cli();

    // Try to print to serial if available
    const char *prefix = "[EARLY PANIC] ";
    for (int i = 0; prefix[i]; i++) {
        outb(0x3F8, prefix[i]);
    }

    for (int i = 0; msg[i]; i++) {
        outb(0x3F8, msg[i]);
    }

    outb(0x3F8, '\n');

    for(;;) {
        cli();
        hlt();
    }
}
