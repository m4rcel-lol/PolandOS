// PolandOS — Virtual Memory Manager (4-level paging)
#pragma once
#include "../../../../include/types.h"

#define PAGE_SIZE 0x1000ULL
#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITE      (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_PWT        (1ULL << 3)
#define PAGE_PCD        (1ULL << 4)
#define PAGE_ACCESSED   (1ULL << 5)
#define PAGE_DIRTY      (1ULL << 6)
#define PAGE_HUGE       (1ULL << 7)
#define PAGE_GLOBAL     (1ULL << 8)
#define PAGE_NX         (1ULL << 63)

#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL

void vmm_init(u64 hhdm_offset, u64 kernel_phys, u64 kernel_virt, u64 kernel_size);
void vmm_map(u64 virt, u64 phys, u64 flags);
void vmm_unmap(u64 virt);
u64  vmm_get_phys(u64 virt);
u64 *vmm_get_pml4(void);
