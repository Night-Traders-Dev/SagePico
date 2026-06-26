/* Minimal Sage-to-Pico bridge - lightweight GPIO/print wrappers */
#ifndef SAGE_PICO_PORT_H
#define SAGE_PICO_PORT_H

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdio.h>

/* ---- GPIO ---- */
#define SAGE_GPIO_OUT 1
#define SAGE_GPIO_IN  0

static inline void sage_gpio_init(int pin)          { gpio_init(pin); }
static inline void sage_gpio_set_dir(int pin, int d) { gpio_set_dir(pin, d); }
static inline void sage_gpio_put(int pin, int v)     { gpio_put(pin, v); }
static inline int  sage_gpio_get(int pin)            { return gpio_get(pin); }

/* ---- Print (direct printf, no Sage runtime) ---- */
#define sage_print(s) printf("%s\n", s)

#endif
