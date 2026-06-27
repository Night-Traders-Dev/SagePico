# Pure Sage PWM module
let pico = ffi_open("pico")

proc setup(pin, wrap, level):
    ffi_call(pico, "pwm_setup", [pin, wrap, level])

proc duty(pin, duty):
    ffi_call(pico, "pwm_duty", [pin, duty])
