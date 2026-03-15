// PolandOS — Physical Memory Manager
#pragma once
#include "../../../../include/types.h"

void  pmm_init(void *memmap_entries, u64 count, u64 hhdm_offset);
u64   pmm_alloc(void);       // returns physical address of 4K frame, or 0 on fail
void  pmm_free(u64 phys);
u64   pmm_alloc_pages(u64 count); // contiguous pages
u64   pmm_total_bytes(void);
u64   pmm_used_bytes(void);
u64   pmm_free_bytes(void);
extern u64 hhdm_offset;
