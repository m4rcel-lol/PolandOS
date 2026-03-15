// PolandOS — Implementacja ACPI
// Jadro Orzel — parsowanie tabel systemowych ACPI
#include "acpi.h"
#include "../../include/io.h"
#include "../lib/printf.h"
#include "../lib/string.h"

// ─── Exported globals ─────────────────────────────────────────────────────────
u64 acpi_lapic_phys        = 0;
u64 acpi_ioapic_phys       = 0;
u32 acpi_ioapic_gsi_base   = 0;
u32 acpi_pm1a_cnt          = 0;
u32 acpi_pm1b_cnt          = 0;
u8  acpi_pm1_cnt_len       = 0;
u64 acpi_hpet_phys         = 0;
u64 acpi_mcfg_phys         = 0;
u64 acpi_mcfg_len          = 0;
u64 acpi_fadt_reset_addr   = 0;
u8  acpi_fadt_reset_value  = 0;
u8  acpi_fadt_reset_reg_space = 0;

// ─── Internal state ───────────────────────────────────────────────────────────
#define ACPI_MAX_TABLES 64

static ACPITableHeader *acpi_tables[ACPI_MAX_TABLES];
static int              acpi_table_count = 0;
static u64              acpi_hhdm_base   = 0;

// ioapic_base lives in apic.c; update it when we find the MADT I/O APIC entry
extern u64 ioapic_base;

// ─── Helpers ──────────────────────────────────────────────────────────────────
static inline void *phys_to_virt(u64 phys) {
    return (void *)(phys + acpi_hhdm_base);
}

// ─── Checksum validation ──────────────────────────────────────────────────────
bool acpi_validate_checksum(ACPITableHeader *header) {
    u8 sum = 0;
    const u8 *bytes = (const u8 *)header;
    for (u32 i = 0; i < header->length; i++)
        sum += bytes[i];
    return sum == 0;
}

// Validate RSDP v1 checksum (first 20 bytes)
static bool rsdp_validate_v1(RSDP *rsdp) {
    u8 sum = 0;
    const u8 *bytes = (const u8 *)rsdp;
    for (int i = 0; i < 20; i++)
        sum += bytes[i];
    return sum == 0;
}

// ─── Table search ─────────────────────────────────────────────────────────────
ACPITableHeader *acpi_find_table(const char *sig) {
    for (int i = 0; i < acpi_table_count; i++) {
        if (memcmp(acpi_tables[i]->signature, sig, 4) == 0)
            return acpi_tables[i];
    }
    return NULL;
}

// ─── FADT parser ─────────────────────────────────────────────────────────────
static void parse_fadt(FADT *fadt) {
    acpi_pm1a_cnt    = fadt->pm1a_cnt_blk;
    acpi_pm1b_cnt    = fadt->pm1b_cnt_blk;
    acpi_pm1_cnt_len = fadt->pm1_cnt_len;

    // GAS for reset register: byte 0 = address space, bytes 4-11 = address
    acpi_fadt_reset_reg_space = fadt->reset_reg[0];
    acpi_fadt_reset_addr      = *(u64 *)(fadt->reset_reg + 4);
    acpi_fadt_reset_value     = fadt->reset_value;

    kprintf("[DOBRZE] ACPI FADT: PM1a_CNT=0x%x PM1b_CNT=0x%x reset_addr=0x%lx\n",
            acpi_pm1a_cnt, acpi_pm1b_cnt, acpi_fadt_reset_addr);
}

