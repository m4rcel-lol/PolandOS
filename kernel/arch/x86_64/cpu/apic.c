// PolandOS — APIC (Local APIC + I/O APIC)
// Wyłącza legacy PIC 8259A, włącza LAPIC i I/O APIC
#include "apic.h"
#include "../../../../include/io.h"
#include "../../../../include/types.h"
#include "../../../lib/panic.h"
#include "../../../lib/printf.h"

// ─── Legacy PIC ports ────────────────────────────────────────────────────────
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

// ─── MSR ─────────────────────────────────────────────────────────────────────
#define MSR_APIC_BASE 0x1B
#define APIC_BASE_ENABLE  (1ULL << 11)
#define APIC_BASE_BSP     (1ULL << 8)

// ─── LAPIC register offsets ───────────────────────────────────────────────────
#define LAPIC_ID          0x020
#define LAPIC_VER         0x030
#define LAPIC_TPR         0x080
#define LAPIC_EOI         0x0B0
#define LAPIC_SVR         0x0F0  // Spurious Interrupt Vector Register
#define LAPIC_ICR_LO      0x300
#define LAPIC_ICR_HI      0x310
#define LAPIC_LVT_TIMER   0x320
#define LAPIC_LVT_LINT0   0x350
#define LAPIC_LVT_LINT1   0x360
#define LAPIC_LVT_ERROR   0x370
#define LAPIC_TIMER_INIT  0x380
#define LAPIC_TIMER_CURR  0x390
#define LAPIC_TIMER_DIV   0x3E0

#define LAPIC_SVR_ENABLE  (1 << 8)
#define LAPIC_LVT_MASKED  (1 << 16)

// ─── I/O APIC register offsets ───────────────────────────────────────────────
#define IOAPIC_REGSEL  0x00
#define IOAPIC_WIN     0x10
#define IOAPIC_REDTBL  0x10  // Redirection Table base (2 registers per entry)

// ─── Globals ─────────────────────────────────────────────────────────────────
u64 lapic_base   = 0;
u64 ioapic_base  = 0xFEC00000ULL; // default I/O APIC base

extern u64 hhdm_offset; // from pmm.c

// ─── LAPIC MMIO access ───────────────────────────────────────────────────────
static inline u32 lapic_read(u32 reg) {
    return *((volatile u32 *)(lapic_base + hhdm_offset + reg));
}

static inline void lapic_write(u32 reg, u32 val) {
    *((volatile u32 *)(lapic_base + hhdm_offset + reg)) = val;
}

// ─── I/O APIC MMIO access ────────────────────────────────────────────────────
static inline u32 ioapic_read(u32 reg) {
    volatile u32 *base = (volatile u32 *)(ioapic_base + hhdm_offset);
    base[IOAPIC_REGSEL / 4] = reg;
    return base[IOAPIC_WIN / 4];
}

static inline void ioapic_write(u32 reg, u32 val) {
    volatile u32 *base = (volatile u32 *)(ioapic_base + hhdm_offset);
    base[IOAPIC_REGSEL / 4] = reg;
    base[IOAPIC_WIN / 4] = val;
}

// ─── Disable legacy 8259A PIC ────────────────────────────────────────────────
static void pic_disable(void) {
    // Remap PIC to 0x20/0x28 first (good practice even if masking all)
    outb(PIC1_CMD,  0x11); io_wait(); // Initialize in cascade mode
    outb(PIC2_CMD,  0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait(); // Master PIC vector offset = 32
    outb(PIC2_DATA, 0x28); io_wait(); // Slave PIC vector offset = 40
    outb(PIC1_DATA, 0x04); io_wait(); // Master: IRQ2 = cascade
    outb(PIC2_DATA, 0x02); io_wait(); // Slave: cascade identity
    outb(PIC1_DATA, 0x01); io_wait(); // 8086/88 mode
    outb(PIC2_DATA, 0x01); io_wait();
    // Mask all IRQs
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

// ─── APIC init ───────────────────────────────────────────────────────────────
void apic_init(void) {
    // Verify APIC support via CPUID
    u32 eax, edx;
    __asm__ volatile("cpuid" : "=a"(eax), "=d"(edx) : "a"(1) : "ecx", "ebx");
    if (!(edx & (1 << 9))) {
        kpanic("APIC nie jest dostepny na tym procesorze!\n");
    }

    // Disable legacy PIC
    pic_disable();

    // Read LAPIC base from MSR
    u64 apic_msr = rdmsr(MSR_APIC_BASE);
    lapic_base = apic_msr & 0xFFFFF000ULL; // 4K-aligned physical base

    // Enable LAPIC via MSR
    wrmsr(MSR_APIC_BASE, apic_msr | APIC_BASE_ENABLE);

    // Set Task Priority Register to 0 (accept all interrupts)
    lapic_write(LAPIC_TPR, 0);

    // Enable LAPIC by setting bit 8 of SVR, use vector 0xFF for spurious
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | 0xFF);

    // Mask LVT timer, LINT0, LINT1, ERROR
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_ERROR, LAPIC_LVT_MASKED);

    kprintf("[APIC] LAPIC pbase=0x%lx, ID=%u\n",
            lapic_base, lapic_read(LAPIC_ID) >> 24);
    kprintf("[APIC] I/O APIC pbase=0x%lx\n", ioapic_base);
}

void apic_send_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

// ─── I/O APIC redirection table ───────────────────────────────────────────────
// Each entry: 64-bit split over two 32-bit registers at offset 0x10 + irq*2
void ioapic_set_irq(u8 irq, u8 vector, u8 dest) {
    u32 reg_lo = IOAPIC_REDTBL + (u32)(irq * 2);
    u32 reg_hi = reg_lo + 1;
    // High: destination LAPIC ID in bits [63:56]
    ioapic_write(reg_hi, (u32)dest << 24);
    // Low: vector, delivery mode=0 (Fixed), dest mode=0 (Physical),
    //      polarity=0 (active high), trigger=0 (edge), unmasked
    ioapic_write(reg_lo, (u32)vector);
}

void ioapic_mask_irq(u8 irq) {
    u32 reg_lo = IOAPIC_REDTBL + (u32)(irq * 2);
    u32 val = ioapic_read(reg_lo);
    ioapic_write(reg_lo, val | LAPIC_LVT_MASKED);
}

void ioapic_unmask_irq(u8 irq) {
    u32 reg_lo = IOAPIC_REDTBL + (u32)(irq * 2);
    u32 val = ioapic_read(reg_lo);
    // Set vector to IRQ + 32 if not already set
    if ((val & 0xFF) < 32) val = (val & ~0xFFu) | ((u32)irq + 32);
    ioapic_write(reg_lo, val & ~(u32)LAPIC_LVT_MASKED);
}
