# SagePico: SageLang for RP2350

Build bootable UF2 firmware from Sage source code targeting the Feather RP2350 (Cortex-M33 ARM / Hazard3 RISC-V).

## Quick Start

```bash
# Build for ARM (recommended)
./build.sh arm

# Build for RISC-V
./build.sh riscv

# Flash automatically
make flash-arm

# Read serial output
cat /dev/ttyACM0
```

Output:
```
=== SagePico (arm) HSTX ===
HSTX init OK
Pattern drawn
SagePico: Hello from Sage on RP2350!
Native pico bridge loaded
Frame 0
Frame 1
...
```

## Build Paths

### Path 1: Baremetal (Sage -> C -> UF2) ✓ Production

```
src/hello.sage  →  sage --emit-pico-c  →  hello.c  →  patching  →  CMake + Pico SDK  →  build/hello-{arch}.uf2
```

The Sage compiler generates a C file with the full Sage runtime. The build pipeline:
1. Patches includes for baremetal (`dlfcn.h`, `semaphore.h`, `pthread.h` → shims)
2. Replaces `fputs`/`fputc` with `printf` (newlib fd layer crashes baremetal)
3. Replaces `sage_print_ln(sage_string("..."))` with `printf("...\n")`
4. Injects `sage_bridge.h` — SageValue-wrapped pico_port functions + baremetal FFI
5. Adds `pico_port.h` and `hstx_display.h` for GPIO/display
6. Configures HSTX DVI display output + scanline render loop

### Path 2: SageVM SRVM (Sage -> Bytecode) ✓ Working

```
src/hello.sage  →  sagevm compile --riscv  →  src/hello.sgrv (compact RISC-V bytecode)
```

Desktop-runnable via `sagevm run`. Embedding SRVM interpreter on-device is planned.

## Native Bridge & FFI

### `sage_bridge.h` — SageValue Native Bridge

`src/pico/sage_bridge.h` wraps all 7 pico_port peripherals as SageValue-native C functions:

| Module | Functions |
|--------|-----------|
| GPIO | `gpio_init`, `gpio_set_dir`, `gpio_put`, `gpio_get`, `gpio_pull_up/down`, `gpio_set_function` |
| Time | `sleep_ms`, `sleep_us`, `time_us`, `time_ms` |
| UART | `init`, `putc`, `puts`, `getc`, `readable` |
| ADC | `init`, `gpio_init`, `select`, `read` |
| PWM | `setup`, `duty` |
| I2C | `init`, `write`, `read` |
| SPI | `init`, `transfer` |

### FFI Dispatch System

The bridge implements `ffi_open` and `ffi_call` for baremetal, replacing the `dlopen`-based stubs. Sage code uses the standard FFI pattern:

```sage
let pico = ffi_open("pico")
ffi_call(pico, "gpio_init", [7])
ffi_call(pico, "gpio_set_dir", [7, 1])
ffi_call(pico, "gpio_put", [7, 1])
ffi_call(pico, "sleep_ms", [250])
ffi_call(pico, "gpio_put", [7, 0])
```

The bridge also populates `sage_init_native_module()` so Sage `import` statements resolve:
```sage
import pico          # Returns dict with all GPIO functions
import time          # Returns dict with sleep_ms, sleep_us, time_us, time_ms
import uart          # Returns dict with UART functions
```

### `pico_port.h` — Direct C Bridge

`src/pico/pico_port.h` provides zero-overhead `static inline` C wrappers for direct C usage (no SageValue overhead):

| Module | Functions |
|--------|-----------|
| GPIO | `sage_gpio_init`, `sage_gpio_set_dir`, `sage_gpio_put`, `sage_gpio_get`, `sage_gpio_pull_up`, `sage_gpio_pull_down` |
| UART | `sage_uart_init`, `sage_uart_putc`, `sage_uart_puts`, `sage_uart_getc` |
| Time | `sage_sleep_ms`, `sage_sleep_us`, `sage_time_us`, `sage_time_ms` |
| Print | `sage_print(msg)` — printf-based, no Sage runtime needed |
| ADC | `sage_adc_init`, `sage_adc_gpio`, `sage_adc_select`, `sage_adc_read` |
| PWM | `sage_pwm_gpio`, `sage_pwm_duty` |
| I2C | `sage_i2c_init`, `sage_i2c_write`, `sage_i2c_read` |
| SPI | `sage_spi_init`, `sage_spi_xfer` |

