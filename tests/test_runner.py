#!/usr/bin/env python3
"""SagePico Test Framework — connects to Feather over serial and runs test suites.
   Usage: python3 tests/test_runner.py [--port /dev/ttyACM0] [test_name...]
   Requires: pyserial (pip install pyserial)"""

import serial
import sys
import time
import os

class SagePicoTest:
    def __init__(self, port="/dev/ttyACM0", baud=115200, timeout=2):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser = None
        self.passed = 0
        self.failed = 0
        self.skipped = 0
        self.errors = []

    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.2)
            time.sleep(2)  # wait for REPL to be ready after boot
            # Drain any pending output (non-blocking)
            try:
                while self.ser.read(1024):
                    pass
            except:
                pass
            return True
        except Exception as e:
            print(f"  FAIL: Cannot connect to {self.port}: {e}")
            return False

    def disconnect(self):
        if self.ser:
            self.ser.close()

    def cmd(self, text, wait=0.4):
        """Send a command and return the response."""
        self.ser.write((text + "\n").encode())
        time.sleep(wait)
        return self.ser.read(4096).decode(errors="replace")

    def assert_contains(self, test_name, response, expected, should_contain=True):
        """Assert that response contains (or doesn't contain) expected text."""
        found = expected in response
        ok = found if should_contain else not found
        if ok:
            self.passed += 1
            print(f"  PASS: {test_name}")
        else:
            self.failed += 1
            msg = f"  FAIL: {test_name} — expected '{expected}' {'in' if should_contain else 'NOT in'} response"
            print(msg)
            print(f"    Got: {repr(response[:200])}")
            self.errors.append(msg)

    def assert_eq(self, test_name, actual, expected):
        if actual == expected:
            self.passed += 1
            print(f"  PASS: {test_name}")
        else:
            self.failed += 1
            msg = f"  FAIL: {test_name} — expected '{expected}', got '{actual}'"
            print(msg)
            self.errors.append(msg)

    def skip(self, test_name, reason):
        self.skipped += 1
        print(f"  SKIP: {test_name} — {reason}")

    def summary(self):
        total = self.passed + self.failed + self.skipped
        print(f"\n{'='*50}")
        print(f"  Results: {self.passed} passed, {self.failed} failed, {self.skipped} skipped ({total} total)")
        if self.failed > 0:
            print(f"  FAILURES:")
            for e in self.errors:
                print(f"    {e}")
        print(f"{'='*50}")
        return self.failed == 0


def run_suite(t, suite_name, suite_fn):
    print(f"\n--- {suite_name} ---")
    suite_fn(t)


if __name__ == "__main__":
    port = "/dev/ttyACM0"
    args = sys.argv[1:]
    if "--port" in args:
        idx = args.index("--port")
        port = args[idx + 1]
        args = args[idx+2:] if len(args) > idx+2 else []

    t = SagePicoTest(port=port)
    if not t.connect():
        sys.exit(1)

    # Import test modules
    from test_repl import run_repl_tests
    from test_shell import run_shell_tests
    from test_gpio import run_gpio_tests
    from test_clock import run_clock_tests
    from test_flash import run_flash_tests
    from test_pioasm import run_pioasm_tests
    from test_sagepicotool import run_sagepicotool_tests
    from test_elf2uf2 import run_elf2uf2_tests

    filter_tests = [a for a in args if not a.startswith("--")]

    if not filter_tests or "repl" in filter_tests:
        run_suite(t, "REPL Expressions", run_repl_tests)
    if not filter_tests or "shell" in filter_tests:
        run_suite(t, "Shell Commands", run_shell_tests)
    if not filter_tests or "gpio" in filter_tests:
        run_suite(t, "GPIO", run_gpio_tests)
    if not filter_tests or "clock" in filter_tests:
        run_suite(t, "Clock", run_clock_tests)
    if not filter_tests or "flash" in filter_tests:
        run_suite(t, "Flash Storage", run_flash_tests)
    if not filter_tests or "pioasm" in filter_tests:
        run_suite(t, "PIO Assembler", run_pioasm_tests)
    if not filter_tests or "picotool" in filter_tests:
        run_suite(t, "Picotool", run_sagepicotool_tests)
    if not filter_tests or "elf2uf2" in filter_tests:
        run_suite(t, "ELF2UF2", run_elf2uf2_tests)

    t.disconnect()
    ok = t.summary()
    sys.exit(0 if ok else 1)
