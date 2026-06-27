"""GPIO tests — LED on/off, pin init, set_dir, read back."""

def run_gpio_tests(t):
    # LED on GPIO 7
    t.assert_contains("gpio_put on", t.cmd("gpio_put(7, 1)"), ">>>")  # no error
    t.assert_contains("gpio_put off", t.cmd("gpio_put(7, 0)"), ">>>")

    # gpio_get
    t.cmd("gpio_put(7, 1)")
    r = t.cmd("gpio_get(7)")
    t.assert_contains("gpio_get high", r, "1")

    t.cmd("gpio_put(7, 0)")
    r = t.cmd("gpio_get(7)")
    t.assert_contains("gpio_get low", r, "0")

    # gpio_init + gpio_set_dir (test on pin 8 if available, or skip)
    t.cmd("gpio_init(8)")
    t.cmd("gpio_set_dir(8, 1)")  # OUT
    t.assert_contains("gpio_set_dir", t.cmd("gpio_put(8, 1)"), ">>>")

    # gpio_set_function
    t.cmd('gpio_set_function(8, 5)')  # SIO
    t.assert_contains("gpio_set_func SIO", t.cmd("gpio_set_dir(8, 1)"), ">>>")

    # Shell led commands
    t.assert_contains("shell led on", t.cmd("led on"), "LED on")
    t.assert_contains("shell led off", t.cmd("led off"), "LED off")
