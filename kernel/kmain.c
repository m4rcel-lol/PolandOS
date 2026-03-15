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
#include "acpi/acpi.h"
#include "net/ethernet.h"
#include "net/dhcp.h"
#include "shell/shell.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/panic.h"

// ─── Limine Requests ─────────────────────────────────────────────────────────

// NOTE: LIMINE_BASE_REVISION macro already includes section(".limine_reqs")
static volatile LIMINE_BASE_REVISION(6);

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

// ─── Kernel Heap Address ─────────────────────────────────────────────────────
#define KERNEL_HEAP_START  0xFFFF900000000000ULL
#define KERNEL_HEAP_SIZE   (64ULL * 1024ULL * 1024ULL)  // 64 MB

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
            fb->bpp);
    kprintf("[DOBRZE] Framebuffer: %ux%u %ubpp\n",
            (u32)fb->width, (u32)fb->height, fb->bpp);

    // ── 4. Boot screen ────────────────────────────────────────────────────────
    fb_draw_boot_screen();
    kprintf("[INFO]   Ekran startowy wyswietlony\n");

    // NOTE: Mazurek Dabrowskiego moved after HPET + sti() — timer_sleep_ms()
    //       requires working interrupts and an active HPET tick.

    // ── 5. GDT ───────────────────────────────────────────────────────────────
    gdt_init();
    kprintf("[DOBRZE] GDT zainicjalizowany\n");

    // ── 7. IDT ───────────────────────────────────────────────────────────────
    idt_init();
    idt_register_handler(13, gpf_handler);
    idt_register_handler(14, page_fault_handler);
    kprintf("[DOBRZE] IDT zainicjalizowany\n");
    // ── 8. PMM ────────────────────────────────────────────────────────────────
    u64 hhdm = hhdm_request.response->offset;
    struct limine_memmap_response *mm = memmap_request.response;

    pmm_init((void *)mm->entries, mm->entry_count, hhdm);
    kprintf("[DOBRZE] PMM: total=%llu MB, free=%llu MB\n",
            pmm_total_bytes() / (1024ULL * 1024ULL),
            pmm_free_bytes()  / (1024ULL * 1024ULL));

    // ── 9. VMM ────────────────────────────────────────────────────────────────
    u64 kernel_phys = kaddr_request.response->physical_base;
    u64 kernel_virt = kaddr_request.response->virtual_base;

    // Determine kernel size by scanning memmap for kernel region
    u64 kernel_size = 2ULL * 1024ULL * 1024ULL;  // conservative 2 MB default
    for (u64 i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        if (e->type == LIMINE_MEMMAP_KERNEL_AND_MODULES) {
            kernel_size = e->length;
            break;
        }
    }

    vmm_init(hhdm, kernel_phys, kernel_virt, kernel_size);
    kprintf("[DOBRZE] VMM zainicjalizowany (HHDM=0x%llx)\n", hhdm);

    // ── 9a. IST stacks (double-fault needs a known-good stack) ───────────────
    gdt_setup_ist_stacks();
    kprintf("[DOBRZE] GDT: IST stack skonfigurowany\n");

    // ── 10. Heap ──────────────────────────────────────────────────────────────
    // Map the heap virtual address range before initialising the heap allocator
    vmm_map_region(KERNEL_HEAP_START, KERNEL_HEAP_SIZE,
                   PAGE_PRESENT | PAGE_WRITE | PAGE_GLOBAL);
    heap_init(KERNEL_HEAP_START, KERNEL_HEAP_SIZE);
    kprintf("[DOBRZE] Sterta jadra: 0x%llx, %llu MB\n",
            KERNEL_HEAP_START, KERNEL_HEAP_SIZE / (1024ULL * 1024ULL));

    // ── 11. ACPI ─────────────────────────────────────────────────────────────
    u64 rsdp_phys = (u64)(uintptr_t)rsdp_request.response->address;
    acpi_init(rsdp_phys, hhdm);
    kprintf("[DOBRZE] ACPI zainicjalizowany\n");

    // ── 12. APIC ─────────────────────────────────────────────────────────────
    apic_init();
    kprintf("[DOBRZE] APIC zainicjalizowany\n");

    // ── 13. HPET ─────────────────────────────────────────────────────────────
    if (acpi_hpet_phys) {
        hpet_init(acpi_hpet_phys, hhdm);
        kprintf("[DOBRZE] HPET zainicjalizowany (phys=0x%llx)\n", acpi_hpet_phys);
    } else {
        kprintf("[UWAGA]  Brak HPET — zegar moze nie dzialac\n");
    }

    // ── 14. Enable interrupts ─────────────────────────────────────────────────
    sti();
    kprintf("[DOBRZE] Przerwania wlaczone\n");

    // ── 14a. Mazurek Dabrowskiego (needs working timer + interrupts) ──────────
    play_mazurek_dabrowskiego();
    kprintf("[INFO]   Mazurek Dabrowskiego zagrany\n");

    // ── 15. PCI ───────────────────────────────────────────────────────────────
    if (acpi_mcfg_phys) {
        pci_init(acpi_mcfg_phys, hhdm);
    } else {
        kprintf("[INFO]   Brak MCFG — PCI przez porty I/O (CF8/CFC)\n");
        pci_init(0, hhdm);  // 0 = use legacy I/O ports
    }
    kprintf("[DOBRZE] PCI: znaleziono %d urzadzen\n", pci_device_count);

    // ── 15a. GPU detection ────────────────────────────────────────────────────
    gpu_init();

    // ── 16. e1000 ─────────────────────────────────────────────────────────────
    int e1000_ok = e1000_init();
    if (e1000_ok == 0) {
        kprintf("[DOBRZE] e1000: karta sieciowa zainicjalizowana\n");
    } else {
        kprintf("[UWAGA]  e1000: karta sieciowa niedostepna (kod=%d)\n", e1000_ok);
    }

    // ── 17. NVMe ─────────────────────────────────────────────────────────────
    int nvme_ok = nvme_init();
    if (nvme_ok == 0) {
        u64 blocks   = nvme_get_block_count(1);
        u32 blk_size = nvme_get_block_size(1);
        u64 mb       = (blocks * blk_size) / (1024ULL * 1024ULL);
        kprintf("[DOBRZE] NVMe: %llu MB pojemnosci\n", mb);
    } else {
        kprintf("[UWAGA]  NVMe: dysk niedostepny (kod=%d)\n", nvme_ok);
    }

    // ── 18. RTC ───────────────────────────────────────────────────────────────
    rtc_init();
    RTCTime t = rtc_read();
    kprintf("[DOBRZE] RTC: %02u.%02u.%04u %02u:%02u:%02u\n",
            t.day, t.month, t.year, t.hours, t.minutes, t.seconds);

    // ── 19. Network init ─────────────────────────────────────────────────────
    net_init();
    kprintf("[DOBRZE] Warstwa sieciowa zainicjalizowana\n");

    // ── 20. DHCP ─────────────────────────────────────────────────────────────
    if (e1000_ok == 0) {
        kprintf("[INFO]   DHCP: wyslano zapytanie...\n");
        int dhcp_ok = dhcp_discover();
        if (dhcp_ok == 0) {
            char ip_str[16];
            ip_to_str(net_ip, ip_str);
            kprintf("[DOBRZE] DHCP: adres IP = %s\n", ip_str);
        } else {
            kprintf("[UWAGA]  DHCP: nie uzyskano adresu IP\n");
        }
    }

    // ── 21. Keyboard ─────────────────────────────────────────────────────────
    kb_init();
    kprintf("[DOBRZE] Klawiatura PS/2 zainicjalizowana\n");

    // ── 22. Shell ─────────────────────────────────────────────────────────────
    kprintf("[DOBRZE] Uruchamianie powloki systemowej...\n");
    shell_run();  // never returns

    // Should never reach here
    cli();
    while (1) hlt();
}
