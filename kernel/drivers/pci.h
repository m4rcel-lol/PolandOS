// PolandOS — Sterownik PCI/PCIe
#pragma once
#include "../../include/types.h"

typedef struct {
    u16 vendor_id;
    u16 device_id;
    u8  class_code;
    u8  subclass;
    u8  prog_if;
    u8  header_type;
    u8  bus, slot, func;
    u32 bar[6];
    u8  irq_line;
    u8  irq_pin;
    u16 subsystem_vendor;
    u16 subsystem_device;
} PCIDevice;

#define PCI_MAX_DEVICES 256

extern PCIDevice pci_devices[PCI_MAX_DEVICES];
extern int pci_device_count;

void  pci_init(u64 mcfg_base, u64 hhdm);
u32   pci_read32(u8 bus, u8 slot, u8 func, u8 offset);
u16   pci_read16(u8 bus, u8 slot, u8 func, u8 offset);
u8    pci_read8(u8 bus, u8 slot, u8 func, u8 offset);
void  pci_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 val);
void  pci_write16(u8 bus, u8 slot, u8 func, u8 offset, u16 val);
void  pci_write8(u8 bus, u8 slot, u8 func, u8 offset, u8 val);
PCIDevice *pci_find_device(u16 vendor, u16 device);
PCIDevice *pci_find_class(u8 class_code, u8 subclass);
void  pci_enable_busmaster(u8 bus, u8 slot, u8 func);
u64   pci_get_bar_addr(u8 bus, u8 slot, u8 func, u8 bar_idx);
void  pci_list_devices(void);
