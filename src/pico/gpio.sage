# Pico SDK GPIO wrapper module for Sage
# Provides gpio_init, gpio_set_dir, gpio_put, gpio_get
# These map to the Pico SDK hardware/gpio functions

let GPIO_OUT = 1
let GPIO_IN = 0

proc gpio_init(pin):
    # Stub - actual implementation is in C wrapper gpio_wrap.c
    return nil

proc gpio_set_dir(pin, dir):
    return nil

proc gpio_put(pin, value):
    return nil

proc gpio_get(pin):
    return 0

proc gpio_set_function(pin, fn):
    return nil
