# SagePico: SageLang for RP2350

Build bootable UF2 firmware from Sage source code targeting the Feather RP2350 (Cortex-M33 ARM core).

## Quick Start

```bash
# Build for ARM (recommended)
./build.sh arm

# Flash automatically  
make flash-arm

# Read serial output
cat /dev/ttyACM0
```

Output:
```
=== SagePico (arm) ===
Hello from Sage on RP2350!
SagePico uptime 2s
SagePico uptime 3s
...
```

## Build Paths

### Path 1: Baremetal (Sage -> C -> UF2) ✓ Production

```
src/hello.sage  →  sage --emit-pico-c  →  hello.c  →  patching  →  CMake + Pico SDK + arm-none-eabi-gcc  →  build/hello-arm.uf2
```

The Sage compiler generates a C file with the full Sage runtime. The build pipeline:
1. Patches includes for baremetal (`dlfcn.h`, `semaphore.h`, `pthread.h` → shims)
2. Replaces `fputs`/`fputc` with `printf` (newlib fd layer crashes baremetal)
3. Replaces `sage_print_ln(sage_string("..."))` with `printf("...\n")`
4. Removes Sage GC/runtime calls (not needed for simple programs)
5. Adds `pico_port.h` bridge for GPIO/UART/etc.
6. Adds LED heartbeat on GPIO 7

### Path 2: SageVM SRVM (Sage -> Bytecode) ✓ Working

```
src/hello.sage  →  sagevm compile --riscv  →  src/hello.sgrv (61 bytes)
```

Compact RISC-V bytecode. Desktop-runnable via `sagevm run`. Embedding SRVM interpreter on-device is planned.

## pico_port.h Bridge

`src/pico/pico_port.h` provides a zero-overhead Sage-to-Pico SDK bridge — lightweight `static inline` C wrappers:

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

## Project Structure

```
src/
  hello.sage            # Sage source program
  blink.sage            # GPIO blink demo (WIP)
  pico/
    pico_port.h         # C bridge: GPIO, UART, Time, Print, ADC, PWM, I2C, SPI
    gpio.sage           # Sage GPIO module stubs
    gpio_wrap.c         # C GPIO wrapper
shims/                  # Baremetal compatibility
  dlfcn.h, semaphore.h, stdatomic.h   # Missing POSIX headers
  pthread.h             # Minimal pthread declarations
  stubs.c               # pthread/nanosleep link stubs
patch_stdio.py          # Replaces fputs/fputc with printf
patch_main.py           # Replaces Sage runtime calls, adds LED, pico_port
build.sh                # Full build pipeline
build/                  # Output UF2 files
  hello-arm.uf2         # ARM Cortex-M33 image (~50KB)
  hello-riscv.uf2       # Hazard3 RISC-V image (~65KB)
docs/                   # Architecture and board docs
deps/                   # Git submodules
  sagelang/             # SageLang compiler v3.8.7
  SageVM/               # SageVM v0.9.8 with SRVM backend
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
| ARM | Cortex-M33 | ✓ Working | USB CDC, printf, GPIO, LED |
| RISC-V | Hazard3 | ⚠ Compiles | UF2 builds, USB CDC silent |

## Known Issues

- **fputs/fputc crash**: newlib's file descriptor layer segfaults on baremetal. Workaround: all patched to `printf`.
- **Sage GC runtime**: linking it pulls in `exit`/`_sbrk` which corrupt baremetal startup. Workaround: bypassed for simple programs.
- **RISC-V USB CDC**: program runs but no serial output. Likely interrupt handling difference in Hazard3 vs Cortex-M33 NVIC.
- **RISC-V toolchain**: bundled `riscv32-unknown-elf-gcc 15.2.0` in `deps/pico-sdk-tools/build/riscv-install/`.

## Dependencies

All in `deps/` as git submodules. Host system needs: `cmake`, `python3`, `gcc-arm-none-eabi`. RISC-V toolchain is bundled.
