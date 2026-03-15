// PolandOS — Global Descriptor Table
#pragma once
#include "../../../../include/types.h"

void gdt_init(void);
void gdt_set_tss_rsp0(u64 rsp0);
