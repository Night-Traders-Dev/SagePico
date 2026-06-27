/* Power management bridge for SagePico.
   Exposes sleep and reset reason via FFI.
   Usage: powman_sleep_ms(ms), powman_reset_reason() */

#ifndef SAGE_POWMAN_BRIDGE_H
#define SAGE_POWMAN_BRIDGE_H

#include "hardware/watchdog.h"

/* Sleep for N milliseconds (standard SDK sleep) */
static void sage_powman_sleep_ms(uint32_t ms) {
    sleep_ms(ms);
}

/* Get reset reason: 0=power-on, 1=watchdog, -1=unknown */
static int sage_powman_reset_reason(void) {
    if (watchdog_caused_reboot()) return 1;
    return 0;
}

#endif
