# Pure Sage Time module
let pico = ffi_open("pico")

proc sleep_ms(ms):
    ffi_call(pico, "sleep_ms", [ms])

proc sleep_us(us):
    ffi_call(pico, "sleep_us", [us])

proc micros():
    return ffi_call(pico, "time_us", [])

proc millis():
    return ffi_call(pico, "time_ms", [])
