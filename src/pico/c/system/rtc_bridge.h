/* Software clock bridge for SagePico — based on system timer (always works).
   Survives watchdog reboots using watchdog scratch registers.
   Usage: clock_get() → "YYYY-MM-DD HH:MM:SS", clock_set(dt) */

#ifndef SAGE_RTC_BRIDGE_H
#define SAGE_RTC_BRIDGE_H

#include "hardware/watchdog.h"
#include "pico/util/datetime.h"
#include <stdio.h>

static uint32_t _clock_base = 0;    /* Unix-ish timestamp at boot */
static uint64_t _clock_boot_us = 0; /* system time at boot */

/* Initialize software clock */
static void sage_clock_init(void) {
    if (_clock_base == 0) {
        /* Try to restore from watchdog scratch registers (survives reboot) */
        uint32_t hi = watchdog_hw->scratch[6];
        uint32_t lo = watchdog_hw->scratch[7];
        if (hi != 0 || lo != 0) {
            _clock_base = (hi << 16) | (lo & 0xFFFF);
        } else {
            /* Default: 2026-06-26 00:00:00 = ~1770000000 unix-ish */
            _clock_base = 1770000000;
        }
    }
    _clock_boot_us = time_us_64();
}

/* Save clock to watchdog scratch (survives reboot) */
static void sage_clock_save(void) {
    uint32_t now = _clock_base + (uint32_t)((time_us_64() - _clock_boot_us) / 1000000);
    watchdog_hw->scratch[6] = now >> 16;
    watchdog_hw->scratch[7] = now & 0xFFFF;
}

/* Get current time as string "YYYY-MM-DD HH:MM:SS" */
static const char* sage_clock_get(void) {
    static char buf[20];
    if (_clock_base == 0) sage_clock_init();
    uint32_t secs = _clock_base + (uint32_t)((time_us_64() - _clock_boot_us) / 1000000);
    /* Simple date conversion (approximate, good enough for display) */
    uint32_t days = secs / 86400;
    uint32_t time = secs % 86400;
    int year = 2026, month = 1;
    while (days >= 365) { days -= 365; year++; }
    while (days >= 30)  { days -= 30;  month++; }
    int day = (int)days + 1;
    int hour = (int)(time / 3600);
    int min  = (int)((time % 3600) / 60);
    int sec  = (int)(time % 60);
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, min, sec);
    return buf;
}

/* Set clock from string "YYYY-MM-DD HH:MM:SS" */
static int sage_clock_set(const char* dt) {
    int year, month, day, hour, min, sec;
    if (sscanf(dt, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) != 6)
        return 0;
    uint32_t days = (year - 2026) * 365 + (month - 1) * 30 + (day - 1);
    _clock_base = days * 86400 + hour * 3600 + min * 60 + sec;
    _clock_boot_us = time_us_64();
    sage_clock_save();
    return 1;
}

#endif
