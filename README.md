# SagePico: SageLang for RP2350

Build bootable UF2 firmware from Sage source targeting the Feather RP2350 (Cortex-M33 ARM / Hazard3 RISC-V). Features an on-device Sage REPL with DVI display output and flash-backed persistence.

## Quick Start

```bash
# Build for ARM (recommended)
./build.sh arm

# Build for RISC-V
./build.sh riscv

# Flash (board must be in BOOTSEL mode)
picotool load -f build/hello-arm.uf2

# Read serial output
cat /dev/ttyACM0
```

On boot the DVI display shows a color bar, boot banner, then drops into a **Sage REPL** visible on both the DVI screen and USB serial:
```
=== Sage REPL ===
>>> gpio_put(7, 1)
>>> let x = 42
  x = 42
>>> x * 2
84
>>> ws2812_init(16, 0)
>>> quit
Saving vars to flash...
Resuming display
```

## Build Paths

### Path 1: Baremetal (Sage -> C -> UF2) ✓ Production

```
src/hello.sage  →  sage --emit-pico-c  →  hello.c  →  patching  →  CMake + Pico SDK  →  build/hello-{arch}.uf2
```

The Sage compiler generates C with the full Sage runtime. The pipeline:
1. Patches includes for baremetal (`dlfcn.h` → shims)
2. Replaces `fputs`/`fputc` with `printf` (newlib fd layer broken on baremetal)
3. Strips old FFI/module stubs (`dlopen`-based, don't work)
4. Injects `flash_store.h` + `sage_bridge.h` + `pio_bridge.h` — native peripherals
5. Adds `pico_port.h` + `hstx_display.h` — C bridges + DVI driver
6. Configures HSTX display, framebuffer console, then enters Sage REPL

### Path 2: SageVM SRVM (Sage -> Bytecode) ✓ Working

```
src/hello.sage  →  sagevm compile --riscv  →  src/hello.sgrv (compact RISC-V bytecode)
```

Desktop-runnable via `sagevm run`. On-device SRVM interpreter planned.

## Native Bridge & FFI

### Peripherals Available via FFI (`ffi_call`)

| Category | Functions |
|----------|-----------|
| **GPIO** | `gpio_init`, `gpio_set_dir`, `gpio_put`, `gpio_get`, `gpio_pull_up/down`, `gpio_set_function` |
| **Time** | `sleep_ms`, `sleep_us`, `time_us`, `time_ms` |
| **UART** | `uart_init`, `uart_putc`, `uart_puts`, `uart_getc`, `uart_readable` |
| **ADC** | `adc_init`, `adc_gpio`, `adc_select`, `adc_read` |
| **PWM** | `pwm_setup`, `pwm_duty` |
| **I2C** | `i2c_init`, `i2c_write`, `i2c_read` |
| **SPI** | `spi_init`, `spi_transfer` |
| **PIO** | `pio_claim`, `pio_put`, `pio_get`, `pio_set_pins`, `pio_set_enabled`, `pio_set_clkdiv`, `pio_clear_fifos` |
| **WS2812** | `ws2812_init(gpio, pio_idx)`, `ws2812_put(pio, sm, r, g, b)` |
| **Flash** | `flash_save(key, val)`, `flash_load(key)`, `flash_del(key)`, `flash_keys()` |

### FFI Pattern

```sage
let pico = ffi_open("pico")
ffi_call(pico, "gpio_put", [7, 1])
ffi_call(pico, "sleep_ms", [500])
ffi_call(pico, "ws2812_init", [16, 0])
let sm = ffi_call(pico, "pio_claim", [0, 0])
ffi_call(pico, "ws2812_put", [0, sm, 255, 0, 0])  # red NeoPixel
```

Native `import` statements also resolve:
```sage
import pico          # GPIO functions
import time          # sleep_ms, time_us, etc.
import uart          # UART functions
```

## Sage REPL

A full expression parser runs on-device accessible via USB serial and mirrored to the DVI display:

- **Arithmetic**: `2 + 2`, `x * 3`, `10 / 3`, `"hello" + " world"`
- **Variables**: `let x = 42`, persist across power cycles via flash
- **Comparisons**: `x == 42`, `y < 10`
- **Arrays**: `[1, 2, 3]`
- **Function calls**: all FFI peripherals (see table above)
- **Ctrl+C**: exits REPL, saves variables to flash, resumes DVI display loop
- **`quit`** / **`exit`**: same as Ctrl+C

## Flash Persistence

`src/pico/flash_store.h` implements a log-structured wear-leveled key-value store in the upper 2MB of the 8MB flash chip. Features:
- Sequential writes across 4KB sectors with monotonic sequence numbers
- Automatic compaction/GC when sectors fill
- REPL variables auto-saved on quit/Ctrl+C, auto-restored on boot
- Survives power cycles (tested across 10+ reboots)

## HSTX DVI Display

`src/pico/hstx_display.h` drives DVI/HDMI via the RP2350 hardware TMDS encoder:
- 640x400 internal framebuffer, 2x scaled to 1280x800 output
- 256-color palette (8bpp indexed)
- **Framebuffer console**: 80x50 text on 8x8 bitmap font (95 printable ASCII)
- Hardware TMDS command expander: opcode at bits [15:12], RGB888 packing
- Differential output on GPIO 12-19 (8 pins, 3 data lanes + clock)
- GT911 touch controller on I2C0 (GPIO 22/23)
- 252 MHz system clock with HSTX CLKDIV=8

## PIO & WS2812

`src/pico/pio_bridge.h` provides PIO state machine control + WS2812 NeoPixel driver:
- Claim/release state machines, set pin mappings, clock dividers
- TX/RX FIFO push/pull, FIFO status
- Built-in WS2812 PIO program (8 MHz, sideset for precise timing)
- Single-LED and multi-LED write APIs

## Project Structure

```
src/
  hello.sage             # Sage program entry (FFI-based)
  blink.sage             # GPIO blink demo using FFI
  pico/
    pico_port.h          # Direct C bridge: GPIO, UART, Time, Print, ADC, PWM, I2C, SPI
    sage_bridge.h        # SageValue native bridge + FFI dispatch + module init
    hstx_display.h       # HSTX DVI display driver (TMDS encoder, framebuffer console, touch)
    flash_store.h        # Flash-backed key-value store (wear-leveled, 2MB)
    pio_bridge.h         # PIO state machine + WS2812 NeoPixel driver
    gpio.sage            # Sage GPIO module (importable)
    gpio_wrap.c          # GPIO SageValue wrapper (standalone, not linked in build)
    elf2uf2.sage         # Pure-Sage ELF-to-UF2 converter (desktop)
shims/                   # Baremetal compatibility
  dlfcn.h, semaphore.h, stdatomic.h   # Missing POSIX headers
  pthread.h              # Minimal pthread declarations (avoids newlib TLS init)
  stubs.c                # pthread/nanosleep link stubs
patch_stdio.py           # Replaces fputs/fputc with printf
patch_main.py            # Injects bridges + drivers + RISC-V USB fix + REPL init
build.sh                 # Full 2-path build pipeline (ARM + RISC-V)
build/                   # Output UF2 files
  hello-arm.uf2          # ARM Cortex-M33 image (~86KB)
  hello-riscv.uf2        # Hazard3 RISC-V image (~109KB)
docs/                    # Architecture and board docs
deps/                    # Git submodules
  sagelang/              # SageLang compiler v3.8.7
  SageVM/                # SageVM v0.9.8 with SRVM backend
  pico-sdk/              # Raspberry Pi Pico SDK (RP2350 support)
  pico-sdk-tools/        # RISC-V toolchain + picotool
```

## Flashing

```bash
# Board must be in BOOTSEL mode
picotool load -f build/hello-arm.uf2

# Or use make targets
make flash-arm     # Build + flash ARM
make flash-riscv   # Build + flash RISC-V
```

picotool auto-reboots into BOOTSEL mode, flashes, and reboots back.

## Architecture Support

| Arch | Core | Status | Notes |
|------|------|--------|-------|
| ARM | Cortex-M33 | **Working** | USB CDC, printf, GPIO, HSTX DVI, REPL, flash persistence |
| RISC-V | Hazard3 | **Testing** | USB fix applied (reset + IRQ priority), awaiting confirmation |

## Known Issues

- **fputs/fputc crash**: newlib's file descriptor layer segfaults on baremetal. Patched to `printf`.
- **RISC-V USB CDC**: fix applied — explicit USBCTRL reset + IRQ priority 0x00 after init. Needs hardware test.
- **RISC-V toolchain**: bundled `riscv32-unknown-elf-gcc 15.2.0` in `deps/pico-sdk-tools/`.

## Dependencies

All in `deps/` as git submodules. Host: `cmake`, `python3`, `gcc-arm-none-eabi`. RISC-V toolchain is bundled.
