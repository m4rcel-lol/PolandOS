// PolandOS — Sterownik PCI/PCIe
// Obsluguje zarówno legacy I/O port (CF8/CFC) jak i ECAM (PCIe MMIO)
#include "pci.h"
#include "../../include/io.h"
#include "../lib/printf.h"
#include "../lib/string.h"

// ─── PCI legacy I/O port constants ───────────────────────────────────────────
#define PCI_ADDR_PORT  0xCF8
#define PCI_DATA_PORT  0xCFC

// ─── PCI config space offsets ────────────────────────────────────────────────
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION        0x08
#define PCI_PROG_IF         0x09
#define PCI_SUBCLASS        0x0A
#define PCI_CLASS           0x0B
#define PCI_CACHE_LINE      0x0C
#define PCI_LATENCY         0x0D
#define PCI_HEADER_TYPE     0x0E
#define PCI_BIST            0x0F
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_SUBSYS_VENDOR   0x2C
#define PCI_SUBSYS_DEVICE   0x2E
#define PCI_IRQ_LINE        0x3C
#define PCI_IRQ_PIN         0x3D

#define PCI_CMD_BUSMASTER   (1 << 2)
#define PCI_CMD_MMIO        (1 << 1)
#define PCI_CMD_IO          (1 << 0)

// ─── Globals ─────────────────────────────────────────────────────────────────
PCIDevice pci_devices[PCI_MAX_DEVICES];
int       pci_device_count = 0;

static u64 pci_mcfg_base = 0;   // ECAM physical base (0 = use I/O ports)
static u64 pci_hhdm      = 0;

// ─── ECAM address calculation ────────────────────────────────────────────────
// base + (bus << 20) | (slot << 15) | (func << 12) | offset
static inline volatile u32 *ecam_ptr(u8 bus, u8 slot, u8 func, u8 offset) {
    u64 addr = pci_mcfg_base + pci_hhdm
             + ((u64)bus  << 20)
             + ((u64)slot << 15)
             + ((u64)func << 12)
             + (offset & 0xFFC);
    return (volatile u32 *)addr;
}

// ─── Config space read/write ─────────────────────────────────────────────────
u32 pci_read32(u8 bus, u8 slot, u8 func, u8 offset) {
    if (pci_mcfg_base) {
        volatile u32 *p = ecam_ptr(bus, slot, func, offset);
        return *p;
    }
    u32 addr = 0x80000000u
             | ((u32)bus  << 16)
             | ((u32)slot << 11)
             | ((u32)func <<  8)
             | (offset & 0xFC);
    outl(PCI_ADDR_PORT, addr);
    return inl(PCI_DATA_PORT);
}

u16 pci_read16(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 val = pci_read32(bus, slot, func, offset & 0xFC);
    return (u16)(val >> ((offset & 2) * 8));
}

u8 pci_read8(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 val = pci_read32(bus, slot, func, offset & 0xFC);
    return (u8)(val >> ((offset & 3) * 8));
}

void pci_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 val) {
    if (pci_mcfg_base) {
        volatile u32 *p = ecam_ptr(bus, slot, func, offset);
        *p = val;
        return;
    }
    u32 addr = 0x80000000u
             | ((u32)bus  << 16)
             | ((u32)slot << 11)
             | ((u32)func <<  8)
             | (offset & 0xFC);
    outl(PCI_ADDR_PORT, addr);
    outl(PCI_DATA_PORT, val);
}

void pci_write16(u8 bus, u8 slot, u8 func, u8 offset, u16 val) {
    u32 cur = pci_read32(bus, slot, func, offset & 0xFC);
    u32 shift = (offset & 2) * 8;
    cur &= ~(0xFFFFu << shift);
    cur |= (u32)val << shift;
    pci_write32(bus, slot, func, offset & 0xFC, cur);
}

void pci_write8(u8 bus, u8 slot, u8 func, u8 offset, u8 val) {
    u32 cur = pci_read32(bus, slot, func, offset & 0xFC);
    u32 shift = (offset & 3) * 8;
    cur &= ~(0xFFu << shift);
    cur |= (u32)val << shift;
    pci_write32(bus, slot, func, offset & 0xFC, cur);
}

// ─── Bus master enable ───────────────────────────────────────────────────────
void pci_enable_busmaster(u8 bus, u8 slot, u8 func) {
    u16 cmd = pci_read16(bus, slot, func, PCI_COMMAND);
    cmd |= PCI_CMD_BUSMASTER | PCI_CMD_MMIO | PCI_CMD_IO;
    pci_write16(bus, slot, func, PCI_COMMAND, cmd);
}

