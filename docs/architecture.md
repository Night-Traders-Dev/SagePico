# SagePico Architecture

## Build Pipeline

```
src/hello.sage                    # Sage source
    │
    ├── sage --emit-pico-c        # Step 1: Generate C
    │       └── hello.c           # ~1600 lines, full Sage runtime
    │
    ├── patch_stdio.py            # Step 2: Fix baremetal stdio
    │       ├── fputs → printf    #   newlib fd layer crashes
    │       ├── fputc → printf    #   same issue
    │       └── exit → while(1)   #   exit() halts CPU
    │
    ├── patch_main.py             # Step 3: Bridge to pico_port
    │       ├── sage_print_ln → printf   # bypass Sage GC/runtime
    │       ├── remove GC calls          # not needed
    │       ├── add #include "pico_port.h"
    │       └── add LED heartbeat
    │
    ├── CMakeLists.txt            # Step 4: Build system
    │       ├── PICO_BOARD=adafruit_feather_rp2350
    │       ├── PICO_PLATFORM=rp2350-arm-s or rp2350-riscv
    │       └── link pico_stdlib + hardware_adc/pwm/i2c/spi
    │
    └── riscv32-gcc or arm-gcc   # Step 5: Compile + Link
            └── hello.uf2         # Bootable image
```

## Why the Sage Runtime Crashes on Baremetal

Three root causes found through extensive bisecting:

### 1. `fputs`/`fputc` segfault
The generated C uses `fputs(value, stdout)` for printing strings and `fputc(char, stdout)` for characters. These go through newlib's file descriptor layer (`_write` syscall) which isn't properly wired on baremetal Pico. The Pico SDK provides its own `printf` that bypasses this layer.

**Fix**: `patch_stdio.py` replaces all `fputs`/`fputc` with `printf` equivalents.

### 2. `exit()` halts the CPU  
The Sage runtime's `sage_fail` calls `exit(1)`. newlib's `_exit` implementation on baremetal executes a `bkpt` (breakpoint) instruction and infinite loops. Even though `sage_fail` isn't called for simple programs, the linker includes `exit` because it's referenced by `sage_fail`, and the mere presence of `exit` in the binary causes issues during C runtime initialization.

**Fix**: `patch_stdio.py` replaces `exit(1)` with `while(1) { tight_loop_contents(); }`.

### 3. Full Sage GC/Runtime link
When any Sage function is called from `main()`, the linker pulls in the full Sage runtime (GC, type system, native functions). This drags in `_sbrk`, `exit`, `pthread_self`, and other newlib stubs that interact poorly with the Pico's baremetal C runtime. The program crashes before reaching `main()`.

**Fix**: `patch_main.py` replaces `sage_print_ln(sage_string("..."))` with direct `printf("...\n")` and removes `sage_gc_push_frame`/`sage_gc_shutdown` calls, allowing the linker to dead-strip the entire Sage runtime.

## pico_port.h Bridge

The `src/pico/pico_port.h` bridge provides lightweight C wrappers for Pico SDK peripherals. All functions are `static inline`, incurring zero call overhead after compiler optimization.

This bridge serves as the foundation for porting pico-sdk to Sage:
- Sage programs call `sage_print()`, `sage_gpio_put()`, etc.
- The patcher translates Sage function calls to these C wrappers
- Eventually, Sage-native modules will provide idiomatic Sage APIs wrapping these C functions

## Shim Layer

`shims/` provides headers and stubs for POSIX functions unavailable on baremetal:

| File | Purpose |
|------|---------|
| `dlfcn.h` | dlopen/dlclose/dlsym stubs (FFI unavailable on baremetal) |
| `semaphore.h` | sem_init/sem_wait/sem_post stubs |
| `stdatomic.h` | C11 atomics via GCC builtins |
| `pthread.h` | Minimal pthread type/function declarations (avoids newlib TLS init) |
| `stubs.c` | pthread_mutex_*, pthread_create, nanosleep link stubs |

## Architecture Decisions

| Decision | Rationale |
|----------|-----------|
| ARM over RISC-V | USB CDC works reliably on ARM Cortex-M33 |
| Bypass Sage runtime | 10KB smaller, no crashes, simpler |
| `printf` over `fputs` | Pico SDK's printf bypasses newlib fd layer |
| `static inline` bridge | Zero overhead, fully inlined by compiler |
| Post-processing patches | Sage compiler can't be modified (submodule) |
