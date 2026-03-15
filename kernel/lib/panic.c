// PolandOS — Panika jądra — Gdy wszystko się posypie
#include "panic.h"
#include "printf.h"
#include "../../include/io.h"

extern void fb_set_panic_colors(void);

__attribute__((noreturn)) void kpanic(const char *fmt, ...) {
    cli();
    fb_set_panic_colors();
    kprintf("\n\n");
    kprintf("╔══════════════════════════════════════════════════════════════╗\n");
    kprintf("║           PANIKA JADRA — ORZEL KERNEL CRASH                 ║\n");
    kprintf("║    PolandOS — System zatrzymany ze wzgledu na blad          ║\n");
    kprintf("╚══════════════════════════════════════════════════════════════╝\n\n");
    kprintf("[PANIKA] ");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf("\n\n");
    kprintf("Polska nigdy nie zginie — ale ten kernel wlasnie tak.\n");
    kprintf("Restart wymagany. Przepraszamy za utrudnienia.\n");
    for(;;) { cli(); hlt(); }
}

__attribute__((noreturn)) void kpanic_at(const char *file, int line, const char *fmt, ...) {
    cli();
    fb_set_panic_colors();
    kprintf("\n\n");
    kprintf("╔══════════════════════════════════════════════════════════════╗\n");
    kprintf("║           PANIKA JADRA — ORZEL KERNEL CRASH                 ║\n");
    kprintf("╚══════════════════════════════════════════════════════════════╝\n\n");
    kprintf("[PANIKA] w pliku %s linia %d\n", file, line);
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf("\n\nPolska nigdy nie zginie — ale ten kernel wlasnie tak.\n");
    for(;;) { cli(); hlt(); }
}
