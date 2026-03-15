// PolandOS — Sterownik CMOS RTC (Real Time Clock)
#include "rtc.h"
#include "../../include/io.h"
#include "../lib/printf.h"

// ─── CMOS ports & registers ──────────────────────────────────────────────────
#define CMOS_INDEX  0x70
#define CMOS_DATA   0x71

// Disable NMI while accessing CMOS by setting bit 7 of index port
#define CMOS_NMI_DISABLE  0x80

#define RTC_REG_SECONDS   0x00
#define RTC_REG_MINUTES   0x02
#define RTC_REG_HOURS     0x04
#define RTC_REG_DAY       0x07
#define RTC_REG_MONTH     0x08
#define RTC_REG_YEAR      0x09
#define RTC_REG_STATUS_A  0x0A
#define RTC_REG_STATUS_B  0x0B

// Status A bit 7: update in progress
#define RTC_UIP  (1 << 7)

// Status B bits
#define RTC_SB_24H    (1 << 1)  // 24-hour mode
#define RTC_SB_BINARY (1 << 2)  // binary mode (not BCD)

// ─── Low-level CMOS access ───────────────────────────────────────────────────
static u8 cmos_read(u8 reg) {
    outb(CMOS_INDEX, reg | CMOS_NMI_DISABLE);
    io_wait();
    return inb(CMOS_DATA);
}

// ─── Update-in-progress helpers ──────────────────────────────────────────────
static bool rtc_update_in_progress(void) {
    return (cmos_read(RTC_REG_STATUS_A) & RTC_UIP) != 0;
}

// Wait until RTC is NOT performing an update cycle
static void rtc_wait_ready(void) {
    // Wait for UIP to be set (update cycle starting)
    while (!rtc_update_in_progress())
        cpu_relax();
    // Wait for UIP to clear (update finished, registers stable for ~999 ms)
    while (rtc_update_in_progress())
        cpu_relax();
}

// ─── BCD to binary conversion ────────────────────────────────────────────────
static inline u8 bcd_to_bin(u8 bcd) {
    return (u8)(((bcd >> 4) * 10) + (bcd & 0x0F));
}

// ─── Read all RTC registers at once ──────────────────────────────────────────
static void rtc_read_regs(RTCTime *t) {
    t->seconds = cmos_read(RTC_REG_SECONDS);
    t->minutes = cmos_read(RTC_REG_MINUTES);
    t->hours   = cmos_read(RTC_REG_HOURS);
    t->day     = cmos_read(RTC_REG_DAY);
    t->month   = cmos_read(RTC_REG_MONTH);
    t->year    = cmos_read(RTC_REG_YEAR);
}

// ─── Public API ───────────────────────────────────────────────────────────────
void rtc_init(void) {
    // Verify CMOS is accessible: read status register A, should be non-0xFF
    u8 sta = cmos_read(RTC_REG_STATUS_A);
    if (sta == 0xFF) {
        kprintf("[BLAD] RTC: CMOS niedostępny!\n");
        return;
    }
    kprintf("[DOBRZE] RTC: CMOS dostępny, status_A=0x%x\n", sta);
}

RTCTime rtc_read(void) {
    RTCTime t1, t2;

    // Wait for a stable read: read twice and compare
    do {
        rtc_wait_ready();
        rtc_read_regs(&t1);
        rtc_wait_ready();
        rtc_read_regs(&t2);
    } while (t1.seconds != t2.seconds ||
             t1.minutes != t2.minutes ||
             t1.hours   != t2.hours   ||
             t1.day     != t2.day     ||
             t1.month   != t2.month   ||
             t1.year    != t2.year);

    // Read Status B to determine format
    u8 sb = cmos_read(RTC_REG_STATUS_B);
    bool is_binary = (sb & RTC_SB_BINARY) != 0;
    bool is_24h    = (sb & RTC_SB_24H) != 0;

    // Convert BCD to binary if needed
    if (!is_binary) {
        t1.seconds = bcd_to_bin(t1.seconds);
        t1.minutes = bcd_to_bin(t1.minutes);
        t1.day     = bcd_to_bin(t1.day);
        t1.month   = bcd_to_bin(t1.month);
        t1.year    = bcd_to_bin(t1.year);

        // Hours need special handling for 12h PM case
        bool pm = (!is_24h) && (t1.hours & 0x80);
        t1.hours = bcd_to_bin((u8)(t1.hours & 0x7F));
        if (pm && t1.hours != 12)
            t1.hours = (u8)(t1.hours + 12);
        else if (!pm && t1.hours == 12)
            t1.hours = 0;
    } else if (!is_24h) {
        bool pm = (t1.hours & 0x80) != 0;
        t1.hours &= 0x7F;
        if (pm && t1.hours != 12)
            t1.hours = (u8)(t1.hours + 12);
        else if (!pm && t1.hours == 12)
            t1.hours = 0;
    }

    // Build full year (CMOS gives 2-digit year)
    t1.year = (u16)(t1.year + (t1.year < 100 ? 2000 : 0));

    return t1;
}
