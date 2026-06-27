# Pure Sage ADC module
let pico = ffi_open("pico")

proc init():
    ffi_call(pico, "adc_init", [])

proc gpio_init(pin):
    ffi_call(pico, "adc_gpio", [pin])

proc select(channel):
    ffi_call(pico, "adc_select", [channel])

proc read():
    return ffi_call(pico, "adc_read", [])
