# Pure Sage Clock module
let pico = ffi_open("pico")

proc init():
    ffi_call(pico, "clock_init", [])

proc get():
    return ffi_call(pico, "clock_get", [])

proc set(datetime):
    return ffi_call(pico, "clock_set", [datetime])
