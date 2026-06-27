# Pure Sage SHA-256 module — hardware-accelerated on RP2350
let pico = ffi_open("pico")

proc hash(data):
    return ffi_call(pico, "sha256", [data])
