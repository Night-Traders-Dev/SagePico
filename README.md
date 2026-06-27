# SagePico: SageLang for RP2350

Build bootable UF2 firmware from Sage source targeting the Feather RP2350 (Cortex-M33 ARM / Hazard3 RISC-V). Headless REPL with 67 FFI functions, flash persistence, PIO accelerators, and pure Sage tools.

**Docs Site:** [night-traders-dev.github.io/SagePico-Docs](https://night-traders-dev.github.io/SagePico-Docs)

## Quick Start

```bash
./build.sh arm                    # Build for ARM (94K firmware)
picotool load -f build/hello-arm.uf2  # Flash
sagecom --port /dev/ttyACM0       # Connect
```

On boot you enter the **Sage REPL** over USB serial:

```
=== Sage REPL ===
Shell: help, version, free, uptime, led, reboot
Multi-line: end line with : to continue
>>> 1+1
2
>>> gpio_put(7, 1)
>>> sha256("hello")
2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
>>> crc32("hello")
3610a686
>>> trng_read()
2847593612
>>> ~.
```

## Build Paths

### Path 1: Baremetal (Sage -> C -> UF2) ✓ Production

```
examples/hello.sage → sage --emit-pico-c → hello.c → patching → CMake + Pico SDK → build/hello-{arch}.uf2
```

### Path 2: SageVM SRVM (Sage -> Bytecode) ✓

```
examples/hello.sage → sagevm compile --riscv → examples/hello.sgrv
```

## Native Bridge & FFI (67 functions, 18 categories)

| Category | Functions |
|----------|-----------|
| **GPIO** | `gpio_init`, `gpio_set_dir`, `gpio_put`, `gpio_get`, `gpio_pull_up/down`, `gpio_set_function` |
| **Time** | `sleep_ms`, `sleep_us`, `time_us`, `time_ms` |
| **UART** | `uart_init`, `uart_putc`, `uart_puts`, `uart_getc`, `uart_readable` |
| **ADC** | `adc_init`, `adc_gpio`, `adc_select`, `adc_read` |
| **PWM** | `pwm_setup`, `pwm_duty` |
| **I2C** | `i2c_init`, `i2c_write`, `i2c_read` |
| **SPI** | `spi_init`, `spi_xfer` |
| **PIO** | `pio_claim`, `pio_put`, `pio_get`, `pio_set_pins`, `pio_set_enabled`, `pio_set_clkdiv`, `pio_clear_fifos` |
| **WS2812** | `ws2812_init`, `ws2812_put` |
| **DMA** | `dma_claim`, `dma_config`, `dma_start`, `dma_wait`, `dma_busy`, `dma_unclaim` |
| **Flash** | `flash_save`, `flash_load`, `flash_del`, `flash_keys` |
| **GFX VM** | `gfx_init`, `gfx_load`, `gfx_run`, `gfx_vblank` |
| **Clock** | `clock_init`, `clock_get`, `clock_set` |
| **Interp** | `interp_config`, `interp_pop`, `interp_peek` |
| **SHA-256** | `sha256` |
| **Watchdog** | `wdg_reboot`, `wdg_enable`, `wdg_kick` |
| **PIO Accel** | `crc32`, `trng_init`, `trng_read`, `pattern_init`, `pattern_out`, `pwm_pio_init`, `pwm_pio_set` |
| **BitBLT** | `blit_init`, `blit_fill` |

## Performance Benchmarks

Measured on Feather RP2350 (ARM Cortex-M33 @ 150 MHz, headless firmware).

| Operation | Time | Notes |
|-----------|------|-------|
| `1+1` | ~0.5ms | Expression parse + eval |
| `sha256("hello")` | ~1.2ms | 5 bytes, hardware-accelerated |
| `sha256` (256 bytes) | ~3ms | DMA streaming path |
| `crc32("hello")` | ~0.3ms | Software CRC-32 |
| `gpio_put` / `gpio_get` | ~0.4ms | GPIO toggle |
| `clock_get()` | ~0.3ms | Software clock read |
| `flash_save` (10 bytes) | ~15ms | Flash write (sector erase) |
| `flash_load` | ~0.5ms | Flash read (XIP) |
| `trng_read()` | ~2ms | 32-bit entropy from PIO TRNG |

### PIO Accelerator Performance (vs CPU)

| Operation | CPU | PIO | Speedup |
|-----------|-----|-----|---------|
| Framebuffer fill (640×400) | ~256K cycles | ~256K cycles† | 1x (CPU-free) |
| CRC-32 per byte | ~50 cycles | ~8 cycles | 6x |
| GPIO pattern (32-bit) | ~10 cycles | 1 cycle | 10x |
| TRNG (32 bits) | N/A (no CPU entropy) | ~64 cycles | ∞ |

† PIO runs in parallel — CPU continues executing while PIO fills.

## Sage REPL

On-device expression parser with arithmetic, variables, multi-line input, shell commands, and program storage.

```bash
>>> help       # Show all commands
>>> version    # SagePico v2.1 on RP2350
>>> free       # 8MB flash, 520KB SRAM
>>> uptime     # Seconds since boot
>>> reboot --boot  # Enter BOOTSEL mode
```

## Project Structure

```
examples/           # hello.sage, blink.sage
src/
  arm/c/            # ARM init.h
  riscv/c/          # RISC-V init.h  
  pico/
    c/              # 13 C bridges (sage_bridge, pio_accel, sha256, etc.)
    sage/           # 11 pure Sage modules (gpio, time, adc, i2c, etc.)
  tools/
    c/              # sagecom_tty.c, sagepicotool.c
    sage/           # sagecom.sage, sagepioasm.sage, sagepicotool.sage
tests/
  bench/            # Benchmarking suite
  test_*.py         # Hardware-in-the-loop tests (69 tests, 8 suites)
docs/               # Full documentation (12 files + site submodule)
build/              # Output UF2 files
deps/               # Git submodules (sagelang, pico-sdk, SageVM)
```

## Architecture Support

| Arch | Core | Status | USB CDC | REPL | Size |
|------|------|--------|---------|------|------|
| ARM | Cortex-M33 | **Production** | Working | Working | 94K text, 164K UF2 |
| RISC-V | Hazard3 | **Production** | Working | Working | 208K UF2 |

## Pure Sage Tools

| Tool | Description |
|------|-------------|
| **sagecom** | Serial terminal (91K native ELF) |
| **sagepioasm** | PIO assembly compiler (all 9 opcodes) |
| **sagepicotool** | USB BOOTSEL flash/reboot tool |
| **sageelf2uf2** | ELF→UF2 converter (ARM + RISC-V) |

## Dependencies

`cmake`, `python3`, `gcc-arm-none-eabi`. RISC-V toolchain bundled in `deps/pico-sdk-tools/`.
