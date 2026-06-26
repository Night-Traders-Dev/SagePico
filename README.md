# SagePico: SageLang for RP2350

Bootable Sage programs for the [Adafruit Feather RP2350](https://www.adafruit.com/product/6000) with Hazard3 RISC-V cores.

## Quick Start

```bash
# Run Sage interpreter (desktop)
./deps/sagelang/sage src/hello.sage

# Build bootable UF2 image for Feather RP2350
./build.sh

# Flash to board (bootloader mode)
# Copy build/hello.uf2 to the RPI-RP2 USB drive
```

## Build Paths

### Path 1: Baremetal C (Sage -> C -> UF2)

```
src/hello.sage  -->  sage --emit-pico-c  -->  hello.c  -->  CMake + Pico SDK + riscv32-gcc  -->  build/hello.uf2
```

### Path 2: SageVM SRVM (Sage -> Bytecode)

```
src/hello.sage  -->  sagevm compile --riscv  -->  src/hello.sgrv (RISC-V bytecode)
```

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
docs/                   # Documentation
  architecture.md       # Build paths and architecture
  feather_rp2350.md     # Board specs and pinout
deps/                   # Dependencies
  sagelang/             # SageLang compiler + tools
  SageVM/               # Sage Virtual Machine
  pico-sdk/              # Raspberry Pi Pico SDK
  pico-sdk-tools/        # RISC-V toolchain (riscv32-unknown-elf-gcc)
build.sh                # Full build script
build/hello.uf2         # Bootable image (72KB)
```

## Target Hardware

| Feature | Value |
|---------|-------|
| Board | Adafruit Feather RP2350 |
| MCU | RP2350B |
| Core | Hazard3 RISC-V (rv32imac) |
| Flash | 8MB |
| LED | GPIO 7 |

## Dependencies

Included in `deps/`:
- SageLang v3.8.7 - Sage compiler
- SageVM v0.9.8 - Sage Virtual Machine with SRVM backend
- Pico SDK - Raspberry Pi Pico SDK
- Pico SDK Tools - RISC-V GNU toolchain (riscv32-unknown-elf-gcc 15.2.0)

System requirements: `cmake`, `python3`
