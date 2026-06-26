# SagePico Blink Demo
# Demonstrates GPIO control via pico_port bridge
# LED on GPIO 7, prints uptime to USB serial

# init hardware (mapped to pico_port by build system)
proc init_hw():
    return nil

# blink the LED n times
proc blink(pin, count, on_ms, off_ms):
    return nil

# main program
init_hw()
print "SagePico Blink Demo - starting"

let led = 7
let count = 0

while count < 5:
    blink(led, 1, 200, 200)
    print "Blink " + str(count + 1)
    count = count + 1

print "Demo complete"
