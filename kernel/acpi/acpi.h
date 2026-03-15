// PolandOS — Parsowanie ACPI
// Jadro Orzel — analiza tabel systemowych
#pragma once
#include "../../include/types.h"

typedef struct __attribute__((packed)) {
    char signature[8];
    u8   checksum;
    char oem_id[6];
    u8   revision;  // 0=RSDP v1 (RSDT), 2=RSDP v2 (XSDT)
    u32  rsdt_addr;
    // v2 fields:
    u32  length;
    u64  xsdt_addr;
    u8   ext_checksum;
    u8   reserved[3];
} RSDP;

typedef struct __attribute__((packed)) {
    char signature[4];
    u32  length;
    u8   revision;
    u8   checksum;
    char oem_id[6];
    char oem_table_id[8];
    u32  oem_revision;
    u32  creator_id;
    u32  creator_revision;
} ACPITableHeader;

typedef struct __attribute__((packed)) {
    ACPITableHeader header;
    u32 entries[];  // physical addresses for RSDT
} RSDT;

typedef struct __attribute__((packed)) {
    ACPITableHeader header;
    u64 entries[];  // physical addresses for XSDT
} XSDT;

// FADT (simplified, just power fields we need)
typedef struct __attribute__((packed)) {
    ACPITableHeader header;
    u32 firmware_ctrl;
    u32 dsdt;
    u8  reserved1;
    u8  preferred_pm_profile;
    u16 sci_int;
    u32 smi_cmd;
    u8  acpi_enable;
    u8  acpi_disable;
    u8  s4bios_req;
    u8  pstate_cnt;
    u32 pm1a_evt_blk;
    u32 pm1b_evt_blk;
    u32 pm1a_cnt_blk;
    u32 pm1b_cnt_blk;
    u32 pm2_cnt_blk;
    u32 pm_tmr_blk;
    u32 gpe0_blk;
    u32 gpe1_blk;
    u8  pm1_evt_len;
    u8  pm1_cnt_len;
    u8  pm2_cnt_len;
    u8  pm_tmr_len;
    u8  gpe0_blk_len;
    u8  gpe1_blk_len;
    u8  gpe1_base;
    u8  cst_cnt;
    u16 p_lvl2_lat;
    u16 p_lvl3_lat;
    u16 flush_size;
    u16 flush_stride;
    u8  duty_offset;
    u8  duty_width;
    u8  day_alrm;
    u8  mon_alrm;
    u8  century;
    u16 iapc_boot_arch;
    u8  reserved2;
    u32 flags;
    // GAS structure for reset register
    u8  reset_reg[12]; // GAS: addr_space(1), bit_width(1), bit_offset(1), access_size(1), address(8)
    u8  reset_value;
    u8  reserved3[3];
    u64 x_firmware_ctrl;
    u64 x_dsdt;
    // ... more fields we skip
} FADT;

// MADT
typedef struct __attribute__((packed)) {
    ACPITableHeader header;
    u32 lapic_addr;
    u32 flags;
    u8  entries[];
} MADT;

// MADT entry header
typedef struct __attribute__((packed)) {
    u8 type;
    u8 length;
} MADTEntry;

// MADT Local APIC (type 0)
typedef struct __attribute__((packed)) {
    MADTEntry header;
    u8  acpi_cpu_id;
    u8  apic_id;
    u32 flags;  // bit 0: processor enabled, bit 1: online capable
} MADTLocalAPIC;

// MADT I/O APIC (type 1)
typedef struct __attribute__((packed)) {
    MADTEntry header;
    u8  ioapic_id;
    u8  reserved;
    u32 ioapic_addr;
    u32 gsi_base;
} MADTIOApic;

// MADT Interrupt Source Override (type 2)
typedef struct __attribute__((packed)) {
    MADTEntry header;
    u8  bus;       // 0 = ISA
    u8  irq;       // source IRQ
    u32 gsi;       // global system interrupt
    u16 flags;
} MADTIntOverride;

// HPET table
typedef struct __attribute__((packed)) {
    ACPITableHeader header;
    u32 event_timer_block_id;
    u8  base_address[12]; // GAS
    u8  hpet_number;
    u16 minimum_tick;
    u8  page_protection;
} HPETTable;

// MCFG table
typedef struct __attribute__((packed)) {
    ACPITableHeader header;
    u64 reserved;
    // followed by MCFG allocation structures
} MCFGTable;

typedef struct __attribute__((packed)) {
    u64 base_address;
    u16 segment_group;
    u8  start_bus;
    u8  end_bus;
    u32 reserved;
} MCFGAllocation;

// Parsed ACPI data
extern u64 acpi_lapic_phys;
extern u64 acpi_ioapic_phys;
extern u32 acpi_ioapic_gsi_base;
extern u32 acpi_pm1a_cnt;
extern u32 acpi_pm1b_cnt;
extern u8  acpi_pm1_cnt_len;
extern u64 acpi_hpet_phys;
extern u64 acpi_mcfg_phys;
extern u64 acpi_mcfg_len;
extern u64 acpi_fadt_reset_addr;
extern u8  acpi_fadt_reset_value;
extern u8  acpi_fadt_reset_reg_space; // 0=sys memory, 1=I/O port

void acpi_init(u64 rsdp_phys, u64 hhdm);
void acpi_power_off(void);
void acpi_reset(void);
bool acpi_validate_checksum(ACPITableHeader *header);
ACPITableHeader *acpi_find_table(const char *sig);
