# SagePico: SageLang for Feather RP2350

Build bootable UF2 firmware images from Sage source code for the [Adafruit Feather RP2350](https://www.adafruit.com/product/6000).

## Quick Start

```bash
# 1. Build the UF2 image
./build.sh

# 2. Flash to the board (auto-reboot + flash via picotool)
make flash

# 3. Read serial output (USB CDC, /dev/ttyACM0)
cat /dev/ttyACM0
```

## Build Paths

### Path 1: Baremetal C (Sage -> C -> UF2) ✓ Working

```
src/hello.sage  -->  sage --emit-pico-c  -->  hello.c  -->  CMake + Pico SDK + riscv32-gcc  -->  build/hello.uf2
```

The Sage compiler generates a standalone C file containing the full Sage runtime (GC, types, print). This is cross-compiled for Hazard3 RISC-V using the bundled `riscv32-unknown-elf-gcc 15.2.0` toolchain. Output is a UF2 firmware image (~72KB).

### Path 2: SageVM SRVM (Sage -> Bytecode) ✓ Working

```
src/hello.sage  -->  sagevm compile --riscv  -->  src/hello.sgrv (RISC-V bytecode, 61 bytes)
```

The SageVM compiler produces compact SRVM bytecode. Running the SRVM runtime directly on the RP2350 is planned as a next step (requires embedding the SRVM interpreter in a baremetal binary).

## Project Structure

```
src/hello.sage          # "Hello from Sage on RP2350!" program
src/hello.sgvm          # SageVM bytecode (53 bytes)
src/hello.sgrv          # SRVM RISC-V bytecode (61 bytes)
shims/                  # Baremetal compatibility shims
  dlfcn.h               # dlopen/dlclose stubs
  semaphore.h           # sem_init/sem_wait stubs  
  stdatomic.h           # __atomic_* shims
  stubs.c               # pthread/timer link stubs
  baremetal_stubs.h     # Additional inline stubs
  baremetal_time.h      # Time function stubs
docs/                   # Documentation
  architecture.md       # Build paths and architecture
  feather_rp2350.md     # Board specs, pinout, toolchain
deps/                   # Git submodules
  sagelang/             # SageLang compiler v3.8.7
  SageVM/               # SageVM v0.9.8 (SGVM + SRVM)
  pico-sdk/              # Raspberry Pi Pico SDK
  pico-sdk-tools/        # RISC-V toolchain + picotool
build.sh                # Full build script (Sage -> UF2)
build/hello.uf2         # Bootable image
```

## Flashing

### Automated (picotool)

```bash
make flash     # Build + auto-reboot + flash in one command
```

picotool detects the board over USB, reboots it into BOOTSEL mode, flashes the UF2, and reboots back to application mode. Requires udev rules (installed by `./build.sh` first run).

### Manual

1. Hold BOOTSEL button, tap RESET, release BOOTSEL
2. Board appears as USB drive `RP2350/`
3. Copy `build/hello.uf2` to the drive
4. Board auto-reboots

## Serial Output

The program outputs "Hello from Sage on RP2350!" via USB CDC serial (`/dev/ttyACM0`, 115200 baud). Permissions may require adding your user to the `dialout` group:

```bash
sudo usermod -a -G dialout $USER
# Log out and back in for group membership to take effect
```

## Target Hardware

| Feature | Value |
|---------|-------|
| Board | Adafruit Feather RP2350 |
| MCU | RP2350A |
| Core | Hazard3 RISC-V (rv32imac_zicsr_zifencei_zba_zbb_zbs_zbkb) |
| Flash | 8MB |
| LED | GPIO 7 (red) |
| NeoPixel | GPIO 21 |
| UART0 | TX=GPIO0, RX=GPIO1 |

## Dependencies

All included as git submodules in `deps/`:
- **SageLang** v3.8.7 — Sage compiler and C code generator
- **SageVM** v0.9.8 — SageVM with SGVM and SRVM (RISC-V) bytecode backends
- **Pico SDK** — Raspberry Pi Pico SDK with RP2350/Hazard3 support
- **Pico SDK Tools** — RISC-V GNU toolchain (`riscv32-unknown-elf-gcc` 15.2.0) + picotool

System: `cmake`, `python3`, `libusb-1.0-dev` (for picotool USB flashing)
