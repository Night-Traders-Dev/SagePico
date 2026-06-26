# Blink demo for SagePico — pure Sage using FFI to pico_port
# Compile: sage --emit-pico-c src/blink.sage -o .tmp/blink-riscv/blink.c
# Then patch + build as usual

let pico = ffi_open("pico")

# Blink the LED on GPIO 7 (Feather RP2350 built-in LED)
ffi_call(pico, "gpio_init", [7])
ffi_call(pico, "gpio_set_dir", [7, 1])  # 1 = OUTPUT

let count = 0
while count < 10:
    ffi_call(pico, "gpio_put", [7, 1])   # LED on
    ffi_call(pico, "sleep_ms", [250])
    ffi_call(pico, "gpio_put", [7, 0])   # LED off
    ffi_call(pico, "sleep_ms", [250])
    count = count + 1

print "Blink complete"
