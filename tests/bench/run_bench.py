#!/usr/bin/env python3
"""SagePico Benchmarking Suite — measures performance of CPU vs PIO operations.
   Connects to Feather over USB serial, runs commands, reports timings.
   Usage: python3 tests/bench/run_bench.py [--port /dev/ttyACM0]"""

import serial, time, sys, os

class Benchmark:
    def __init__(self, port="/dev/ttyACM0", baud=115200):
        self.ser = serial.Serial(port, baud, timeout=1)
        time.sleep(2)
        self.ser.read(4096)  # drain

    def cmd(self, text, wait=0.3):
        self.ser.write((text + "\n").encode())
        time.sleep(wait)
        return self.ser.read(4096).decode(errors="replace")

    def bench(self, name, cmd, iterations=100):
        """Time a REPL command over N iterations."""
        # Warm-up
        self.cmd(cmd)
        start = time.time()
        for _ in range(iterations):
            self.cmd(cmd)
        elapsed = time.time() - start
        avg_ms = (elapsed / iterations) * 1000
        return avg_ms

    def run_suite(self):
        print("=" * 60)
        print("  SagePico Benchmark Suite")
        print("=" * 60)

        suites = [
            ("Arithmetic", self.bench_arithmetic),
            ("String Ops", self.bench_strings),
            ("SHA-256", self.bench_sha256),
            ("GPIO", self.bench_gpio),
            ("Flash Read", self.bench_flash_read),
            ("Flash Write", self.bench_flash_write),
            ("Clock", self.bench_clock),
        ]

        results = []
        for name, fn in suites:
            print(f"\n--- {name} ---")
            try:
                r = fn()
                for label, ms in r:
                    print(f"  {label}: {ms:.2f}ms")
                    results.append((name, label, ms))
            except Exception as e:
                print(f"  ERROR: {e}")

        print(f"\n{'='*60}")
        print("  Summary (fastest to slowest)")
        print("=" * 60)
        results.sort(key=lambda x: x[2])
        for cat, label, ms in results:
            print(f"  {cat:15s} {label:30s} {ms:8.2f}ms")

    def bench_arithmetic(self):
        return [
            ("1+1", self.bench("1+1", "1+1")),
            ("(123+456)*789", self.bench("complex", "(123+456)*789")),
            ("let+read", self.bench("let+read", "let x=42")),
        ]

    def bench_strings(self):
        self.cmd('let s="hello world"')
        return [
            ('concat short', self.bench("concat", '"a"+"b"')),
            ('sha256("hello")', self.bench("sha256-short", 'sha256("hello")')),
            ('sha256 64B', self.bench("sha256-64", 'sha256("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")')),
        ]

    def bench_sha256(self):
        return [
            ('5 bytes', self.bench("sha256-5", 'sha256("hello")')),
            ('64 bytes', self.bench("sha256-64", 'sha256("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")')),
            ('256 bytes', self.bench("sha256-256", 'sha256("' + "a"*256 + '")')),
        ]

    def bench_gpio(self):
        return [
            ('gpio_put(7,1)', self.bench("gpio-put", 'gpio_put(7,1)')),
            ('gpio_get(7)', self.bench("gpio-get", 'gpio_get(7)')),
            ('led on', self.bench("led-on", "led on")),
        ]

    def bench_flash_read(self):
        self.cmd('flash_save("bkey", "benchmark_value")')
        return [
            ('flash_load', self.bench("flash-read", 'flash_load("bkey")')),
            ('flash_keys', self.bench("flash-keys", 'flash_keys()')),
        ]

    def bench_flash_write(self):
        return [
            ('flash_save 10B', self.bench("flash-write", 'flash_save("bk", "0123456789")')),
        ]

    def bench_clock(self):
        return [
            ('clock_get()', self.bench("clock-get", 'clock_get()')),
            ('clock_set()', self.bench("clock-set", 'clock_set("2026-01-01 00:00:00")')),
        ]

    def close(self):
        self.ser.close()


if __name__ == "__main__":
    port = "/dev/ttyACM0"
    for i, a in enumerate(sys.argv):
        if a == "--port" and i+1 < len(sys.argv):
            port = sys.argv[i+1]
    b = Benchmark(port)
    try:
        b.run_suite()
    finally:
        b.close()