## HSTX DVI Display

`src/pico/hstx_display.h` drives a DVI/HDMI display via the RP2350's hardware TMDS encoder on the HSTX peripheral:

- 640x400 internal framebuffer, 2x scaled to 1280x800 output
- 256-color palette (8bpp indexed)
- Hardware TMDS command expander with proper opcode encoding (`0x2 << 12`)
- Differential output on GPIO 12-19 (8 pins, 3 data lanes + clock)
- GT911 touch controller on I2C0 (GPIO 22/23)
- Scanline render loop pushing 2 pixels per FIFO word at 252 MHz system clock

## Project Structure

```
src/
  hello.sage             # Sage program entry (FFI-based)
  blink.sage             # GPIO blink demo using FFI
  pico/
    pico_port.h          # Direct C bridge: GPIO, UART, Time, Print, ADC, PWM, I2C, SPI
    sage_bridge.h         # SageValue native bridge + FFI dispatch + module init
    hstx_display.h        # HSTX DVI display driver (TMDS encoder, framebuffer, touch)
    gpio.sage             # Sage GPIO module (importable)
    gpio_wrap.c           # GPIO SageValue wrapper (standalone, not linked in build)
    elf2uf2.sage          # Pure-Sage ELF-to-UF2 converter (desktop)
shims/                   # Baremetal compatibility
  dlfcn.h, semaphore.h, stdatomic.h   # Missing POSIX headers
  pthread.h              # Minimal pthread declarations (avoids newlib TLS init)
  stubs.c                # pthread/nanosleep link stubs
  baremetal_stubs.h      # Comprehensive unified stub
patch_stdio.py           # Replaces fputs/fputc with printf
patch_main.py            # Injects sage_bridge.h, pico_port.h, hstx_display, render loop
build.sh                 # Full 2-path build pipeline (ARM + RISC-V)
build/                   # Output UF2 files
  hello-arm.uf2          # ARM Cortex-M33 image (~86KB)
  hello-riscv.uf2        # Hazard3 RISC-V image (~109KB)
docs/                    # Architecture and board docs
deps/                    # Git submodules
  sagelang/              # SageLang compiler v3.8.7
  SageVM/                # SageVM v0.9.8 with SRVM backend
  pico-sdk/              # Raspberry Pi Pico SDK (RP2350 support)
  pico-sdk-tools/        # RISC-V toolchain + picotool 2.2.0
```

## Flashing

```bash
make flash-arm     # Build + flash ARM image
make flash-riscv   # Build + flash RISC-V image
```

picotool auto-reboots the board into BOOTSEL mode, flashes the UF2, and reboots back. Requires udev rules (installed on first run).

Manual: hold BOOTSEL, tap RESET, copy UF2 to the `RPI-RP2` USB drive.

## Serial Output

USB CDC at `/dev/ttyACM0` (115200 baud). May need `dialout` group:
```bash
sudo usermod -a -G dialout $USER   # then log out/in
```

## Architecture Support

| Arch | Core | Status | Notes |
|------|------|--------|-------|
| ARM | Cortex-M33 | Working | USB CDC, printf, GPIO, HSTX DVI display |
| RISC-V | Hazard3 | Compiles | UF2 builds, USB CDC silent (interrupt difference) |

## Known Issues

- **fputs/fputc crash**: newlib's file descriptor layer segfaults on baremetal. Workaround: all patched to `printf`.
- **RISC-V USB CDC**: program runs but no serial output. Likely interrupt handling difference in Hazard3 vs Cortex-M33 NVIC.
- **RISC-V toolchain**: bundled `riscv32-unknown-elf-gcc` in `deps/pico-sdk-tools/build/riscv-install/`.

## Dependencies

All in `deps/` as git submodules. Host system needs: `cmake`, `python3`, `gcc-arm-none-eabi`. RISC-V toolchain is bundled.
