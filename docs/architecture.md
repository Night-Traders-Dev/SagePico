# SagePico Architecture

## Build Pipeline

```
src/hello.sage                    # Sage source
    │
    ├── sage --emit-pico-c        # Step 1: Generate C
    │       └── hello.c           # Full Sage runtime + program C code
    │
    ├── patch_stdio.py            # Step 2: Fix baremetal stdio
    │       ├── fputs → printf    #   newlib fd layer crashes
    │       ├── fputc → printf    #   same issue
    │       └── exit → while(1)   #   exit() halts CPU
    │
    ├── patch_main.py             # Step 3: Inject native bridge + drivers
    │       ├── sage_print_ln → printf        # bypass Sage GC/runtime
    │       ├── remove GC calls               # not needed for simple programs
    │       ├── strip old FFI/module stubs    # dlopen-based, not on baremetal
    │       ├── inject sage_bridge.h          # SageValue wrappers + FFI dispatch
    │       ├── add #include "pico_port.h"    # direct C bridge
    │       ├── add #include "hstx_display.h" # DVI display driver
    │       └── add display init + scanline render loop
    │
    ├── CMakeLists.txt            # Step 4: Build system
    │       ├── PICO_BOARD=adafruit_feather_rp2350
    │       ├── PICO_PLATFORM=rp2350-arm-s or rp2350-riscv
    │       └── link pico_stdlib + hardware_adc/pwm/i2c/spi
    │
    └── arm-gcc or riscv32-gcc   # Step 5: Compile + Link
            └── hello.uf2         # Bootable image
```

## Native Bridge Architecture

The native bridge uses three layers of indirection:

### Layer 1: `pico_port.h` — Direct C Bridge
`static inline` wrappers that call Pico SDK directly. Zero SageValue overhead. Used by C patch code (display driver, render loop).

### Layer 2: `sage_bridge.h` — SageValue Bridge
SageValue-wrapped C functions (e.g. `sage_nv_gpio_put(SageValue pin, SageValue val)`). These accept/return SageValue structs and extract C types internally via helper functions (`sv_int`, `sv_num`, `sv_str`).

### Layer 3: FFI Dispatch Table
A lookup table mapping `(library_handle, function_name)` → C function pointer. `sage_ffi_open("pico")` returns a magic handle. `sage_ffi_call(handle, name, args_array)` looks up the function and dispatches.

```
Sage source:                    Generated C:                     Runtime:
────────────                    ────────────                     ────────
ffi_open("pico")         →      sage_ffi_open("pico")      →     returns CLIB handle
ffi_call(pico, "gpio_put",
         [7, 1])          →      sage_ffi_call(handle,      →     FFI table lookup
                                    "gpio_put", [7,1])            → sage_ffi_gpio_put_wrap
                                                                   → sage_nv_gpio_put
                                                                     → gpio_put(7,1)
```

### Module Init (`sage_init_native_module`)
Also populated so native Sage `import` statements work:
```sage
import pico           # → sage_init_native_module("pico") → dict of function pointers
import time           # → dict with sleep_ms, time_us, etc.
```

## HSTX DVI Display Driver

`src/pico/hstx_display.h` is a hardware TMDS encoder driver for the RP2350's HSTX peripheral. Key design decisions:

### Command Expander
The HSTX command expander processes 32-bit FIFO words. The opcode sits at bits [15:12]:
- `0x0`: RAW pass-through
- `0x1`: RAW_REPEAT
- `0x2`: TMDS — routes data through the hardware TMDS encoder
- `0x3`: TMDS_REPEAT
- `0xF`: NOP

Each pixel push sends two words: a command word `(0x2 << 12) | 1` then a data word `B<<16|G<<8|R`.

### expand_tmds Register
Configures how color bits map to TMDS lanes. For RGB888 packed as `0x00BBGGRR`:
- L0 (Red): NBITS=7 (8 bits), ROT=0
- L1 (Green): NBITS=7 (8 bits), ROT=8
- L2 (Blue): NBITS=7 (8 bits), ROT=16

The ROT values right-rotate the shifter word to align each channel's byte to bits 7:0.

### Pinout
All 8 HSTX-capable GPIOs (12-19) are driven as differential pairs:
- GPIO 12/13: Lane 2 Blue (output bits 0,1)
- GPIO 14/15: Clock (output bits 2,3)
- GPIO 16/17: Lane 1 Green (output bits 4,5)
- GPIO 18/19: Lane 0 Red (output bits 6,7)

## Why the Sage Runtime Crashes on Baremetal

Three root causes found through extensive bisecting:

### 1. `fputs`/`fputc` segfault
The generated C uses `fputs(value, stdout)` for printing strings and `fputc(char, stdout)` for characters. These go through newlib's file descriptor layer (`_write` syscall) which isn't properly wired on baremetal Pico. The Pico SDK provides its own `printf` that bypasses this layer.

**Fix**: `patch_stdio.py` replaces all `fputs`/`fputc` with `printf` equivalents.

### 2. `exit()` halts the CPU
The Sage runtime's `sage_fail` calls `exit(1)`. newlib's `_exit` implementation on baremetal executes a `bkpt` (breakpoint) instruction and infinite loops.

**Fix**: `patch_stdio.py` replaces `exit(1)` with `while(1) { tight_loop_contents(); }`.

### 3. Full Sage GC/Runtime link
When any Sage function is called from `main()`, the linker pulls in the full Sage runtime (GC, type system, native functions). This drags in `_sbrk`, `exit`, `pthread_self`, and other newlib stubs that interact poorly with the Pico's baremetal C runtime.

**Fix**: With the native bridge injected, the Sage runtime can now stay linked — the bridge functions use Sage types natively. GC calls are still stripped for programs that don't need them, but the type system remains available.

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
| `printf` over `fputs` | Pico SDK's printf bypasses newlib fd layer |
| `static inline` C bridge | Zero overhead, fully inlined by compiler |
| Post-processing patches | Sage compiler can't be modified (submodule) |
| FFI dispatch table | Allows Sage code to call any pico_port function without compiler changes |
| Dual bridge (direct + SageValue) | C code uses `pico_port.h` for speed; Sage code uses `sage_bridge.h` through FFI |
| Command expander opcode at [15:12] | Matches pico-examples reference, not CSR [31:28] which is CLKDIV |