// ─── MADT parser ─────────────────────────────────────────────────────────────
static void parse_madt(MADT *madt) {
    acpi_lapic_phys = madt->lapic_addr;

    u32 entries_len = madt->header.length - sizeof(MADT);
    u8 *ptr = madt->entries;
    u8 *end = ptr + entries_len;

    while (ptr < end) {
        MADTEntry *entry = (MADTEntry *)ptr;
        if (entry->length < 2)
            break;

        switch (entry->type) {
        case 0: {  // Local APIC
            MADTLocalAPIC *lapic = (MADTLocalAPIC *)entry;
            if (lapic->flags & 0x1)
                kprintf("[DOBRZE] ACPI MADT: CPU APIC_ID=%u\n", lapic->apic_id);
            break;
        }
        case 1: {  // I/O APIC
            MADTIOApic *ioa = (MADTIOApic *)entry;
            if (acpi_ioapic_phys == 0) {
                acpi_ioapic_phys     = ioa->ioapic_addr;
                acpi_ioapic_gsi_base = ioa->gsi_base;
                ioapic_base          = ioa->ioapic_addr;
            }
            kprintf("[DOBRZE] ACPI MADT: IOAPIC addr=0x%x gsi_base=%u\n",
                    ioa->ioapic_addr, ioa->gsi_base);
            break;
        }
        case 5: {  // Local APIC Address Override (64-bit)
            u64 *addr64 = (u64 *)(ptr + 4);
            acpi_lapic_phys = *addr64;
            break;
        }
        default:
            break;
        }

        ptr += entry->length;
    }

    kprintf("[DOBRZE] ACPI MADT: LAPIC phys=0x%lx IOAPIC phys=0x%lx\n",
            acpi_lapic_phys, acpi_ioapic_phys);
}

// ─── HPET parser ─────────────────────────────────────────────────────────────
static void parse_hpet(HPETTable *hpet) {
    // GAS base_address: bytes 0-3 = address space info, bytes 4-11 = address
    acpi_hpet_phys = *(u64 *)(hpet->base_address + 4);
    kprintf("[DOBRZE] ACPI HPET: base=0x%lx number=%u min_tick=%u\n",
            acpi_hpet_phys, hpet->hpet_number, hpet->minimum_tick);
}

// ─── MCFG parser ─────────────────────────────────────────────────────────────
static void parse_mcfg(MCFGTable *mcfg) {
    // First allocation entry follows immediately after MCFGTable header
    MCFGAllocation *alloc = (MCFGAllocation *)((u8 *)mcfg + sizeof(MCFGTable));
    u32 remaining = mcfg->header.length - (u32)sizeof(MCFGTable);
    if (remaining >= sizeof(MCFGAllocation)) {
        acpi_mcfg_phys = alloc->base_address;
        acpi_mcfg_len  = mcfg->header.length;
        kprintf("[DOBRZE] ACPI MCFG: base=0x%lx seg=%u bus=%u-%u\n",
                alloc->base_address, alloc->segment_group,
                alloc->start_bus, alloc->end_bus);
    }
}

