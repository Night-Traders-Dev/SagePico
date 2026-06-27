"""Shell command tests — help, version, free, uptime, led, reboot identification."""

def run_shell_tests(t):
    # Help
    t.assert_contains("help command", t.cmd("help"), "Shell commands")
    t.assert_contains("help lists sage", t.cmd("help"), "sage")

    # Version
    r = t.cmd("version")
    t.assert_contains("version: SagePico", r, "SagePico")
    t.assert_contains("version: RP2350", r, "RP2350")

    # Free
    r = t.cmd("free")
    t.assert_contains("free: flash", r, "Flash")
    t.assert_contains("free: SRAM", r, "SRAM")

    # Uptime
    r = t.cmd("uptime")
    t.assert_contains("uptime: seconds", r, "Uptime")

    # LED
    t.assert_contains("led on", t.cmd("led on"), "LED on")
    t.assert_contains("led off", t.cmd("led off"), "LED off")

    # Sage banner
    t.assert_contains("sage banner", t.cmd("sage"), "Sage REPL")
