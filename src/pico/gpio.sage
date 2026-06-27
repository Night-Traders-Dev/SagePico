# Pure Sage GPIO module — wraps FFI calls with idiomatic Sage API
# Usage: import gpio; gpio.set_dir(7, gpio.OUT); gpio.write(7, 1)

let pico = ffi_open("pico")

let OUT = 1
let IN = 0
let HIGH = 1
let LOW = 0

proc init(pin):
    ffi_call(pico, "gpio_init", [pin])

proc set_dir(pin, dir):
    ffi_call(pico, "gpio_set_dir", [pin, dir])

proc write(pin, val):
    ffi_call(pico, "gpio_put", [pin, val])

proc read(pin):
    return ffi_call(pico, "gpio_get", [pin])

proc pull_up(pin):
    ffi_call(pico, "gpio_pull_up", [pin])

proc pull_down(pin):
    ffi_call(pico, "gpio_pull_down", [pin])

proc set_function(pin, func):
    ffi_call(pico, "gpio_set_function", [pin, func])
