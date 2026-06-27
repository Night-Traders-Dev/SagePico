## GPIO — digital I/O with pin mode constants and batch read/write
## Usage:
##   let gpio = GPIO()
##   gpio.init(7)
##   gpio.mode(7, gpio.OUT)
##   gpio.write(7, 1)          # LED on
##   let val = gpio.read(7)    # Returns 0 or 1
##   gpio.high(7)              # Shorthand for write(7, 1)
##   gpio.low(7)               # Shorthand for write(7, 0)
##   gpio.toggle(7)            # Flip current state

class GPIO:
    ## Direction constants
    let OUT = 1
    let IN  = 0
    ## Value constants
    let HIGH = 1
    let LOW  = 0
    ## Pin function constants
    let FUNC_UART = 2
    let FUNC_I2C  = 3
    let FUNC_SPI  = 4
    let FUNC_PWM  = 5
    let FUNC_SIO  = 5

    ## Initialize GPIO subsystem
    proc init(self):
        self.pico = ffi_open("pico")

    ## Initialize a GPIO pin
    proc init(self, pin):
        ffi_call(self.pico, "gpio_init", [pin])

    ## Set pin direction (GPIO.OUT or GPIO.IN)
    proc set_dir(self, pin, dir):
        ffi_call(self.pico, "gpio_set_dir", [pin, dir])

    ## Write HIGH or LOW to pin
    proc write(self, pin, val):
        ffi_call(self.pico, "gpio_put", [pin, val])

    ## Read pin state (returns 0 or 1)
    proc read(self, pin):
        return ffi_call(self.pico, "gpio_get", [pin])

    ## Set pin HIGH
    proc high(self, pin):
        self.write(pin, GPIO.HIGH)

    ## Set pin LOW
    proc low(self, pin):
        self.write(pin, GPIO.LOW)

    ## Toggle pin state
    proc toggle(self, pin):
        let current = self.read(pin)
        if current == 1:
            self.write(pin, 0)
        else:
            self.write(pin, 1)

    ## Enable pull-up resistor
    proc pull_up(self, pin):
        ffi_call(self.pico, "gpio_pull_up", [pin])

    ## Enable pull-down resistor
    proc pull_down(self, pin):
        ffi_call(self.pico, "gpio_pull_down", [pin])

    ## Set alternate function (UART, I2C, SPI, PWM)
    proc set_function(self, pin, func):
        ffi_call(self.pico, "gpio_set_function", [pin, func])
