// PolandOS — Glowna funkcja jadra
// Jadro Orzel — serce systemu
// Polska nigdy nie zginie. Pisz jadro.

#include <stddef.h>
#include "../include/limine.h"
#include "../include/io.h"
#include "arch/x86_64/cpu/gdt.h"
#include "arch/x86_64/cpu/idt.h"
#include "arch/x86_64/cpu/apic.h"
#include "arch/x86_64/mm/pmm.h"
#include "arch/x86_64/mm/vmm.h"
#include "arch/x86_64/mm/heap.h"
#include "drivers/serial.h"
#include "drivers/fb.h"
#include "drivers/keyboard.h"
#include "drivers/timer.h"
#include "drivers/rtc.h"
#include "drivers/pci.h"
#include "drivers/nvme.h"
#include "drivers/e1000.h"
#include "drivers/speaker.h"
#include "drivers/gpu.h"
#include "drivers/mouse.h"
#include "acpi/acpi.h"
#include "fs/vfs.h"
#include "net/ethernet.h"
#include "net/dhcp.h"
#include "gui/desktop.h"
#include "shell/shell.h"
#include "services/service.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/panic.h"

// ─── Limine Requests ─────────────────────────────────────────────────────────

// Start marker — Limine v7 scans between these markers for requests
__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id       = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id       = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id       = LIMINE_HHDM_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id       = LIMINE_RSDP_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kaddr_request = {
    .id       = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
};

// End marker — must come after all requests
__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// ─── Kernel Heap Address ─────────────────────────────────────────────────────
#define KERNEL_HEAP_START  0xFFFF900000000000ULL
#define KERNEL_HEAP_SIZE   (64ULL * 1024ULL * 1024ULL)  // 64 MB

// ─── Boot-time state (shared between service wrappers) ───────────────────────
static u64 boot_hhdm;
static struct limine_memmap_response *boot_memmap;

// ─── Exception handlers ───────────────────────────────────────────────────────

static void page_fault_handler(InterruptFrame *frame) {
    u64 cr2 = read_cr2();
    kpanic("Blad strony (#PF)\n"
           "  Adres: 0x%llx  Kod: 0x%llx\n"
           "  RIP: 0x%llx  RSP: 0x%llx\n",
           cr2, frame->err_code, frame->rip, frame->rsp);
}

static void gpf_handler(InterruptFrame *frame) {
    kpanic("Naruszenie ogolne ochrony (#GP)\n"
           "  Selektor/kod: 0x%llx\n"
           "  RIP: 0x%llx  RSP: 0x%llx\n",
           frame->err_code, frame->rip, frame->rsp);
}

// ─── Service wrapper functions ───────────────────────────────────────────────
// Each wraps an existing init call to match the svc_init_fn signature (int→0/err)

// PMM is initialized early (before IDT), so this is a no-op placeholder
static int svc_pmm(void) {
    // Already initialized in kmain before IDT setup
    return 0;
}

static int svc_vmm(void) {
    u64 kernel_phys = kaddr_request.response->physical_base;
    u64 kernel_virt = kaddr_request.response->virtual_base;
    u64 kernel_size = 2ULL * 1024ULL * 1024ULL;
    for (u64 i = 0; i < boot_memmap->entry_count; i++) {
        struct limine_memmap_entry *e = boot_memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_KERNEL_AND_MODULES) {
            kernel_size = e->length;
            break;
        }
    }
    vmm_init(boot_hhdm, kernel_phys, kernel_virt, kernel_size);
    return 0;
}

static int svc_heap(void) {
    vmm_map_region(KERNEL_HEAP_START, KERNEL_HEAP_SIZE,
                   PAGE_PRESENT | PAGE_WRITE | PAGE_GLOBAL);
    heap_init(KERNEL_HEAP_START, KERNEL_HEAP_SIZE);
    return 0;
}

static int svc_acpi(void) {
    u64 rsdp_phys = (u64)(uintptr_t)rsdp_request.response->address;
    acpi_init(rsdp_phys, boot_hhdm);
    return 0;
}

static int svc_apic(void) {
    apic_init();
    return 0;
}

static int svc_hpet(void) {
    if (!acpi_hpet_phys) return -1;
    hpet_init(acpi_hpet_phys, boot_hhdm);
    return 0;
}

static int svc_interrupts(void) {
    sti();
    return 0;
}

static int svc_anthem(void) {
    play_mazurek_dabrowskiego();
    return 0;
}

static int svc_pci(void) {
    if (acpi_mcfg_phys)
        pci_init(acpi_mcfg_phys, boot_hhdm);
    else
        pci_init(0, boot_hhdm);
    return 0;
}

static int svc_gpu(void) {
    gpu_init();
    return 0;
}

static int svc_e1000(void) {
    return e1000_init();
}

static int svc_nvme(void) {
    return nvme_init();
}

static int svc_rtc(void) {
    rtc_init();
    return 0;
}

static int svc_network(void) {
    net_init();
    return 0;
}

static int svc_dhcp(void) {
    return dhcp_discover();
}

static int svc_keyboard(void) {
    kb_init();
    return 0;
}

static int svc_mouse(void) {
    mouse_init();
    return 0;
}

static int svc_vfs(void) {
    vfs_init();
    return 0;
}

