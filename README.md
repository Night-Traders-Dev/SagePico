# SagePico: SageLang for RP2350

Build bootable UF2 firmware from Sage source targeting the Feather RP2350 (Cortex-M33 ARM / Hazard3 RISC-V). Headless REPL with 89 FFI functions, 100% pico-sdk coverage (35/35 libraries), PIO accelerators, and native Sage tools.

**Docs Site:** [night-traders-dev.github.io/SagePico-Docs](https://night-traders-dev.github.io/SagePico-Docs)

## Quick Start

```bash
./build.sh arm                    # Build for ARM (94K firmware)
sagepicotool load -f build/hello-arm.uf2  # Flash (our Sage port!)
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

## Native Bridge & FFI (89 functions, 18 categories)

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
| **System** | `vreg_get`, `vreg_set`, `clk_get`, `clk_gpout`, `xip_enable`, `xip_disable`, `xip_flush` |
| **Exception** | `exc_install` |
| **Power** | `powman_dormant` |
| **Low-Level** | `ticks_to_us`, `us_to_ticks`, `sync_lock`, `sync_unlock`, `spin_claim`, `hw_div`, `hw_mod`, `pll_freq`, `xosc_init`, `rv_cycle`, `rv_instret`, `rv_timer`, `rcp_avail` |

## Performance Benchmarks

Measured on Feather RP2350 (ARM Cortex-M33 @ 150 MHz, headless firmware v2.1).

### Transpiled C vs SRVM Bytecode

| Operation | Transpiled C | SRVM | Speedup |
|-----------|-------------|------|---------|
| `1+1` | 0.5ms | 0.2ms | **2.5x** |
| `2+3*4` | 0.6ms | 0.3ms | **2.0x** |
| `let x=42` | 0.5ms | 0.2ms | **2.5x** |
| `sha256("hello")` | 1.2ms | 0.8ms | **1.5x** |
| `gpio_put(7,1)` | 0.4ms | 0.3ms | **1.3x** |

### Per-Operation (Transpiled C)

| Operation | Time | Notes |
|-----------|------|-------|
| `1+1` | 0.5ms | Expression parse + eval |
| `sha256("hello")` | 1.2ms | 5 bytes, hardware SHA-256 |
| `sha256` (256 bytes) | 3ms | DMA streaming path |
| `crc32("hello")` | 0.3ms | Software CRC-32 |
| `gpio_put/get` | 0.4ms | GPIO toggle |
| `clock_get()` | 0.3ms | Software clock read |
| `flash_save` (10B) | 15ms | Flash sector erase + write |
| `flash_load` | 0.5ms | Flash XIP read |
| `trng_read()` | 2ms | 32-bit PIO TRNG entropy |
| `help` | 0.3ms | Shell command |
| `version` | 0.2ms | Shell command |

### SageRTOS Benchmarks

| Operation | Time | Notes |
|-----------|------|-------|
| `rtos_yield()` | 0.2ms | Context switch via PendSV |
| `rtos_sleep(1)` | 1.2ms | 1ms SysTick sleep |
| `rtos_id()` | 0.1ms | Task ID lookup |
| Task create (128 words) | 0.3ms | Stack frame setup |
| Task notify | 0.1ms | Single word write |

### PIO Accelerator Performance (vs CPU)

SRVM (Sage RISC-V Machine) is a compact bytecode format using fixed 32-bit RISC-V instructions. The `sgvmc --riscv` compiler produces `.sgrv` files (~200 bytes for simple programs). Our on-device `rvvm.h` interpreter now supports SRVM's `OP_VMSYS` (0x73) opcode for VM operations (HALT, PRINT, GET/SET_GLOBAL) and includes a 64-entry heap dict for global variable storage.

**Key advantage**: SRVM programs are 10-50x smaller than transpiled C firmware and execute without the Sage runtime overhead. Ideal for compute-bound loops on PIO accelerators.

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
examples/               # hello.sage, blink.sage
src/
  arm/c/                # ARM init.h
  riscv/c/              # RISC-V init.h
  pico/
    c/
      core/             # sage_bridge.h, pico_port.h
      storage/          # flash_store.h
      pio/              # pio_bridge.h, pio_accel.h, pio_bitblt.h
      dma/              # dma_bridge.h
      crypto/           # sha256_bridge.h
      vm/               # rvvm.h, gfx_vm.h
      system/           # sysctrl, excpm, powman, rtc, dualcore, lowlevel
      math/             # interp_bridge.h
      display/          # hstx_display.h
    sage/
      io/               # gpio, adc, pwm, uart, i2c, spi
      time/             # time, clock
      storage/          # flash
      pio/              # pio
      dma/              # dma
      crypto/           # sha256
      vm/               # gfx_vm, elf2uf2
  tools/
    c/                  # sagecom_tty.c, sagepicotool.c
    sage/               # sagecom.sage, sagepioasm.sage, sagepicotool.sage
tests/
  bench/                # Benchmarking suite
  test_*.py             # Hardware-in-the-loop tests
docs/                   # Full documentation + site submodule
build/                  # Output UF2 files
deps/                   # Git submodules
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
