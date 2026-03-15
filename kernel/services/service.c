// PolandOS — Menedzer uslug (OpenRC-style)
// Jadro Orzel — system startowy z animacja

#include "service.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../lib/panic.h"
#include "../drivers/fb.h"
#include "../drivers/timer.h"

// ─── Tablica uslug ──────────────────────────────────────────────────────────

static Service services[SVC_MAX];
static int     service_count = 0;

// ─── Kolory ──────────────────────────────────────────────────────────────────

#define COLOR_GREEN   0x00CC00
#define COLOR_RED     0xFF3333
#define COLOR_YELLOW  0xFFFF00
#define COLOR_CYAN    0x00CCCC
#define COLOR_WHITE   0xFFFFFF
#define COLOR_BLACK   0x000000
#define COLOR_GRAY    0x888888

// ─── Rejestracja uslugi ──────────────────────────────────────────────────────

int svc_register(const char *name, const char *desc, svc_init_fn init, bool critical)
{
    if (service_count >= SVC_MAX) return -1;

    int idx = service_count++;
    services[idx].name     = name;
    services[idx].desc     = desc;
    services[idx].init     = init;
    services[idx].status   = SVC_STOPPED;
    services[idx].critical = critical;
    return idx;
}

// ─── Wyswietlanie statusu w stylu OpenRC ─────────────────────────────────────

// Print a right-aligned status tag, e.g.:  [ OK ] or [ FAIL ]
static void print_status_line(const char *name, ServiceStatus status)
{
    // " * Starting <name> ...                         [ OK ]"
    fb_set_color(COLOR_GREEN, COLOR_BLACK);
    fb_puts(" * ");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    fb_puts("Uruchamianie ");
    fb_puts(name);
    fb_puts(" ...");

    // Pad to align status tag (target column ~60)
    int len = 16 + (int)strlen(name);  // " * Uruchamianie " + name + " ..."
    int pad = 55 - len;
    if (pad < 1) pad = 1;
    for (int i = 0; i < pad; i++) fb_putchar(' ');

    switch (status) {
    case SVC_RUNNING:
        fb_puts("[ ");
        fb_set_color(COLOR_GREEN, COLOR_BLACK);
        fb_puts("OK");
        fb_set_color(COLOR_WHITE, COLOR_BLACK);
        fb_puts(" ]\n");
        break;
    case SVC_FAILED:
        fb_puts("[ ");
        fb_set_color(COLOR_RED, COLOR_BLACK);
        fb_puts("BLAD");
        fb_set_color(COLOR_WHITE, COLOR_BLACK);
        fb_puts(" ]\n");
        break;
    default:
        fb_puts("[ ");
        fb_set_color(COLOR_YELLOW, COLOR_BLACK);
        fb_puts("....");
        fb_set_color(COLOR_WHITE, COLOR_BLACK);
        fb_puts(" ]\n");
        break;
    }
}

// ─── Uruchamianie wszystkich uslug ───────────────────────────────────────────

void svc_start_all(void)
{
    fb_set_color(COLOR_CYAN, COLOR_BLACK);
    fb_puts("\n === PolandOS — Uruchamianie uslug systemowych ===\n\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);

    int ok_count   = 0;
    int fail_count = 0;

    for (int i = 0; i < service_count; i++) {
        Service *svc = &services[i];
        svc->status = SVC_STARTING;

        int rc = 0;
        if (svc->init) {
            rc = svc->init();
        }

        if (rc == 0) {
            svc->status = SVC_RUNNING;
            ok_count++;
        } else {
            svc->status = SVC_FAILED;
            fail_count++;
        }

        print_status_line(svc->name, svc->status);

        // If a critical service fails, panic
        if (svc->status == SVC_FAILED && svc->critical) {
            kpanic("Krytyczna usluga '%s' nie powiodla sie!", svc->name);
        }
    }

    // Summary line
    fb_putchar('\n');
    fb_set_color(COLOR_CYAN, COLOR_BLACK);
    fb_puts(" === ");
    fb_set_color(COLOR_GREEN, COLOR_BLACK);
    kprintf("%d", ok_count);
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
    fb_puts(" uslug uruchomionych");
    if (fail_count > 0) {
        fb_puts(", ");
        fb_set_color(COLOR_RED, COLOR_BLACK);
        kprintf("%d", fail_count);
        fb_set_color(COLOR_WHITE, COLOR_BLACK);
        fb_puts(" nieudanych");
    }
    fb_set_color(COLOR_CYAN, COLOR_BLACK);
    fb_puts(" ===\n\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);
}

// ─── Dostep do uslug ─────────────────────────────────────────────────────────

int svc_count(void)
{
    return service_count;
}

const Service *svc_get(int idx)
{
    if (idx < 0 || idx >= service_count) return NULL;
    return &services[idx];
}

// ─── Wyswietlanie tabeli (polecenie 'uslugi') ───────────────────────────────

void svc_print_status(void)
{
    fb_set_color(COLOR_CYAN, COLOR_BLACK);
    fb_puts("Uslugi systemowe PolandOS:\n\n");
    fb_set_color(COLOR_WHITE, COLOR_BLACK);

    for (int i = 0; i < service_count; i++) {
        const Service *svc = &services[i];

        fb_puts("  ");

        // Status indicator
        switch (svc->status) {
        case SVC_RUNNING:
            fb_set_color(COLOR_GREEN, COLOR_BLACK);
            fb_puts("[AKTYWNA] ");
            break;
        case SVC_FAILED:
            fb_set_color(COLOR_RED, COLOR_BLACK);
            fb_puts("[BLAD]    ");
            break;
        case SVC_STOPPED:
            fb_set_color(COLOR_GRAY, COLOR_BLACK);
            fb_puts("[STOP]    ");
            break;
        default:
            fb_set_color(COLOR_YELLOW, COLOR_BLACK);
            fb_puts("[......] ");
            break;
        }

        fb_set_color(COLOR_WHITE, COLOR_BLACK);
        fb_puts(svc->name);

        // Pad name
        int pad = 22 - (int)strlen(svc->name);
        if (pad < 1) pad = 1;
        for (int j = 0; j < pad; j++) fb_putchar(' ');

        fb_set_color(COLOR_GRAY, COLOR_BLACK);
        fb_puts(svc->desc);
        fb_set_color(COLOR_WHITE, COLOR_BLACK);
        fb_putchar('\n');
    }
    kprintf("\n  Lacznie: %d uslug\n", service_count);
}
