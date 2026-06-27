"""picotool tests — tests the C bridge for BOOTSEL communication.
   These tests require the Feather to be in BOOTSEL mode."""

import subprocess, os

SAGE = "/usr/local/bin/sage"
PICOTOOL = os.path.join(os.path.dirname(__file__), "..", "src", "tools", "sage", "sagepicotool.sage")

def run_sagepicotool_tests(t):
    """Run picotool tests. Most are skipped without BOOTSEL device."""

    # Check if device is in BOOTSEL
    result = subprocess.run(["lsusb"], capture_output=True, text=True)
    in_bootsel = "2e8a:000f" in result.stdout

    if not in_bootsel:
        t.skip("picotool: info", "Feather not in BOOTSEL mode")
        t.skip("picotool: reboot", "Feather not in BOOTSEL mode")
        t.skip("picotool: help output", "Feather not in BOOTSEL mode")
        return

    # Help/usage
    result = subprocess.run([SAGE, PICOTOOL], capture_output=True, text=True, timeout=10)
    t.assert_contains("picotool: help has info", result.stdout, "info")
    t.assert_contains("picotool: help has load", result.stdout, "load")
    t.assert_contains("picotool: help has reboot", result.stdout, "reboot")

    # Info
    result = subprocess.run([SAGE, PICOTOOL, "info"], capture_output=True, text=True, timeout=10)
    t.assert_contains("picotool: info shows RP2350", result.stdout, "RP2350")

    print(f"\n  Picotool: {t.passed} passed, {t.failed} failed, {t.skipped} skipped")
