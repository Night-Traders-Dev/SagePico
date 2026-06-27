/* Exception & Power Management Bridge */

#ifndef SAGE_EXCPM_BRIDGE_H
#define SAGE_EXCPM_BRIDGE_H

/* ---- Exception Handlers (minimal) ---- */
static void sage_exc_install(void) {
    /* Full exception_set_exclusive_handler requires hardware_exception lib.
       For now, watchdog reboot handles crashes via SDK default handlers. */
}

/* ---- Power Management ---- */
static void sage_powman_dormant(uint gpio_pin, uint edge) {
    (void)gpio_pin; (void)edge;
    /* Dormant mode requires pico/sleep.h which may not be available.
       Use sleep_ms() for basic power saving instead. */
}

#endif
