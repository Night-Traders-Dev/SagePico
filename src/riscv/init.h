/* RISC-V Hazard3 specific initialization for SagePico.
   Included into generated hello.c by patch_main.py. */

#ifndef SAGE_RISCV_INIT_H
#define SAGE_RISCV_INIT_H

#include "hardware/resets.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"

/* RISC-V-specific pre-stdio init: reset USB controller before init.
   Hazard3's interrupt controller (MEIEA/MEIPRA CSRs) has different
   timing than ARM NVIC — USB hardware must be explicitly reset
   before stdio_init_all to ensure clean state. */
static inline void sage_arch_pre_init(void) {
    reset_block(RESETS_RESET_USBCTRL_BITS);
    unreset_block_wait(RESETS_RESET_USBCTRL_BITS);
}

/* RISC-V-specific boot init (called after stdio_init_all) */
static inline void sage_arch_init(void) {
    gpio_init(7);
    gpio_set_dir(7, GPIO_OUT);
    gpio_put(7, 0);
}

/* RISC-V-specific post-stdio init: set USB interrupt priority.
   Must be called AFTER stdio_init_all registers the shared USB IRQ
   handler, otherwise the priority set is overridden. */
static inline void sage_arch_post_init(void) {
    irq_set_priority(USBCTRL_IRQ, 0x00);
    irq_set_enabled(USBCTRL_IRQ, true);
    sleep_ms(50);
}

#endif
