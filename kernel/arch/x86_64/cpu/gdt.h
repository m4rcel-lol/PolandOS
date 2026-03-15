// PolandOS — Global Descriptor Table
#pragma once
#include "../../../../include/types.h"

void gdt_init(void);
void gdt_set_tss_rsp0(u64 rsp0);
void gdt_setup_ist_stacks(void); // call after pmm_init + vmm_init
