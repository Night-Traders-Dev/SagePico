# SagePico firmware entry — imports pico native bridge
# The C patch layer handles HSTX display init + scanline render
# Sage code can use FFI to call pico_port functions

let pico = ffi_open("pico")

print "SagePico: Hello from Sage on RP2350!"
print "Native pico bridge loaded"
