// PolandOS — Sterownik GPU (detekcja kart graficznych)
// Wykrywanie kontrolerow graficznych przez szyne PCI
#include "gpu.h"
#include "pci.h"
#include "../lib/printf.h"

// PCI display controller class
#define PCI_CLASS_DISPLAY  0x03

// PCI display subclasses
#define PCI_SUBCLASS_VGA        0x00
#define PCI_SUBCLASS_XGA        0x01
#define PCI_SUBCLASS_3D         0x02
#define PCI_SUBCLASS_DISPLAY_OTHER 0x80

// ─── Known GPU vendors ───────────────────────────────────────────────────────
static const char *gpu_vendor_name(u16 vendor_id) {
    switch (vendor_id) {
    case 0x1234: return "QEMU/Bochs";
    case 0x1AF4: return "Red Hat/virtio";
    case 0x10DE: return "NVIDIA";
    case 0x1002: return "AMD/ATI";
    case 0x8086: return "Intel";
    case 0x1013: return "Cirrus Logic";
    case 0x15AD: return "VMware";
    case 0x1AB8: return "Parallels";
    case 0x80EE: return "VirtualBox";
    default:     return "Unknown";
    }
}

static const char *gpu_subclass_name(u8 subclass) {
    switch (subclass) {
    case PCI_SUBCLASS_VGA:           return "VGA Controller";
    case PCI_SUBCLASS_XGA:           return "XGA Controller";
    case PCI_SUBCLASS_3D:            return "3D Controller";
    case PCI_SUBCLASS_DISPLAY_OTHER: return "Display Controller";
    default:                         return "Display Device";
    }
}

// ─── Globals ─────────────────────────────────────────────────────────────────
GPUDevice gpu_devices[GPU_MAX_DEVICES];
int       gpu_device_count = 0;

// ─── GPU init: scan PCI bus for display controllers ──────────────────────────
void gpu_init(void) {
    gpu_device_count = 0;

    for (int i = 0; i < pci_device_count && gpu_device_count < GPU_MAX_DEVICES; i++) {
        PCIDevice *pdev = &pci_devices[i];

        if (pdev->class_code != PCI_CLASS_DISPLAY)
            continue;

        GPUDevice *gpu = &gpu_devices[gpu_device_count++];
        gpu->vendor_id   = pdev->vendor_id;
        gpu->device_id   = pdev->device_id;
        gpu->class_code  = pdev->class_code;
        gpu->subclass    = pdev->subclass;
        gpu->prog_if     = pdev->prog_if;
        gpu->bus         = pdev->bus;
        gpu->slot        = pdev->slot;
        gpu->func        = pdev->func;
        gpu->bar0        = pci_get_bar_addr(pdev->bus, pdev->slot, pdev->func, 0);
        gpu->type_name   = gpu_subclass_name(pdev->subclass);
        gpu->vendor_name = gpu_vendor_name(pdev->vendor_id);

        kprintf("[DOBRZE] GPU: %s %s [%04x:%04x] @ %02x:%02x.%x BAR0=0x%llx\n",
                gpu->vendor_name, gpu->type_name,
                gpu->vendor_id, gpu->device_id,
                gpu->bus, gpu->slot, gpu->func,
                gpu->bar0);
    }

    if (gpu_device_count == 0) {
        kprintf("[INFO]   GPU: brak kontrolera graficznego na szynie PCI\n");
    } else {
        kprintf("[DOBRZE] GPU: znaleziono %d urzadzen graficznych\n",
                gpu_device_count);
    }
}

// ─── List GPU devices (shell command) ────────────────────────────────────────
void gpu_list_devices(void) {
    if (gpu_device_count == 0) {
        kprintf("[GPU] Brak wykrytych kart graficznych\n");
        return;
    }

    kprintf("[GPU] Wykryte karty graficzne (%d):\n", gpu_device_count);
    for (int i = 0; i < gpu_device_count; i++) {
        GPUDevice *g = &gpu_devices[i];
        kprintf("  [%d] %s %s\n", i, g->vendor_name, g->type_name);
        kprintf("      PCI %02x:%02x.%x  ID %04x:%04x\n",
                g->bus, g->slot, g->func,
                g->vendor_id, g->device_id);
        kprintf("      BAR0: 0x%llx\n", g->bar0);
    }
}
