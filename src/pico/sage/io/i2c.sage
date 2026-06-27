# Pure Sage I2C module
let pico = ffi_open("pico")

proc init(baud):
    ffi_call(pico, "i2c_init", [baud])

proc write(addr, data):
    ffi_call(pico, "i2c_write", [addr, data])

proc read(addr, len):
    return ffi_call(pico, "i2c_read", [addr, len])
