// PolandOS — Advanced Programmable Interrupt Controller
#pragma once
#include "../../../../include/types.h"

void apic_init(void);
void apic_send_eoi(void);
void ioapic_set_irq(u8 irq, u8 vector, u8 dest);
void ioapic_mask_irq(u8 irq);
void ioapic_unmask_irq(u8 irq);
extern u64 lapic_base;
