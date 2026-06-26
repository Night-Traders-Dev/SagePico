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
    │       ├── sage_print_ln → printf           # bypass Sage GC/runtime
    │       ├── remove GC calls                  # not needed for simple programs
    │       ├── strip old FFI/module stubs       # dlopen-based, not on baremetal
    │       ├── inject flash_store.h             # flash-backed K/V store (before bridge)
    │       ├── inject sage_bridge.h             # SageValue wrappers + FFI dispatch + REPL
    │       ├── add pio_bridge.h                 # PIO + WS2812 driver
    │       ├── add pico_port.h + hstx_display.h # C bridges + DVI driver
    │       ├── add RISC-V USB reset + IRQ fix   # __riscv guard
    │       └── add display init + framebuffer console + REPL entry
    │
    ├── CMakeLists.txt            # Step 4: Build system
    │       ├── PICO_BOARD=adafruit_feather_rp2350
    │       ├── PICO_PLATFORM=rp2350-arm-s or rp2350-riscv
    │       └── link pico_stdlib + hardware_adc/pwm/i2c/spi/pio
    │
    └── arm-gcc or riscv32-gcc   # Step 5: Compile + Link
            └── hello.uf2         # Bootable image
```

## Native Bridge Architecture

Three layers of indirection from Sage source to hardware:

### Layer 1: `pico_port.h` — Direct C Bridge
`static inline` wrappers that call Pico SDK directly. Zero SageValue overhead. Used by C patch code (display driver, render loop).

### Layer 2: `sage_bridge.h` — SageValue Bridge
SageValue-wrapped C functions (e.g. `sage_nv_gpio_put(SageValue pin, SageValue val)`). These accept/return SageValue structs and extract C types internally via helpers (`sv_int`, `sv_num`, `sv_str`).

### Layer 3: FFI Dispatch Table
A lookup table mapping `(library_handle, function_name)` → C function pointer.

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
Populated so native Sage `import` statements resolve:
```sage
import pico           # → sage_init_native_module("pico") → dict of function pointers
import time           # → dict with sleep_ms, time_us, etc.
```

## Flash Store (`flash_store.h`)

Log-structured wear-leveled key-value store in upper 2MB of 8MB flash chip.

### Layout
- Offset: 6MB into flash (2MB reserved)
- Each 4KB sector: header (magic, sequence, count, CRC) + entries
- Each entry: key_len (1 byte) + val_len (2 bytes) + flags + key bytes + value bytes

### Wear Leveling
- Monotonic sector sequence numbers
- New writes always go to current sector
- When sector fills: compact active entries to next sector
- Sequence number wrap handled via unsigned comparison

### Persistence
- REPL variables serialized to strings (number/bool/string types)
- Auto-saved on REPL exit (`quit`, Ctrl+C)
- Auto-restored on boot via `flash_store_keys` + `flash_store_get`

## Framebuffer Console (`hstx_display.h`)

Renders text directly to the 640x400 DVI framebuffer.

### Font
8x8 pixel bitmap font, 95 printable ASCII characters (32-126). Each character is 8 bytes where each byte is a row, MSB = top row, bit 7 = leftmost pixel. Approximates the classic PC BIOS font.

### Console API
- `con_putchar_raw(c)` — render single character with newline/backspace handling
- `con_puts(s)` — render string
- `con_printf(fmt, ...)` — render formatted string via `vsnprintf`
- `con_clear()` — clear console
- `con_setpos(x, y)` — set cursor position
- `con_scroll()` — scroll up one line

### Text Layout
80 columns × 50 rows on 640×400 framebuffer. Uses palette index 1 (white) for foreground, 0 (black) for background.

## PIO Bridge (`pio_bridge.h`)

Exposes RP2350 PIO state machines through simple C functions.

### PIO Functions
- `sage_pio_claim(pio_idx, sm)` / `sage_pio_unclaim()`
- `sage_pio_sm_set_pins(pio_idx, sm, base, count, out)`
- `sage_pio_sm_set_clkdiv(pio_idx, sm, div)`
- `sage_pio_sm_put(pio_idx, sm, data)` / `sage_pio_sm_get()`
- `sage_pio_sm_tx_full()` / `sage_pio_sm_rx_empty()`
- `sage_pio_sm_set_enabled(pio_idx, sm, enabled)`
- `sage_pio_sm_restart()` / `sage_pio_sm_clear_fifos()`

### WS2812 Driver
Built-in PIO program for WS2812/NeoPixel LED strips:
- 8 MHz clock (125ns per cycle)
- Sideset pin for precise timing
- 24-bit GRB color format
- `sage_ws2812_init(gpio, pio_idx)` → SM index
- `sage_ws2812_put(pio_idx, sm, r, g, b)`
- `sage_ws2812_write(pio_idx, sm, data, count)` — bulk write

## RISC-V USB Fix

The RP2350 RISC-V (Hazard3) uses a different interrupt controller than ARM (NVIC). The fix:

1. **Explicit USBCTRL reset** before `stdio_init_all()`:
   ```c
   #ifdef __riscv
   reset_block(RESETS_RESET_USBCTRL_BITS);
   unreset_block_wait(RESETS_RESET_USBCTRL_BITS);
   #endif
   ```

2. **IRQ priority after shared handler registration**:
   ```c
   #ifdef __riscv
   irq_set_priority(USBCTRL_IRQ, 0x00);
   irq_set_enabled(USBCTRL_IRQ, true);
   #endif
   ```

The root cause is that Hazard3's custom IRQ controller (MEIEA/MEIPRA CSRs) has different timing behavior than ARM NVIC — pending IRQs may be lost if USB hardware asserts an interrupt before the global MEIE/MSTATUS.MIE bits are fully configured.

## On-Device REPL

The `sage_bridge.h` REPL uses a recursive-descent expression parser supporting:
- Numeric literals (int/float), string literals (`"..."`)
- Binary operators: `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`
- Variables: `let x = expr`, subsequent `x` lookups from dict
- Function calls: `name(arg1, arg2, ...)` dispatched through FFI table
- Array literals: `[a, b, c]`
- Parenthesized expressions

Input handled via `getchar_timeout_us(100ms)` for non-blocking reads with LED heartbeat.

## HSTX DVI Display Driver

Key design decisions for the hardware TMDS encoder:

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

ROT values right-rotate the shifter word to align each channel's byte to bits 7:0.

### Pinout
All 8 HSTX-capable GPIOs (12-19) driven as differential pairs:
- GPIO 12/13: Lane 2 Blue (output bits 0,1)
- GPIO 14/15: Clock (output bits 2,3)
- GPIO 16/17: Lane 1 Green (output bits 4,5)
- GPIO 18/19: Lane 0 Red (output bits 6,7)

## Why the Sage Runtime Crashes on Baremetal

Three root causes:

### 1. `fputs`/`fputc` segfault
The generated C uses `fputs()`/`fputc()` which go through newlib's fd layer (`_write` syscall). Not wired on baremetal Pico.

**Fix**: `patch_stdio.py` replaces all with `printf` equivalents.

### 2. `exit()` halts the CPU
`_exit` on baremetal executes `bkpt` and infinite loops.

**Fix**: `patch_stdio.py` replaces `exit(1)` with `while(1) { tight_loop_contents(); }`.

### 3. Full Sage GC/Runtime link
Linking the Sage runtime pulls in `_sbrk`, `exit`, `pthread_self` — interact poorly with baremetal startup.

**Fix**: With the native bridge injected, Sage types remain available. GC calls stripped for programs that don't need them.

## Shim Layer

`shims/` provides POSIX stubs unavailable on baremetal:

| File | Purpose |
|------|---------|
| `dlfcn.h` | dlopen/dlclose/dlsym stubs |
| `semaphore.h` | sem_init/sem_wait/sem_post stubs |
| `stdatomic.h` | C11 atomics via GCC builtins |
| `pthread.h` | Minimal pthread declarations (avoids newlib TLS init) |
| `stubs.c` | pthread_mutex_*, pthread_create, nanosleep link stubs |

## Architecture Decisions

| Decision | Rationale |
|----------|-----------|
| ARM over RISC-V | USB CDC works reliably on Cortex-M33; RISC-V fix applied but untested |
| `printf` over `fputs` | Pico SDK's printf bypasses newlib fd layer |
| `static inline` C bridge | Zero overhead, fully inlined by compiler |
| Post-processing patches | Sage compiler can't be modified (submodule) |
| FFI dispatch table | Sage code calls any pico_port function without compiler changes |
| Dual bridge (direct + SageValue) | C code uses `pico_port.h`; Sage code uses `sage_bridge.h` through FFI |
| Command expander opcode at [15:12] | Matches pico-examples reference, not CSR [31:28] which is CLKDIV |
| Log-structured flash store | Wear leveling via monotonic sequence numbers, compaction on fill |
| 8x8 framebuffer font | Compact (760 bytes for 95 chars), readable on 640x400 display |
| REPL on both DVI + serial | Self-contained device with HDMI display; USB serial for convenience |