// ─── BAR address (handles 32-bit and 64-bit) ─────────────────────────────────
u64 pci_get_bar_addr(u8 bus, u8 slot, u8 func, u8 bar_idx) {
    if (bar_idx > 5) return 0;
    u8  bar_off = (u8)(PCI_BAR0 + bar_idx * 4);
    u32 bar_lo  = pci_read32(bus, slot, func, bar_off);

    if (bar_lo & 1) {
        // I/O BAR
        return (u64)(bar_lo & 0xFFFFFFFC);
    }

    u32 bar_type = (bar_lo >> 1) & 0x3;
    if (bar_type == 2 && bar_idx < 5) {
        // 64-bit MMIO BAR
        u32 bar_hi = pci_read32(bus, slot, func, (u8)(bar_off + 4));
        u64 addr = ((u64)bar_hi << 32) | (bar_lo & 0xFFFFFFF0u);
        // Keep only architecturally valid physical address bits (x86_64 max 52-bit).
        return addr & 0x000FFFFFFFFFFFF0ULL;
    }

    // 32-bit MMIO BAR
    return (u64)(bar_lo & 0xFFFFFFF0);
}

// ─── Device search ───────────────────────────────────────────────────────────
PCIDevice *pci_find_device(u16 vendor, u16 device) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor &&
            pci_devices[i].device_id == device)
            return &pci_devices[i];
    }
    return NULL;
}

PCIDevice *pci_find_class(u8 class_code, u8 subclass) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code &&
            pci_devices[i].subclass   == subclass)
            return &pci_devices[i];
    }
    return NULL;
}

// ─── Device enumeration ──────────────────────────────────────────────────────
static void pci_probe_function(u8 bus, u8 slot, u8 func) {
    u16 vendor = pci_read16(bus, slot, func, PCI_VENDOR_ID);
    if (vendor == 0xFFFF) return;

    if (pci_device_count >= PCI_MAX_DEVICES) return;

    PCIDevice *dev = &pci_devices[pci_device_count++];
    dev->vendor_id = vendor;
    dev->device_id = pci_read16(bus, slot, func, PCI_DEVICE_ID);
    dev->class_code   = pci_read8(bus, slot, func, PCI_CLASS);
    dev->subclass     = pci_read8(bus, slot, func, PCI_SUBCLASS);
    dev->prog_if      = pci_read8(bus, slot, func, PCI_PROG_IF);
    dev->header_type  = pci_read8(bus, slot, func, PCI_HEADER_TYPE) & 0x7F;
    dev->bus  = bus;
    dev->slot = slot;
    dev->func = func;
    dev->irq_line     = pci_read8(bus, slot, func, PCI_IRQ_LINE);
    dev->irq_pin      = pci_read8(bus, slot, func, PCI_IRQ_PIN);

    if (dev->header_type == 0x00) {
        for (int b = 0; b < 6; b++)
            dev->bar[b] = pci_read32(bus, slot, func, (u8)(PCI_BAR0 + b * 4));
        dev->subsystem_vendor = pci_read16(bus, slot, func, PCI_SUBSYS_VENDOR);
        dev->subsystem_device = pci_read16(bus, slot, func, PCI_SUBSYS_DEVICE);
    }
}

void pci_init(u64 mcfg_base, u64 hhdm) {
    pci_mcfg_base    = mcfg_base;
    pci_hhdm         = hhdm;
    pci_device_count = 0;

    for (u16 bus = 0; bus < 256; bus++) {
        for (u8 slot = 0; slot < 32; slot++) {
            u16 vendor = pci_read16((u8)bus, slot, 0, PCI_VENDOR_ID);
            if (vendor == 0xFFFF) continue;

            pci_probe_function((u8)bus, slot, 0);

            // Check for multi-function device
            u8 htype = pci_read8((u8)bus, slot, 0, PCI_HEADER_TYPE);
            if (htype & 0x80) {
                for (u8 func = 1; func < 8; func++)
                    pci_probe_function((u8)bus, slot, func);
            }
        }
    }

    kprintf("[DOBRZE] PCI: znaleziono %d urządzeń\n", pci_device_count);
}

// ─── List all devices ─────────────────────────────────────────────────────────
static const char *pci_class_name(u8 class_code) {
    switch (class_code) {
    case 0x00: return "Legacy";
    case 0x01: return "Storage";
    case 0x02: return "Network";
    case 0x03: return "Display";
    case 0x04: return "Multimedia";
    case 0x05: return "Memory";
    case 0x06: return "Bridge";
    case 0x07: return "Comm";
    case 0x08: return "System";
    case 0x09: return "Input";
    case 0x0C: return "Serial Bus";
    case 0x0D: return "Wireless";
    case 0x0F: return "Satellite";
    case 0x11: return "Crypto";
    default:   return "Unknown";
    }
}

void pci_list_devices(void) {
    kprintf("[PCI] Lista urządzeń (%d):\n", pci_device_count);
    for (int i = 0; i < pci_device_count; i++) {
        PCIDevice *d = &pci_devices[i];
        kprintf("  [%02x:%02x.%x] %04x:%04x cls=%02x.%02x (%s) IRQ=%u\n",
                d->bus, d->slot, d->func,
                d->vendor_id, d->device_id,
                d->class_code, d->subclass,
                pci_class_name(d->class_code),
                d->irq_line);
    }
}
