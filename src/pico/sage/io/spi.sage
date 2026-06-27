# Pure Sage SPI module
let pico = ffi_open("pico")

proc init(baud):
    ffi_call(pico, "spi_init", [baud])

proc transfer(tx_data):
    return ffi_call(pico, "spi_xfer", [tx_data])
