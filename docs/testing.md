# SagePico Test Suite

Automated hardware-in-the-loop testing for SagePico firmware. Tests run against a connected Feather RP2350 over USB serial.

## Quick Start

```bash
# Install dependency
pip install pyserial

# Run all tests
cd tests && ./run_all.sh

# Run specific test suite
python3 test_runner.py --port /dev/ttyACM0 repl shell

# Run with custom port
python3 test_runner.py --port /dev/ttyACM1
```

## Test Architecture

```
tests/
  test_runner.py    # Main framework: connect, cmd, assert, summary
  test_repl.py      # REPL expression evaluation
  test_shell.py     # Shell commands (help, version, free, uptime, led)
  test_gpio.py      # GPIO operations (init, set_dir, put, get, set_function)
  test_clock.py     # Software clock (get, set)
  test_flash.py     # Flash storage (save, load, delete, keys)
  run_all.sh         # Convenience script to run all suites
```

### Test Framework API

Each test module defines a `run_*_tests(t)` function receiving a `SagePicoTest` instance:

```python
t.assert_contains("test name", t.cmd("REPL command"), "expected text")
t.assert_eq("test name", actual_string, "expected_string")
t.skip("test name", "reason")
```

- `t.cmd(text)` — sends `text\n` to the Feather and returns the response
- `t.assert_contains(name, response, expected)` — passes if `expected` is in `response`
- `t.assert_eq(name, actual, expected)` — passes if strings match exactly
- `t.skip(name, reason)` — skips the test

## Test Suites

### REPL Expressions (`test_repl.py`)

Tests arithmetic, comparisons, strings, booleans, variables, arrays, and parenthesized expressions.

| Test | Command | Expected |
|------|---------|----------|
| Addition | `1+1` | `2` |
| Subtraction | `10-3` | `7` |
| Multiplication | `4*5` | `20` |
| Division | `10/3` | `3.333` |
| Modulo | `10%3` | `1` |
| Negation | `-5+3` | `-2` |
| Equality | `5==5` | `true` |
| Inequality | `5!=3` | `true` |
| Less than | `5<10` | `true` |
| Greater than | `10>5` | `true` |
| String concat | `"hello" + " world"` | `hello world` |
| Boolean | `true`, `false` | `true`, `false` |
| Nil | `nil` | `nil` |
| Variable | `let x = 42` then `x` | `42` |
| Variable expr | `x * 2` | `84` |
| Array | `[1, 2, 3]` | `[1, 2, 3]` |
| Parenthesized | `(2+3)*4` | `20` |

### Shell Commands (`test_shell.py`)

Tests all built-in shell commands.

| Test | Command | Expected |
|------|---------|----------|
| Help | `help` | Contains "Shell commands" |
| Version | `version` | Contains "SagePico", "RP2350" |
| Free | `free` | Contains "Flash", "SRAM" |
| Uptime | `uptime` | Contains "Uptime" |
| LED on | `led on` | Contains "LED on" |
| LED off | `led off` | Contains "LED off" |
| Sage banner | `sage` | Contains "Sage REPL" |

### GPIO (`test_gpio.py`)

Tests GPIO operations on pins 7 (LED) and 8.

| Test | Command | Expected |
|------|---------|----------|
| Set high | `gpio_put(7, 1)` | No error |
| Set low | `gpio_put(7, 0)` | No error |
| Read high | `gpio_get(7)` after set high | `1` |
| Read low | `gpio_get(7)` after set low | `0` |
| Init + dir | `gpio_init(8)` + `gpio_set_dir(8, 1)` | No error |
| Set function | `gpio_set_function(8, 5)` | No error |
| Shell LED | `led on`, `led off` | "LED on", "LED off" |

### Clock (`test_clock.py`)

Tests the software clock.

| Test | Command | Expected |
|------|---------|----------|
| Get time | `clock_get()` | Contains `2026-` |
| Set time | `clock_set("2026-06-26 20:00:00")` | `true` |
| Verify set | `clock_get()` after set | Contains `2026-06-26 20:00:` |

### Flash Storage (`test_flash.py`)

Tests flash-backed key-value storage and variable persistence.

| Test | Command | Expected |
|------|---------|----------|
| Save | `flash_save("test_key", "hello flash")` | No error |
| Load | `flash_load("test_key")` | `hello flash` |
| Delete | `flash_del("test_key")` | No error |
| Verify delete | `flash_load("test_key")` | `nil` |
| List keys | `flash_keys()` | Contains saved keys |
| Variable | `let persist_var = 999` then `persist_var` | `999` |

## Adding New Tests

1. Create a new `test_*.py` file in `tests/`
2. Define a `run_*_tests(t)` function
3. Import and register it in `test_runner.py`

```python
# tests/test_i2c.py
def run_i2c_tests(t):
    t.cmd("i2c_init(400000)")
    t.assert_contains("i2c init", t.cmd("1+1"), "2")  # sanity check
```

Then add to `test_runner.py`:
```python
from test_i2c import run_i2c_tests
# ...
if not filter_tests or "i2c" in filter_tests:
    run_suite(t, "I2C", run_i2c_tests)
```

## Test Coverage Map

| Component | Tests | Coverage |
|-----------|-------|----------|
| REPL expressions | 22 | Arithmetic, comparisons, strings, variables, arrays |
| Shell commands | 7 | help, version, free, uptime, led, sage |
| GPIO | 8 | init, set_dir, put, get, set_function, shell LED |
| Clock | 3 | get, set, verify |
| Flash | 7 | save, load, delete, keys, variable persistence |
| **Total** | **47** | Core REPL + Shell + GPIO + Clock + Flash |

### Not Yet Covered

These components need dedicated test suites:

| Component | Reason |
|-----------|--------|
| I2C | Needs external I2C device connected |
| SPI | Needs external SPI device connected |
| ADC | Needs analog voltage source |
| PWM | Needs oscilloscope or LED with PWM |
| UART | Loopback test needs external wiring |
| PIO | Needs logic analyzer or specific PIO program |
| DMA | Needs PIO or memory-to-memory transfer setup |
| GFX VM | Needs DVI display connected |
| Clock persistence | Needs reboot between tests |
| Multi-line REPL | Needs complex input simulation |
| Program save/load/run | Needs multi-line input |

## Automated Regression Testing

```bash
# In CI, after building and flashing:
cd tests && python3 test_runner.py --port /dev/ttyACM0

# Exit code 0 = all passed, 1 = failures
echo $?
```
