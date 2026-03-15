// PolandOS — Zegar HPET (High Precision Event Timer)
#pragma once
#include "../../include/types.h"

extern volatile u64 kernel_ticks;  // ticks at 1000 Hz

void hpet_init(u64 hpet_phys, u64 hhdm);
void timer_sleep_ms(u64 ms);
u64  timer_get_ticks(void);
u64  timer_get_ms(void);
