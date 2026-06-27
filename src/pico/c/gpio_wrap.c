/* Pico SDK GPIO wrapper - bridges Sage types to Pico SDK C functions */

#include "pico/stdlib.h"
#include "hardware/gpio.h"

/* Called from Sage-generated C code.
   Sage values are passed as SageValue structs.
   We use the type definitions from the generated hello.c preamble. */

typedef struct SageValue SageValue;

/* Sage tag types (must match generated code) */
#define SAGE_TAG_NIL     0
#define SAGE_TAG_NUMBER  1
#define SAGE_TAG_BOOL    2

/* Helper to extract int from SageValue */
static inline int sage_to_int(SageValue v) {
    if (v.type == SAGE_TAG_NUMBER) return (int)v.as.number;
    if (v.type == SAGE_TAG_BOOL) return v.as.boolean;
    return 0;
}

/* GPIO wrappers */
SageValue sage_pico_gpio_init(SageValue pin) {
    gpio_init(sage_to_int(pin));
    SageValue nil; nil.type = SAGE_TAG_NIL; return nil;
}

SageValue sage_pico_gpio_set_dir(SageValue pin, SageValue dir) {
    gpio_set_dir(sage_to_int(pin), sage_to_int(dir));
    SageValue nil; nil.type = SAGE_TAG_NIL; return nil;
}

SageValue sage_pico_gpio_put(SageValue pin, SageValue value) {
    gpio_put(sage_to_int(pin), sage_to_int(value));
    SageValue nil; nil.type = SAGE_TAG_NIL; return nil;
}

SageValue sage_pico_gpio_get(SageValue pin) {
    int val = gpio_get(sage_to_int(pin));
    SageValue v; v.type = SAGE_TAG_NUMBER; v.as.number = val; return v;
}
