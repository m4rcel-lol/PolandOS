// PolandOS — Virtual Memory Manager (4-poziomowe stronicowanie)
// Obsługuje mapowanie/odmapowanie stron, tworzenie PML4
#include "vmm.h"
#include "pmm.h"
#include "../../../lib/string.h"
#include "../../../lib/panic.h"
#include "../../../../include/io.h"

// ─── Page table indices ───────────────────────────────────────────────────────
#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v)   (((v) >> 21) & 0x1FF)
#define PT_IDX(v)   (((v) >> 12) & 0x1FF)

#define PHYS_ADDR_MASK 0x000FFFFFFFFFF000ULL

static u64 *kernel_pml4 = (u64 *)0;

// NOTE: vmm_init requires hhdm_offset to already be set by pmm_init().
static u64 *phys_to_virt(u64 phys) {
    return (u64 *)(phys + hhdm_offset);
}

// Get or create a child page table
static u64 *get_or_create_table(u64 *table, u64 index, u64 flags) {
    if (table[index] & PAGE_PRESENT) {
        return phys_to_virt(table[index] & PHYS_ADDR_MASK);
    }
    u64 phys = pmm_alloc();
    if (!phys) kpanic("VMM: brak pamieci na tablice stron!\n");
    // pmm_alloc already zeroes the page
    table[index] = phys | flags | PAGE_PRESENT;
    return phys_to_virt(phys);
}

void vmm_map(u64 virt, u64 phys, u64 flags) {
    u64 *pdpt = get_or_create_table(kernel_pml4, PML4_IDX(virt), PAGE_WRITE | PAGE_PRESENT);
    u64 *pd   = get_or_create_table(pdpt, PDPT_IDX(virt), PAGE_WRITE | PAGE_PRESENT);
    u64 *pt   = get_or_create_table(pd,   PD_IDX(virt),   PAGE_WRITE | PAGE_PRESENT);

    pt[PT_IDX(virt)] = (phys & PHYS_ADDR_MASK) | flags | PAGE_PRESENT;
    invlpg(virt);
}

void vmm_unmap(u64 virt) {
    if (!kernel_pml4) return;

    u64 pml4e = kernel_pml4[PML4_IDX(virt)];
    if (!(pml4e & PAGE_PRESENT)) return;

    u64 *pdpt = phys_to_virt(pml4e & PHYS_ADDR_MASK);
    u64 pdpte = pdpt[PDPT_IDX(virt)];
    if (!(pdpte & PAGE_PRESENT)) return;

    u64 *pd  = phys_to_virt(pdpte & PHYS_ADDR_MASK);
    u64 pde  = pd[PD_IDX(virt)];
    if (!(pde & PAGE_PRESENT)) return;

    u64 *pt  = phys_to_virt(pde & PHYS_ADDR_MASK);
    pt[PT_IDX(virt)] = 0;
    invlpg(virt);
}

u64 vmm_get_phys(u64 virt) {
    if (!kernel_pml4) return 0;

    u64 pml4e = kernel_pml4[PML4_IDX(virt)];
    if (!(pml4e & PAGE_PRESENT)) return 0;

    u64 *pdpt = phys_to_virt(pml4e & PHYS_ADDR_MASK);
    u64 pdpte = pdpt[PDPT_IDX(virt)];
    if (!(pdpte & PAGE_PRESENT)) return 0;

    // Check for 1GB huge page
    if (pdpte & PAGE_HUGE)
        return (pdpte & ~0x3FFFFFFFULL) | (virt & 0x3FFFFFFFULL);

    u64 *pd  = phys_to_virt(pdpte & PHYS_ADDR_MASK);
    u64 pde  = pd[PD_IDX(virt)];
    if (!(pde & PAGE_PRESENT)) return 0;

    // Check for 2MB huge page
    if (pde & PAGE_HUGE)
        return (pde & ~0x1FFFFFULL) | (virt & 0x1FFFFFULL);

    u64 *pt  = phys_to_virt(pde & PHYS_ADDR_MASK);
    u64 pte  = pt[PT_IDX(virt)];
    if (!(pte & PAGE_PRESENT)) return 0;

    return (pte & PHYS_ADDR_MASK) | (virt & 0xFFF);
}

u64 *vmm_get_pml4(void) { return kernel_pml4; }

// Allocate physical frames and map a contiguous virtual region
void vmm_map_region(u64 virt, u64 size, u64 flags) {
    u64 aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    for (u64 off = 0; off < aligned_size; off += PAGE_SIZE) {
        u64 phys = pmm_alloc();
        if (!phys) kpanic("vmm_map_region: brak pamieci fizycznej!\n");
        vmm_map(virt + off, phys, flags);
    }
}

void vmm_init(u64 hhdm_off, u64 kernel_phys, u64 kernel_virt, u64 kernel_size) {
    // hhdm_offset should already be set by pmm_init, but just in case
    (void)hhdm_off;

    // Allocate new PML4
    u64 pml4_phys = pmm_alloc();
    if (!pml4_phys) kpanic("VMM: nie mozna przydzielic PML4!\n");
    kernel_pml4 = phys_to_virt(pml4_phys);

    // Map HHDM: cover all physical memory seen in the memory map.
    // Use 2MB pages (huge) for efficiency.
    // Minimum 4 GB, rounded up to next 2MB boundary.
    u64 max_phys = pmm_get_max_phys();
    if (max_phys < 4ULL * 1024 * 1024 * 1024)
        max_phys = 4ULL * 1024 * 1024 * 1024;
    u64 hhdm_map_size = (max_phys + 0x1FFFFFULL) & ~0x1FFFFFULL; // round to 2MB
    for (u64 phys = 0; phys < hhdm_map_size; phys += 0x200000) {
        u64 virt = phys + hhdm_offset;
        u64 *pdpt = get_or_create_table(kernel_pml4, PML4_IDX(virt), PAGE_WRITE | PAGE_PRESENT);
        u64 *pd   = get_or_create_table(pdpt, PDPT_IDX(virt), PAGE_WRITE | PAGE_PRESENT);
        pd[PD_IDX(virt)] = phys | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE | PAGE_GLOBAL;
    }

    // Map kernel to higher half (KERNEL_VIRT_BASE) using 4K pages
    u64 ksize_aligned = (kernel_size + 0xFFF) & ~0xFFFULL;
    for (u64 off = 0; off < ksize_aligned; off += PAGE_SIZE) {
        vmm_map(kernel_virt + off, kernel_phys + off,
                PAGE_PRESENT | PAGE_WRITE | PAGE_GLOBAL);
    }

    // Switch to new page tables
    write_cr3(pml4_phys);
}
