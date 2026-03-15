// PolandOS — GDT (Global Descriptor Table) dla x86-64
// 7 deskryptorów: null, kernel code, kernel data, user data, user code, TSS low, TSS high
#include "gdt.h"
#include "../../../lib/string.h"

// ─── Structures ──────────────────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
    u16 limit_lo;
    u16 base_lo;
    u8  base_mid;
    u8  access;     // P DPL S Type
    u8  flags_limit_hi; // G DB L AVL | limit[19:16]
    u8  base_hi;
} GDTEntry;

typedef struct __attribute__((packed)) {
    u32 reserved0;
    u64 rsp[3];     // RSP0, RSP1, RSP2
    u64 reserved1;
    u64 ist[7];     // IST1..IST7
    u64 reserved2;
    u16 reserved3;
    u16 iopb_offset;
} TSS64;

typedef struct __attribute__((packed)) {
    u16 limit;
    u64 base;
} GDTR;

// ─── GDT entries ─────────────────────────────────────────────────────────────
// Index 0: null
// Index 1: kernel code  (0x08)
// Index 2: kernel data  (0x10)
// Index 3: user data    (0x18)  — must be before user code for syscall
// Index 4: user code    (0x20)
// Index 5: TSS low      (0x28)
// Index 6: TSS high     (0x30)
static GDTEntry gdt_entries[7];
static TSS64    tss_entry;
static GDTR     gdtr;

// ─── Assembly helpers ─────────────────────────────────────────────────────────
extern void gdt_flush(GDTR *gdtr_ptr);
extern void tss_flush(void);

// ─── Helper ──────────────────────────────────────────────────────────────────
static void gdt_set_entry(int idx, u32 base, u32 limit, u8 access, u8 flags) {
    gdt_entries[idx].limit_lo       = (u16)(limit & 0xFFFF);
    gdt_entries[idx].base_lo        = (u16)(base  & 0xFFFF);
    gdt_entries[idx].base_mid       = (u8)((base  >> 16) & 0xFF);
    gdt_entries[idx].access         = access;
    gdt_entries[idx].flags_limit_hi = (u8)(flags & 0xF0) | (u8)((limit >> 16) & 0x0F);
    gdt_entries[idx].base_hi        = (u8)((base  >> 24) & 0xFF);
}

// Set up a 16-byte system descriptor for the TSS (occupies slots 5 and 6)
static void gdt_set_tss_descriptor(u64 base, u32 limit) {
    // Low 8 bytes in slot 5
    gdt_entries[5].limit_lo       = (u16)(limit & 0xFFFF);
    gdt_entries[5].base_lo        = (u16)(base  & 0xFFFF);
    gdt_entries[5].base_mid       = (u8)((base  >> 16) & 0xFF);
    gdt_entries[5].access         = 0x89; // Present, DPL=0, S=0 (system), Type=9 (64-bit TSS available)
    gdt_entries[5].flags_limit_hi = (u8)((limit >> 16) & 0x0F); // G=0 (byte granularity)
    gdt_entries[5].base_hi        = (u8)((base  >> 24) & 0xFF);

    // High 8 bytes in slot 6 (upper 32 bits of base + reserved)
    u32 *slot6 = (u32 *)&gdt_entries[6];
    slot6[0] = (u32)(base >> 32);
    slot6[1] = 0;
}

void gdt_init(void) {
    // Zero everything
    memset(gdt_entries, 0, sizeof(gdt_entries));
    memset(&tss_entry,  0, sizeof(tss_entry));

    // 0: Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);

    // 1: Kernel code — 64-bit, DPL=0
    // Access: P=1, DPL=00, S=1, Type=1010 (execute/read) → 0x9A
    // Flags:  G=1, L=1 (64-bit), D/B=0, AVL=0 → 0xA0
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    // 2: Kernel data — 32/64-bit data, DPL=0
    // Access: P=1, DPL=00, S=1, Type=0010 (read/write) → 0x92
    // Flags:  G=1, D/B=1 → 0xC0
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xC0);

    // 3: User data — DPL=3
    // Access: P=1, DPL=11, S=1, Type=0010 → 0xF2
    // Flags:  G=1, D/B=1 → 0xC0
    gdt_set_entry(3, 0, 0xFFFFF, 0xF2, 0xC0);

    // 4: User code — 64-bit, DPL=3
    // Access: P=1, DPL=11, S=1, Type=1010 → 0xFA
    // Flags:  G=1, L=1 → 0xA0
    gdt_set_entry(4, 0, 0xFFFFF, 0xFA, 0xA0);

    // 5+6: TSS
    tss_entry.iopb_offset = sizeof(TSS64);
    gdt_set_tss_descriptor((u64)&tss_entry, sizeof(TSS64) - 1);

    // Load GDT
    gdtr.limit = (u16)(sizeof(gdt_entries) - 1);
    gdtr.base  = (u64)gdt_entries;
    gdt_flush(&gdtr);

    // Load TSS
    tss_flush();
}

void gdt_set_tss_rsp0(u64 rsp0) {
    tss_entry.rsp[0] = rsp0;
}
