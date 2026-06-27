# Pure Sage PIO module
let pico = ffi_open("pico")

proc claim(pio_idx, sm):
    return ffi_call(pico, "pio_claim", [pio_idx, sm])

proc put(pio_idx, sm, data):
    ffi_call(pico, "pio_put", [pio_idx, sm, data])

proc get(pio_idx, sm):
    return ffi_call(pico, "pio_get", [pio_idx, sm])

proc set_pins(pio_idx, sm, pin_base, pin_count, out):
    ffi_call(pico, "pio_set_pins", [pio_idx, sm, pin_base, pin_count, out])

proc enable(pio_idx, sm, enabled):
    ffi_call(pico, "pio_set_enabled", [pio_idx, sm, enabled])

proc set_clkdiv(pio_idx, sm, div):
    ffi_call(pico, "pio_set_clkdiv", [pio_idx, sm, div])

proc clear_fifos(pio_idx, sm):
    ffi_call(pico, "pio_clear_fifos", [pio_idx, sm])
