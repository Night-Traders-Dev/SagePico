/* ARM Cortex-M33 specific initialization for SagePico.
   Included into generated hello.c by patch_main.py. */

#ifndef SAGE_ARM_INIT_H
#define SAGE_ARM_INIT_H

/* ARM-specific boot init (called after stdio_init_all):
   - GPIO LED heartbeat on pin 7
   - No special interrupt setup needed (NVIC handles it) */
static inline void sage_arch_init(void) {
    gpio_init(7);
    gpio_set_dir(7, GPIO_OUT);
    gpio_put(7, 0);
}

/* ARM-specific pre-stdio init (called before stdio_init_all) */
static inline void sage_arch_pre_init(void) {
    /* Nothing needed — NVIC is initialized by SDK runtime */
}

/* ARM-specific post-stdio init (called after stdio_init_all) */
static inline void sage_arch_post_init(void) {
    /* Nothing needed — USB interrupts are handled by NVIC */
}

#endif
