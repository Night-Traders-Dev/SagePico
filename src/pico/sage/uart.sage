# Pure Sage UART module
let pico = ffi_open("pico")

proc init(baud):
    ffi_call(pico, "uart_init", [baud])

proc write_byte(c):
    ffi_call(pico, "uart_putc", [c])

proc write(s):
    ffi_call(pico, "uart_puts", [s])

proc read_byte():
    return ffi_call(pico, "uart_getc", [])

proc available():
    return ffi_call(pico, "uart_readable", [])
