/* ARM Cortex-M33 specific initialization for SagePico.
   Included into generated hello.c by patch_main.py. */

#ifndef SAGE_ARM_INIT_H
#define SAGE_ARM_INIT_H

#include "hardware/resets.h"
#include "hardware/irq.h"

/* ARM-specific pre-stdio init: reset USB before init.
   The Sage runtime's large BSS/heap may overlap USB DMA buffers;
   an explicit USB reset ensures clean state. */
static inline void sage_arch_pre_init(void) {
    reset_block(RESETS_RESET_USBCTRL_BITS);
    unreset_block_wait(RESETS_RESET_USBCTRL_BITS);
}

/* ARM-specific boot init (called after stdio_init_all) */
static inline void sage_arch_init(void) {
    gpio_init(7);
    gpio_set_dir(7, GPIO_OUT);
    gpio_put(7, 0);
}

/* ARM-specific post-stdio init */
static inline void sage_arch_post_init(void) {
    /* Ensure USB IRQ is enabled (NVIC handles priority) */
    irq_set_enabled(USBCTRL_IRQ, true);
}

#endif