// ─── kmain ───────────────────────────────────────────────────────────────────

void kmain(void)
{
    // ── 1. Serial (earliest possible output) ─────────────────────────────────
    serial_init();
    kprintf("[DOBRZE] PolandOS Orzel kernel booting...\n");

    // ── 2. Validate Limine responses ─────────────────────────────────────────
    if (!fb_request.response || fb_request.response->framebuffer_count < 1)
        kpanic("[BLAD] Brak odpowiedzi Limine: framebuffer");

    if (!memmap_request.response)
        kpanic("[BLAD] Brak odpowiedzi Limine: memmap");

    if (!hhdm_request.response)
        kpanic("[BLAD] Brak odpowiedzi Limine: HHDM");

    if (!rsdp_request.response)
        kpanic("[BLAD] Brak odpowiedzi Limine: RSDP");

    if (!kaddr_request.response)
        kpanic("[BLAD] Brak odpowiedzi Limine: kernel address");

    kprintf("[DOBRZE] Odpowiedzi Limine otrzymane\n");

    // ── 3. Framebuffer ────────────────────────────────────────────────────────
    struct limine_framebuffer *fb = fb_request.response->framebuffers[0];
    fb_init((u64)(uintptr_t)fb->address,
            (u32)fb->width,
            (u32)fb->height,
            (u32)fb->pitch,
            fb->bpp,
            fb->red_mask_size,
            fb->red_mask_shift,
            fb->green_mask_size,
            fb->green_mask_shift,
            fb->blue_mask_size,
            fb->blue_mask_shift);
    kprintf("[DOBRZE] Framebuffer: %ux%u %ubpp\n",
            (u32)fb->width, (u32)fb->height, fb->bpp);

    // ── 4. Boot screen ────────────────────────────────────────────────────────
    fb_draw_boot_screen();
    kprintf("[INFO]   Ekran startowy wyswietlony\n");

    // ── 5. GDT (required before anything else) ───────────────────────────────
    gdt_init();
    kprintf("[DOBRZE] GDT zainicjalizowany\n");

    // ── Save boot-time state for early initialization ────────────────────────
    boot_hhdm   = hhdm_request.response->offset;
    boot_memmap = memmap_request.response;

    // ── Initialize PMM early (required for IST stacks) ───────────────────────
    pmm_init((void *)boot_memmap->entries, boot_memmap->entry_count, boot_hhdm);
    kprintf("[DOBRZE] PMM zainicjalizowany wczesnie\n");

    // ── Setup IST stacks (requires PMM and hhdm_offset) ──────────────────────
    gdt_setup_ist_stacks();
    kprintf("[DOBRZE] IST stacks zainicjalizowane\n");

    // ── 6. IDT (required for exception handling) ─────────────────────────────
    idt_init();
    idt_register_handler(13, gpf_handler);
    idt_register_handler(14, page_fault_handler);
    kprintf("[DOBRZE] IDT zainicjalizowany\n");

    // ── Register system services (OpenRC-style) ──────────────────────────────
    svc_register("pamiec-fizyczna",    "Menedzer pamieci fizycznej (PMM)",     svc_pmm,        true);
    svc_register("pamiec-wirtualna",   "Menedzer pamieci wirtualnej (VMM)",   svc_vmm,        true);
    svc_register("sterta",             "Alokator sterty jadra (64 MB)",       svc_heap,       true);
    svc_register("acpi",               "Tabele ACPI (FADT, MADT, HPET)",     svc_acpi,       true);
    svc_register("apic",               "Kontroler przerwan APIC",            svc_apic,       true);
    svc_register("zegar",              "Zegar HPET (1000 Hz)",               svc_hpet,       false);
    svc_register("przerwania",         "Przerwania procesora (sti)",          svc_interrupts, true);
    svc_register("hymn",               "Mazurek Dabrowskiego",                svc_anthem,     false);
    svc_register("szyna-pci",          "Magistrala PCI/PCIe",                svc_pci,        false);
    svc_register("gpu",                "Detekcja kart graficznych",           svc_gpu,        false);
    svc_register("siec-e1000",         "Karta sieciowa Intel e1000",          svc_e1000,      false);
    svc_register("dysk-nvme",          "Dysk NVMe SSD",                       svc_nvme,       false);
    svc_register("zegar-rtc",          "Zegar czasu rzeczywistego (RTC)",     svc_rtc,        false);
    svc_register("stos-sieciowy",      "Warstwa sieciowa (Ethernet/IP)",      svc_network,    false);
    svc_register("dhcp",               "Klient DHCP (autokonfiguracja IP)",   svc_dhcp,       false);
    svc_register("klawiatura",         "Klawiatura PS/2",                     svc_keyboard,   false);
    svc_register("mysz",               "Mysz PS/2",                           svc_mouse,      false);
    svc_register("vfs",                "Wirtualny system plikow (VFS)",       svc_vfs,        false);

    // ── Start all services with OpenRC-style animation ───────────────────────
    svc_start_all();

    // ── Launch desktop environment (Windows 3.0 style) ───────────────────────
    kprintf("[DOBRZE] Uruchamianie pulpitu Windows 3.0...\n");
    desktop_run();

    // Should never reach here
    cli();
    while (1) hlt();
}
