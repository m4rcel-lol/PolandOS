// PolandOS — Sterownik RTC (Real Time Clock)
#pragma once
#include "../../include/types.h"

typedef struct {
    u8 seconds;
    u8 minutes;
    u8 hours;
    u8 day;
    u8 month;
    u16 year;
} RTCTime;

void rtc_init(void);
RTCTime rtc_read(void);
