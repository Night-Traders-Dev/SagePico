# SagePico Architecture

## Build Paths

### Path 1: Baremetal C Compilation (Sage -> C -> UF2) ✓

```
src/hello.sage
    │
    ├── sage interpreter (desktop test)
    │
    └── sage --emit-pico-c
            │
            └── .tmp/hello/hello.c  (generated C + full Sage runtime)
                    │
                    ├── shim patching (dlfcn, semaphore, pthread stubs)
                    │
                    └── CMake + Pico SDK + riscv32-unknown-elf-gcc
                            │
                            └── build/hello.uf2  (bootable image, ~72KB)
```

The Sage compiler's `--emit-pico-c` flag generates a self-contained C file (~1600 lines) including:
- Full Sage value type system (SageValue, SageArray, SageDict, SageTuple)
- Mark-sweep garbage collector
- `print`, `str`, `len`, and other built-in functions
- Stubbed thread/FFI/semaphore functions (not used by simple programs)

The generated C is patched before compilation to:
1. Replace POSIX headers unavailable on baremetal (`dlfcn.h`, `semaphore.h`)
2. Enable `_POSIX_THREADS` and `_POSIX_TIMERS` macros for newlib declarations
3. Add an infinite loop at end of `main()` to keep USB CDC alive
4. Add an early `printf` for boot confirmation

### Path 2: SageVM SRVM (Sage -> Bytecode) ✓

```
src/hello.sage
    │
    └── sagevm compile --riscv
            │
            ├── src/hello.sgvm  (SageVM bytecode, 53 bytes)
            │       │
            │       └── srvm_compiler (translate to RISC-V bytecode)
            │               │
            │               └── src/hello.sgrv  (SRVM, 61 bytes)
            │
            └── Future: Embed SRVM interpreter on RP2350
                    └── Run bytecode directly on Hazard3 core
```

The SageVM compiler produces two formats:
- **SGVM** (.sgvm) — Stack-based SageVM bytecode, runs on desktop via `sagevm run`
- **SRVM** (.sgrv) — RISC-V native bytecode, produced by translating SGVM through the SRVM compiler

Next step: embed the SRVM interpreter (written in C or Sage) as a baremetal binary on the RP2350 to execute .sgrv files directly on the Hazard3 core.

## Shim Layer

The `shims/` directory contains compatibility stubs for baremetal compilation:

| File | Purpose |
|------|---------|
| `dlfcn.h` | dlopen/dlclose/dlsym stubs (FFI, not available baremetal) |
| `semaphore.h` | sem_init/sem_wait/sem_post stubs (no OS semaphores) |
| `stdatomic.h` | __atomic_load/__atomic_store shims |
| `stubs.c` | Link-time implementations for pthread_create, pthread_mutex_*, nanosleep |
| `baremetal_stubs.h` | Inline stubs for pthread, dlfcn (alternative approach) |
| `baremetal_time.h` | nanosleep/clock stubs |

These shims are necessary because the Sage compiler's C backend emits POSIX-dependent code even when targeting Pico. A future SageLang update should make `--emit-pico-c` produce baremetal-compatible C directly.

## Build Script (`build.sh`)

The build script performs these steps:

1. **Generate C** — `sage --emit-pico-c src/hello.sage -o .tmp/hello/hello.c`
2. **Patch C** — sed/python fixes for baremetal compatibility:
   - Replace `#include <dlfcn.h>` → `#include "dlfcn.h"` (shim)
   - Replace `#include <semaphore.h>` → `#include "semaphore.h"` (shim)
   - Replace `#include <stdatomic.h>` → `#include "stdatomic.h"` (shim)
   - Add `#define _POSIX_THREADS 1` and `#define _POSIX_TIMERS 1`
   - Add `while(1) { tight_loop_contents(); }` before main's return
   - Add early `printf("=== SagePico booting... ===\n")` after `stdio_init_all()`
3. **Write CMakeLists.txt** — Targets `adafruit_feather_rp2350`, `rp2350-riscv` platform, includes shims and stubs
4. **CMake Configure** — Sets `PICO_TOOLCHAIN_PATH` to bundled RISC-V toolchain
5. **Build** — `cmake --build` produces `.elf`, `.bin`, `.uf2`
6. **SRVM** — `sagevm compile --riscv` produces `.sgvm` and `.sgrv` bytecode

## Flashing (`make flash`)

Uses picotool (built with libusb support) to:
1. Detect the board over USB
2. Send reboot-to-BOOTSEL command
3. Load the UF2 file
4. Reboot back to application mode

picotool is installed to `~/.local/bin/picotool` during the first build.
