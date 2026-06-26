# SagePico Architecture

## Build Paths

### Path 1: Baremetal C Compilation (Sage -> C -> UF2)

```
src/hello.sage
    │
    ├── sage interpreter (run directly)
    │
    └── sage --emit-pico-c
            │
            └── src/hello.c  (generated C with Sage runtime)
                    │
                    └── CMake + Pico SDK + riscv32-gcc
                            │
                            └── build/hello.uf2  (bootable image)
```

Uses the Sage compiler's C backend to generate a self-contained C file including the full Sage runtime (VM, GC, types). Compiles with the Pico SDK for baremetal execution.

### Path 2: SageVM SRVM (Sage -> SGVM -> SRVM -> UF2)

```
src/hello.sage
    │
    └── sagevm compile --riscv
            │
            ├── src/hello.sgvm  (SageVM bytecode)
            │       │
            │       └── srvm_compiler (translate to RISC-V bytecode)
            │               │
            │               └── src/hello.sgrv
            │
            └── Future: SRVM runtime embedded on RP2350
                    └── build/hello_srvm.uf2
```

Produces compact bytecode (.sgvm / .sgrv) that runs on SageVM. The SRVM backend adds RISC-V native instruction translation. Requires embedding the SRVM interpreter on the RP2350.

## Directory Structure

```
SagePico/
├── src/               # Sage source files
│   └── hello.sage     # Hello world program
├── deps/              # Dependencies (git submodules)
│   ├── sagelang/      # SageLang compiler + tools
│   ├── SageVM/        # Sage Virtual Machine
│   ├── pico-sdk/       # Raspberry Pi Pico SDK
│   └── pico-sdk-tools/ # RISC-V toolchain + pico tools
├── build/             # Build output (UF2 files)
├── docs/              # Documentation
├── Makefile           # Build system
└── build.sh           # Manual build script
```
