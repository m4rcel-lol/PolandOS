// PolandOS — Sterownik HPET (High Precision Event Timer)
#include "timer.h"
#include "../../include/io.h"
#include "../lib/printf.h"
#include "../arch/x86_64/cpu/idt.h"
#include "../arch/x86_64/cpu/apic.h"

// ─── HPET register offsets ────────────────────────────────────────────────────
#define HPET_GCAP        0x000   // General Capabilities & ID (64-bit)
#define HPET_GCFG        0x010   // General Configuration (64-bit)
#define HPET_GIST        0x020   // General Interrupt Status (64-bit)
#define HPET_MCNTR       0x0F0   // Main Counter Value (64-bit)

// Timer N base: 0x100 + N*0x20
#define HPET_TN_CFG(n)   (0x100 + (n) * 0x20 + 0x00)  // Config & Cap (64-bit)
#define HPET_TN_CMP(n)   (0x100 + (n) * 0x20 + 0x08)  // Comparator (64-bit)
#define HPET_TN_FSB(n)   (0x100 + (n) * 0x20 + 0x10)  // FSB Interrupt Route (64-bit)

// GCAP fields
#define HPET_GCAP_PERIOD_SHIFT  32   // femtosecond period in bits 63:32
#define HPET_GCAP_NUM_TIM_MASK  0x1F00
#define HPET_GCAP_NUM_TIM_SHIFT 8

// GCFG bits
#define HPET_GCFG_ENABLE  (1ULL << 0)   // main counter enable
#define HPET_GCFG_LEGACY  (1ULL << 1)   // legacy replacement mapping

// Timer config bits
#define HPET_TN_INT_TYPE_LEVEL   (1ULL << 1)   // 0=edge, 1=level
#define HPET_TN_INT_ENB          (1ULL << 2)   // interrupt enable
#define HPET_TN_TYPE_PERIODIC    (1ULL << 3)   // periodic mode
#define HPET_TN_PER_INT_CAP      (1ULL << 4)   // periodic capable (read-only)
#define HPET_TN_SIZE_CAP         (1ULL << 5)   // 64-bit capable (read-only)
#define HPET_TN_VAL_SET          (1ULL << 6)   // set accumulator value
#define HPET_TN_32MODE           (1ULL << 8)   // force 32-bit mode
// bits 13:9 = INT_ROUTE_CNF (IOAPIC pin selection)
#define HPET_TN_INT_ROUTE_SHIFT  9
// bits 44:32 = INT_ROUTE_CAP (valid IOAPIC pins, read-only)
#define HPET_TN_ROUTE_CAP_SHIFT  32

// HPET timer 0 IRQ → IOAPIC pin 2 → vector 0x22
#define HPET_IRQ        2
#define HPET_VECTOR     0x22

// ─── State ───────────────────────────────────────────────────────────────────
volatile u64 kernel_ticks = 0;

static u64  hpet_base    = 0;   // virtual base address
static u64  hpet_freq    = 0;   // Hz
static u64  hpet_period  = 0;   // ticks per 1 ms

// ─── MMIO accessors ──────────────────────────────────────────────────────────
static inline u64 hpet_read(u32 reg) {
    return *(volatile u64 *)(hpet_base + reg);
}

static inline void hpet_write(u32 reg, u64 val) {
    *(volatile u64 *)(hpet_base + reg) = val;
}

// ─── IRQ handler ─────────────────────────────────────────────────────────────
static void hpet_irq_handler(InterruptFrame *frame) {
    (void)frame;
    kernel_ticks++;

    // Clear level-triggered interrupt status bit for timer 0
    hpet_write(HPET_GIST, 1ULL);

    apic_send_eoi();
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void hpet_init(u64 hpet_phys, u64 hhdm) {
    hpet_base = hpet_phys + hhdm;

    // Read General Capabilities
    u64 gcap = hpet_read(HPET_GCAP);
    u32 period_fs = (u32)(gcap >> HPET_GCAP_PERIOD_SHIFT); // femtoseconds per tick

    if (period_fs == 0) {
        kprintf("[BLAD] HPET: nieprawidlowy okres (zero)!\n");
        return;
    }

    // frequency = 10^15 / period_fs  (period_fs is in femtoseconds)
    hpet_freq   = 1000000000000000ULL / (u64)period_fs;
    hpet_period = hpet_freq / 1000;  // ticks per millisecond (for 1 kHz)

    kprintf("[DOBRZE] HPET: okres=%u fs, czestotl.=%lu Hz, ticki/ms=%lu\n",
            period_fs, hpet_freq, hpet_period);

    // Disable HPET while configuring
    hpet_write(HPET_GCFG, 0);

    // Reset main counter
    hpet_write(HPET_MCNTR, 0);

    // Verify timer 0 supports periodic mode
    u64 t0cap = hpet_read(HPET_TN_CFG(0));
    if (!(t0cap & HPET_TN_PER_INT_CAP)) {
        kprintf("[BLAD] HPET: timer 0 nie obsluguje trybu periodycznego!\n");
        // Fall back to non-periodic (one-shot) — still usable for basic tick
    }

    // Choose IOAPIC pin: prefer pin 2 if available in INT_ROUTE_CAP,
    // otherwise fall back to pin 0
    u32 route_cap = (u32)(t0cap >> HPET_TN_ROUTE_CAP_SHIFT);
    u8  chosen_pin = 2;
    if (!(route_cap & (1u << 2))) {
        chosen_pin = 0;
        kprintf("[INFO] HPET: pin 2 niedostepny, uzywam pinu 0\n");
    }

    // Configure timer 0: periodic, level-triggered, interrupt enabled
    u64 t0cfg = HPET_TN_INT_ENB |
                HPET_TN_TYPE_PERIODIC |
                HPET_TN_VAL_SET |
                HPET_TN_INT_TYPE_LEVEL |
                ((u64)chosen_pin << HPET_TN_INT_ROUTE_SHIFT);

    hpet_write(HPET_TN_CFG(0), t0cfg);

    // Write period (comparator accumulator) — sets the repeat interval
    hpet_write(HPET_TN_CMP(0), hpet_period);

    // Register IDT handler
    idt_register_handler(HPET_VECTOR, hpet_irq_handler);

    // Route IOAPIC: chosen_pin → vector HPET_VECTOR, CPU 0
    ioapic_set_irq(chosen_pin, HPET_VECTOR, 0);

    // Enable HPET (start main counter)
    hpet_write(HPET_GCFG, HPET_GCFG_ENABLE);

    kprintf("[DOBRZE] HPET: zainicjalizowany, IRQ%u -> vector 0x%x\n",
            chosen_pin, HPET_VECTOR);
}

// ─── Public API ───────────────────────────────────────────────────────────────
void timer_sleep_ms(u64 ms) {
    u64 target = kernel_ticks + ms;
    while (kernel_ticks < target)
        cpu_relax();
}

u64 timer_get_ticks(void) {
    return kernel_ticks;
}

u64 timer_get_ms(void) {
    return kernel_ticks;  // 1 tick = 1 ms at 1000 Hz
}
