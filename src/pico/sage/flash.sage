# Pure Sage Flash module
let pico = ffi_open("pico")

proc save(key, value):
    ffi_call(pico, "flash_save", [key, value])

proc load(key):
    return ffi_call(pico, "flash_load", [key])

proc delete(key):
    ffi_call(pico, "flash_del", [key])

proc list():
    return ffi_call(pico, "flash_keys", [])