// ─── Main init ────────────────────────────────────────────────────────────────
void acpi_init(u64 rsdp_phys, u64 hhdm) {
    acpi_hhdm_base = hhdm;

    RSDP *rsdp = (RSDP *)phys_to_virt(rsdp_phys);

    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        kprintf("[BLAD] ACPI: nieprawidłowy podpis RSDP!\n");
        return;
    }

    if (!rsdp_validate_v1(rsdp)) {
        kprintf("[BLAD] ACPI: nieprawidłowy checksum RSDP!\n");
        return;
    }

    kprintf("[DOBRZE] ACPI: RSDP rev=%u OEM=%.6s\n",
            rsdp->revision, rsdp->oem_id);

    // Enumerate all tables
    if (rsdp->revision >= 2 && rsdp->xsdt_addr != 0) {
        // XSDT with 64-bit entries
        XSDT *xsdt = (XSDT *)phys_to_virt(rsdp->xsdt_addr);
        if (!acpi_validate_checksum(&xsdt->header)) {
            kprintf("[BLAD] ACPI: nieprawidłowy checksum XSDT!\n");
            return;
        }
        u32 n = (xsdt->header.length - sizeof(ACPITableHeader)) / 8;
        kprintf("[DOBRZE] ACPI: XSDT, %u tabel\n", n);
        for (u32 i = 0; i < n && acpi_table_count < ACPI_MAX_TABLES; i++) {
            ACPITableHeader *tbl = (ACPITableHeader *)phys_to_virt(xsdt->entries[i]);
            if (!acpi_validate_checksum(tbl)) continue;
            acpi_tables[acpi_table_count++] = tbl;
        }
    } else {
        // RSDT with 32-bit entries
        RSDT *rsdt = (RSDT *)phys_to_virt(rsdp->rsdt_addr);
        if (!acpi_validate_checksum(&rsdt->header)) {
            kprintf("[BLAD] ACPI: nieprawidłowy checksum RSDT!\n");
            return;
        }
        u32 n = (rsdt->header.length - sizeof(ACPITableHeader)) / 4;
        kprintf("[DOBRZE] ACPI: RSDT, %u tabel\n", n);
        for (u32 i = 0; i < n && acpi_table_count < ACPI_MAX_TABLES; i++) {
            ACPITableHeader *tbl = (ACPITableHeader *)phys_to_virt((u64)rsdt->entries[i]);
            if (!acpi_validate_checksum(tbl)) continue;
            acpi_tables[acpi_table_count++] = tbl;
        }
    }

    // Parse known tables
    ACPITableHeader *fadt_hdr = acpi_find_table("FACP");
    if (fadt_hdr) parse_fadt((FADT *)fadt_hdr);

    ACPITableHeader *madt_hdr = acpi_find_table("APIC");
    if (madt_hdr) parse_madt((MADT *)madt_hdr);

    ACPITableHeader *hpet_hdr = acpi_find_table("HPET");
    if (hpet_hdr) parse_hpet((HPETTable *)hpet_hdr);

    ACPITableHeader *mcfg_hdr = acpi_find_table("MCFG");
    if (mcfg_hdr) parse_mcfg((MCFGTable *)mcfg_hdr);

    kprintf("[DOBRZE] ACPI: zainicjalizowano, %d tabel załadowanych\n",
            acpi_table_count);
}

// ─── Power off ────────────────────────────────────────────────────────────────
void acpi_power_off(void) {
    // ACPI S5 shutdown via PM1a/PM1b control block
    // SLP_TYP=5 (QEMU S5), SLP_EN=bit13
    if (acpi_pm1a_cnt != 0) {
        outw((u16)acpi_pm1a_cnt, (u16)((5 << 10) | (1 << 13)));
        if (acpi_pm1b_cnt != 0)
            outw((u16)acpi_pm1b_cnt, (u16)((5 << 10) | (1 << 13)));
        // Short pause for hardware to process
        for (volatile int i = 0; i < 100000; i++)
            cpu_relax();
    }

    // QEMU PIIX4 ACPI power off (port 0x604)
    outw(0x604, 0x2000);
    for (volatile int i = 0; i < 100000; i++)
        cpu_relax();

    // Bochs/older QEMU shutdown port
    outw(0xB004, 0x2000);
    for (volatile int i = 0; i < 100000; i++)
        cpu_relax();

    // Last resort: ACPI reset then halt
    acpi_reset();

    // Halt
    cli();
    for (;;) hlt();
}

// ─── Reset ────────────────────────────────────────────────────────────────────
void acpi_reset(void) {
    // FADT reset register
    if (acpi_fadt_reset_addr != 0) {
        if (acpi_fadt_reset_reg_space == 1) {
            // I/O port
            outb((u16)acpi_fadt_reset_addr, acpi_fadt_reset_value);
        } else if (acpi_fadt_reset_reg_space == 0) {
            // System memory
            volatile u8 *addr = (volatile u8 *)(acpi_fadt_reset_addr + acpi_hhdm_base);
            *addr = acpi_fadt_reset_value;
        }
        for (volatile int i = 0; i < 100000; i++)
            cpu_relax();
    }

    // PS/2 keyboard controller reset (port 0x64, command 0xFE)
    outb(0x64, 0xFE);
    for (volatile int i = 0; i < 100000; i++)
        cpu_relax();

    // Halt if reset did not work
    cli();
    for (;;) hlt();
}
