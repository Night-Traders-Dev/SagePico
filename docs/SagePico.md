# SagePico — Comprehensive Project Reference

**Version:** 2.0  
**Target:** Adafruit Feather RP2350 (Cortex-M33 ARM / Hazard3 RISC-V)  
**Language:** Sage (transpiled to C via `--emit-pico-c`), with C bridging headers  
**Repository:** [Night-Traders-Dev/SagePico](https://github.com/Night-Traders-Dev/SagePico)

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Build Pipeline](#2-build-pipeline)
3. [Architecture](#3-architecture)
4. [Source Inventory](#4-source-inventory)
5. [Peripheral Porting Status](#5-peripheral-porting-status)
6. [FFI Dispatch System](#6-ffi-dispatch-system)
7. [On-Device REPL & Shell](#7-on-device-repl--shell)
8. [HSTX DVI Display Driver](#8-hstx-dvi-display-driver)
9. [Graphics Virtual Machine](#9-graphics-virtual-machine)
10. [Flash Storage](#10-flash-storage)
11. [PIO & DMA Bridges](#11-pio--dma-bridges)
12. [sagecom Serial Terminal](#12-sagecom-serial-terminal)
13. [SageLang Compiler Modifications](#13-sagelang-compiler-modifications)
14. [What Remains C vs What is Ported to Sage](#14-what-remains-c-vs-what-is-ported-to-sage)
15. [Pico-SDK Libraries — Remaining Porting Work](#15-pico-sdk-libraries--remaining-porting-work)
16. [Known Issues & Limitations](#16-known-issues--limitations)
17. [Build Sizes](#17-build-sizes)

---

## 1. Project Overview

SagePico compiles Sage source code into bootable UF2 firmware for the Adafruit Feather RP2350. The firmware includes:

- **On-device Sage REPL** with expression parser, arithmetic, variables, multi-line input
- **Command shell** with `help`, `version`, `free`, `uptime`, `led`, `reboot`
- **HSTX DVI display** driving 1280x800 over HDMI via hardware TMDS encoder
- **Framebuffer console** (80x50 text, 8x8 bitmap font) mirrored to DVI display
- **Flash-backed persistence** — variables and programs survive power cycles
- **37 FFI functions** exposing GPIO, UART, ADC, PWM, I2C, SPI, PIO, DMA, and GFX VM
- **Graphics Virtual Machine** — RISC-V RV32I bytecode interpreter with custom graphics extensions
- **sagecom** — dedicated serial terminal written in Sage for host↔Feather interaction

### Architectural Support

| Arch | Core | Status | USB CDC | REPL | DVI | Size |
|------|------|--------|---------|------|-----|------|
| ARM | Cortex-M33 | **Production** | Working | Working | Working | 87K text, 164K UF2 |
| RISC-V | Hazard3 | **Production** | Working | Working | Working | 208K UF2 |

---

## 2. Build Pipeline

```
src/hello.sage
    │
    ├── sage --emit-pico-c          # Sage → C transpilation
    │   └── hello.c                  # Full Sage runtime (~1800 lines)
    │
    ├── sed (include patching)       # dlfcn.h, pthread.h → shims
    ├── patch_stdio.py               # fputs/fputc → printf, exit → while(1)
    ├── patch_main.py                # Inject bridges, strip FFI stubs, arch init, REPL
    │   ├── Include: pico_port.h, hstx_display.h, pio_bridge.h, dma_bridge.h, init.h
    │   ├── Strip: sage_ffi_open/ffi_call/ffi_call_full (all SDK versions v3.8.7-v3.9.2)
    │   ├── Inject: flash_store.h, rvvm.h, gfx_vm.h, sage_bridge.h (in order)
    │   ├── Init: sage_arch_pre_init() → stdio_init_all() → sage_arch_init() → sage_arch_post_init()
    │   ├── Display: display_init() → con_clear() → color bars → sleep_ms(3000) → sage_repl()
    │   └── Render: inject HSTX scanline loop after sage_repl() exit (targeted, not regex)
    │
    ├── CMakeLists.txt               # Generated dynamically by build.sh
    │   ├── PICO_BOARD=adafruit_feather_rp2350
    │   ├── PICO_PLATFORM=rp2350-arm-s | rp2350-riscv
    │   ├── PICO_STDIO_DEFAULT_CRLF=0 (bare \n, no \r\n translation)
    │   ├── Link: pico_stdlib hw_adc hw_pwm hw_i2c hw_spi hw_pio hw_dma
    │   └── Include: src/pico, src/{arm|riscv}, shims
    │
    └── arm-none-eabi-gcc | riscv32-unknown-elf-gcc
        └── hello.uf2                # Bootable firmware image
```

**Dual-path**: `build.sh arm` or `build.sh riscv` selects target. Architecture-specific `init.h` is resolved via `#include "init.h"` through the include path `src/{ARCH_DIR}/`.

---

## 3. Architecture

### Native Bridge Layers

```
┌─────────────────────────────────────────────────────────┐
│  Sage Source (.sage)                                    │
│  ffi_open("pico") → ffi_call(pico, "gpio_put", [7,1])  │
└──────────────────────┬──────────────────────────────────┘
                       │ Generated C: sage_ffi_call(handle, name, ret_type, args)
┌──────────────────────▼──────────────────────────────────┐
│  FFI Dispatch Table (sage_bridge.h)                     │
│  37 entries: (handle, name) → native wrapper function   │
│  Dispatch by dlsym-style lookup, ported from interpreter│
└──────────────────────┬──────────────────────────────────┘
                       │ Calls sage_nv_* wrappers
┌──────────────────────▼──────────────────────────────────┐
│  SageValue Bridge (sage_bridge.h)                       │
│  sage_nv_gpio_put(SageValue pin, SageValue val)         │
│  Extracts int/double/string from SageValue via helpers  │
└──────────────────────┬──────────────────────────────────┘
                       │ Calls Pico SDK directly
┌──────────────────────▼──────────────────────────────────┐
│  Pico SDK (hardware_gpio, etc.)                         │
│  gpio_put(pin, value)                                   │
└─────────────────────────────────────────────────────────┘
```

### Memory Layout

| Region | Size | Usage |
|--------|------|-------|
| Flash (XIP) | 8MB total, ~7.2MB free | Firmware (87K text) + flash_store (2MB) + free |
| SRAM | 520KB total | BSS (72K) + stack + heap + framebuffer (256K heap) |
| USB DPRAM | 4KB at 0x50100000 | Hardware USB buffer memory (outside SRAM) |
| Scratch X | 4KB at 0x20080000 | Core 0 stack (PICO_STACK_SIZE default) |
| Scratch Y | 4KB at 0x20081000 | Core 1 stack (unused in single-core mode) |

---

## 4. Source Inventory

### Sage Source (7 files, 739 lines)

| File | Lines | Purpose |
|------|-------|---------|
| `src/hello.sage` | 7 | Firmware entry point: `ffi_open("pico")`, prints boot banner |
| `src/blink.sage` | 19 | GPIO blink demo in pure Sage via FFI |
| `src/sagecom.sage` | 158 | Desktop serial terminal — connects to Feather over USB CDC |
| `src/pico/gfx_vm.sage` | 286 | RISC-V RV32I assembler + Graphics VM frontend API |
| `src/pico/elf2uf2.sage` | 217 | Pure-Sage ELF→UF2 converter for RP2350 |
| `src/pico/gpio.sage` | 22 | Sage GPIO module stub (resolves to C bridge on-device) |

### C Headers (10 files, 2,651 lines)

| File | Lines | Layer | Purpose |
|------|-------|-------|---------|
| `src/pico/sage_bridge.h` | 1,142 | FFI+SageValue | Core bridge: FFI dispatch table (37 entries), SageValue wrappers, module init, REPL parser/evaluator, command shell, flash persistence |
| `src/pico/hstx_display.h` | 334 | C bridge | HSTX DVI driver: TMDS encoder, 640x400 256-color FB, 8x8 font, 80x50 console, GT911 touch |
| `src/pico/rvvm.h` | 326 | C bridge | RV32I interpreter: 32 regs, 64KB RAM, full decode, graphics opcodes |
| `src/pico/flash_store.h` | 300 | C bridge | Log-structured wear-leveled K/V store in upper 2MB flash |
| `src/pico/pio_bridge.h` | 176 | C bridge | PIO state machine + WS2812 NeoPixel driver |
| `src/pico/gfx_vm.h` | 145 | C bridge | GFX VM integration: ties VM to FB, flash storage, MMIO |
| `src/pico/dma_bridge.h` | 102 | C bridge | DMA channel control: claim/config/start/wait/chain, DREQ constants |
| `src/pico/pico_port.h` | 93 | Direct C | `static inline` wrappers for GPIO/UART/Time/ADC/PWM/I2C/SPI |
| `src/arm/init.h` | 33 | Arch init | ARM Cortex-M33: USB reset, GPIO LED, IRQ enable |
| `src/riscv/init.h` | 38 | Arch init | RISC-V Hazard3: USB reset, IRQ priority 0x00 fix |

### C Source (2 files, 184 lines)

| File | Lines | Purpose |
|------|-------|---------|
| `src/pico/gpio_wrap.c` | 43 | Standalone GPIO wrapper (not linked; replaced by sage_bridge.h injection) |
| `tools/sagecom_tty.c` | 141 | `libsagecom_tty.so` — POSIX termios helpers for sagecom |

### Build Scripts (5 files, 404 lines)

| File | Lines | Purpose |
|------|-------|---------|
| `patch_main.py` | 118 | Post-processing: FFI stub stripping, bridge injection, arch init, render loop |
| `build.sh` | 144 | Full build pipeline: Sage→C, patching, CMake, Pico SDK, UF2, SageVM |
| `test.sh` | 55 | Desktop test suite |
| `patch_stdio.py` | 42 | fputs/fputc→printf, exit→while(1) |
| `tools/install_sagecom.sh` | 42 | sagecom launcher installation |

### Shims (7 files, 197 lines)

| File | Purpose |
|------|---------|
| `shims/baremetal_stubs.h` | Comprehensive POSIX stubs |
| `shims/semaphore.h` | Spinlock semaphores |
| `shims/stubs.c` | Link-level pthread/nanosleep |
| `shims/pthread.h` | Minimal pthread declarations |
| `shims/baremetal_time.h` | clock/nanosleep shims |
| `shims/dlfcn.h` | dlopen/dlsym stubs |
| `shims/stdatomic.h` | C11 atomics via GCC builtins |

### Documentation (3 files)

| File | Lines | Purpose |
|------|-------|---------|
| `docs/architecture.md` | 234 | Full architecture reference |
| `docs/graphics_vm.md` | 282 | GFX VM specification |
| `docs/feather_rp2350.md` | 81 | Board pinout and reference |

---

## 5. Peripheral Porting Status

### Fully Ported to Sage (FFI-accessible from `.sage` code)

| Peripheral | FFI Functions | Module |
|------------|---------------|--------|
| **GPIO** | `gpio_init`, `gpio_set_dir`, `gpio_put`, `gpio_get`, `gpio_pull_up`, `gpio_pull_down`, `gpio_set_function` | `"pico"` / `"gpio"` |
| **Time** | `sleep_ms`, `sleep_us`, `time_us`, `time_ms` | `"time"` |
| **UART** | `init`, `putc`, `puts`, `getc`, `readable` | `"uart"` |
| **ADC** | `init`, `gpio_init`, `select`, `read` | `"adc"` |
| **PWM** | `setup`, `duty` | `"pwm"` |
| **PIO** | `pio_claim`, `pio_put`, `pio_get`, `pio_set_pins`, `pio_set_enabled`, `pio_set_clkdiv`, `pio_clear_fifos` | `"pico"` |
| **WS2812** | `ws2812_init`, `ws2812_put` | `"pico"` |
| **DMA** | `dma_claim`, `dma_config`, `dma_start`, `dma_wait`, `dma_busy`, `dma_unclaim` | `"pico"` |
| **Flash** | `flash_save`, `flash_load`, `flash_del`, `flash_keys` | `"pico"` |
| **GFX VM** | `gfx_init`, `gfx_load`, `gfx_run`, `gfx_vblank` | `"pico"` |

### Partially Ported (available in C bridge, FFI stubs)

| Peripheral | Available | Missing |
|------------|-----------|---------|
| **I2C** | `i2c_init` in FFI, `sage_i2c_write`/`sage_i2c_read` in `pico_port.h` | FFI `i2c_write`/`i2c_read` are stubs (unused parameters) |
| **SPI** | `spi_init` in FFI | FFI `spi_xfer` is a stub; `sage_spi_xfer` in `pico_port.h` is fully implemented |

### Available Only as C Bridge (no FFI)

| Peripheral | Used By |
|------------|---------|
| `hardware_clocks` | `hstx_display.h`, `pio_bridge.h` |
| `hardware_vreg` | `hstx_display.h` (252 MHz overclock) |
| `hardware_resets` | Arch init headers, `hstx_display.h` |
| `hardware_irq` | Arch init headers |
| `hardware_watchdog` | REPL `reboot` command |
| `hardware_sync` | `flash_store.h` |
| `hardware_timer` | SDK time functions (wrapped by FFI time functions) |

### Not Yet Ported

See [Section 15](#15-pico-sdk-libraries--remaining-porting-work) for the complete list.

---

## 6. FFI Dispatch System

The FFI dispatch table in `sage_bridge.h` maps `(library_handle, function_name)` pairs to native C function pointers. This system replaces the `dlopen`/`dlsym`-based FFI that doesn't work on baremetal.

### Registration

Functions are registered at compile time in the `sage_ffi_table[]` array:

```c
static const SageFFIEntry sage_ffi_table[] = {
    FFI_ENTRY(FFI_HANDLE_PICO, "gpio_put", sage_ffi_gpio_put_wrap),
    FFI_ENTRY(FFI_HANDLE_TIME, "sleep_ms", sage_ffi_sleep_ms_wrap),
    // ... 37 entries total
};
```

### Dispatch Flow

```
Sage: ffi_call(pico, "gpio_put", [7, 1])
  → Generated C: sage_ffi_call(handle, "gpio_put", "int", args_array)
    → Runtime: dlsym-style lookup in sage_ffi_table[]
      → sage_ffi_gpio_put_wrap(2, [sage_number(7), sage_number(1)])
        → sage_nv_gpio_put(SageValue(7), SageValue(1))
          → gpio_put(7, 1)
```

### Module Init

`sage_init_native_module("pico")` returns a populated Sage dict with all registered functions, enabling `import pico` syntax:

```sage
import pico
pico.gpio_put(25, 1)    # Resolves via module dict
```

---

## 7. On-Device REPL & Shell

### Expression Parser

Recursive-descent parser with precedence climbing supporting:

- **Arithmetic**: `+`, `-`, `*`, `/`, `%` (precedence 1-2)
- **Comparisons**: `==`, `!=`, `<`, `>`, `<=`, `>=` (precedence 0)
- **Literals**: integers, floats, strings (`"..."`), `true`/`false`/`nil`
- **Variables**: `let x = 42` (stored in REPL vars dict, persisted to flash)
- **Arrays**: `[1, 2, 3]`
- **Function calls**: all 37 FFI functions via dispatch table
- **String concat**: `"hello" + " world"`

### Identifiers

Must start with letter or underscore (digits not allowed as first character — prevents `1+1` from being parsed as variable `1` + variable `1`).

### Shell Commands

| Command | Description |
|---------|-------------|
| `help` | Show all shell commands with descriptions |
| `version` | Firmware version (`SagePico v2.0 on RP2350`) |
| `sage` | Re-display REPL banner |
| `free` | Memory stats (8MB flash, 520KB SRAM) |
| `uptime` | Seconds since boot |
| `led on` / `led off` | Toggle GPIO 7 LED |
| `reboot` | Watchdog reboot |
| `reboot --boot` | Reboot to BOOTSEL mode (for firmware update) |
| `quit` / `exit` | Save vars to flash, exit REPL, resume DVI render loop |
| `procs` | List programs stored in flash |
| `save <name>` | Save multi-line input buffer to flash |
| `run <name>` | Load and execute stored program line-by-line |
| `load <name>` | Load stored program into buffer for viewing |
| `edit <name>` | Load stored program and enter edit mode |

### Multi-Line Input

End a line with `:` to enter multi-line accumulation mode. Blank line finishes. Up to 2KB buffer. The `... ` continuation prompt indicates multi-line mode.

### Persistence

REPL variables are serialized to flash on `quit`/`exit`/`Ctrl+C` and restored on next boot. Types supported: number, bool, string. Multi-line programs are stored via `save <name>` and survive power cycles.

---

## 8. HSTX DVI Display Driver

### Hardware Configuration

- **Clock**: System PLL at 252 MHz, HSTX CLKDIV=8 → 31.5 MHz pixel clock
- **Resolution**: 640×400 internal framebuffer, 2× scaled to 1280×800 output
- **Timing**: H_FP=48, H_SYNC=32, H_BP=80, V_FP=3, V_SYNC=6, V_BP=14
- **Color**: 256-entry palette (8bpp indexed), 16-bit RGB565 entries
- **Encoder**: Hardware TMDS command expander (opcode `0x2 << 12`, two-word FIFO push)

### Pinout (Differential Pairs)

| GPIO | Lane | Output Bit | Signal |
|------|------|------------|--------|
| 12/13 | Lane 2 (Blue) | 0,1 | D0+/D0- |
| 14/15 | Clock | 2,3 | CK+/CK- |
| 16/17 | Lane 1 (Green) | 4,5 | D2+/D2- |
| 18/19 | Lane 0 (Red) | 6,7 | D1+/D1- |

### Framebuffer Console

- 8×8 pixel bitmap font (95 printable ASCII, ~760 bytes)
- 80 columns × 50 rows on 640×400 display
- API: `con_putchar_raw(c)`, `con_puts(s)`, `con_printf(fmt, ...)`, `con_clear()`, `con_scroll()`
- Palette index 1 (white) foreground, 0 (black) background
- Heap-allocated framebuffer (256KB via `malloc`) to avoid BSS bloat

---

## 9. Graphics Virtual Machine

### VM Core (`rvvm.h`)

- 32 × 32-bit general-purpose registers (x0 = zero hardwired)
- 64KB RAM for program code, stack, and data
- Full RV32I subset: R/I/S/B/U/J-type instructions, branches, jumps, loads/stores
- Custom graphics opcode `0x0B` with funct3-encoded extensions

### Graphics Extensions

| Instruction | Description |
|-------------|-------------|
| `FILL color, x, y, w, h` | Fill rectangle using `memset` |
| `BLIT dst_xy, src, w_h` | Copy pixels from RAM to framebuffer |
| `FLIP` | Trigger framebuffer swap via MMIO |
| `VSYNC` | Wait for vertical blank |

### Sage Frontend (`gfx_vm.sage`)

- Complete RV32I assembler: `asm_line()`, `asm_program()`, `words_to_bytes()`
- Register ABI names mapped to numbers (zero, ra, sp, t0-t6, a0-a7, etc.)
- Example programs: `example_clear_screen()`, `example_fill_rect()`, `example_pattern()`
- Programs stored as: 4-byte size + 4-byte entry + bytecode

### Memory Map

| Range | Size | Access | Description |
|-------|------|--------|-------------|
| `0x00000000` | 64KB | R/W | VM RAM |
| `0x10000000` | 256KB | R/W | Framebuffer |
| `0x20000000` | 256B | R/W | MMIO GPU registers |
| `0x30000000` | 1MB | R | Flash (XIP read) |

---

## 10. Flash Storage

### Design

- **Region**: Upper 2MB of 8MB flash (offset 6MB)
- **Sector size**: 4KB with monotonic sequence numbers for wear leveling
- **Structure**: Sector header (magic 0x5346504B, sequence, entry count) + key-value entries
- **Entry format**: 1-byte key_len + 2-byte val_len + 1-byte flags + key bytes + value bytes
- **Compaction**: When sector fills, active entries are copied to next sector (GC)
- **Sequence wrap**: Handled via unsigned comparison

### API

| Function | Description |
|----------|-------------|
| `flash_store_put(key, val, vlen)` | Write key-value pair |
| `flash_store_get(key, val_out, vlen_out)` | Read value by key |
| `flash_store_del(key)` | Delete key |
| `flash_store_keys(keys, count)` | List all keys |
| `flash_persist_repl_vars()` | Serialize REPL variables to flash |

---

## 11. PIO & DMA Bridges

### PIO (`pio_bridge.h`)

- Claim/release state machines on pio0/pio1
- Configure pin mappings, clock dividers (integer+fractional)
- TX/RX FIFO push/pull, status checks
- Load/run PIO programs with offset tracking
- Built-in WS2812 NeoPixel driver (8 MHz, sideset timing, GRB color format)

### DMA (`dma_bridge.h`)

- Claim/unclaim DMA channels
- Configure: source, destination, transfer count, DREQ, data size, increment flags
- Start/wait/busy/abort
- Chain to another channel
- Set read/write/count for ring buffers
- DREQ constants: PIO0_TX0-3, PIO0_RX0-3, PIO1_TX0, PIO1_RX0, HSTX
- Data size constants: DMA_SIZE_8/16/32

---

## 12. sagecom Serial Terminal

### Architecture

```
┌─────────────────────────────────────────────┐
│  sagecom (91K native ELF, Sage-compiled)    │
│  Links: libsagecom_tty.so (C helper)        │
├─────────────────────────────────────────────┤
│  Banner → Drain serial → Main loop          │
│  Serial read (\r stripped in C) → stdout     │
│  Stdin read → forward to serial              │
│  ~. / ~q → exit                              │
│  Ctrl+A C → clear, Ctrl+A R → reset         │
└─────────────────────────────────────────────┘
```

### C Helper (`libsagecom_tty.so`)

- `sagecom_raw_stdin()` — save and set raw mode on stdin
- `sagecom_restore_stdin()` — restore original terminal settings
- `sagecom_open_serial(path, baud)` — open and configure serial port
- `sagecom_serial_read(fd)` — read with `\r` stripping (VTIME=1, 100ms timeout)
- `sagecom_stdin_read()` — non-blocking stdin read
- `sagecom_serial_write(fd, data)` — write to serial port
- `sagecom_serial_drain(fd)` — flush all pending serial input

### Key Design Decisions

- All pointer operations in C (avoids Sage FFI 64→32 bit pointer truncation)
- String-based API (static buffers, returned as `const char*`)
- `ICRNL` set on serial port to translate CR→NL
- Bonus `\r` stripping in C helper for reliability
- `~.` and `~q` escape sequences for exit (works in line-buffered stdin)

---

## 13. SageLang Compiler Modifications

The SageLang compiler (`deps/sagelang`, v3.9.2) was modified to support baremetal FFI:

### Changes in `core/src/c/compiler.c`

| Change | Lines | Description |
|--------|-------|-------------|
| Codegen handlers | 2489+ | Added `ptr_to_int`, `ffi_sym`, `ffi_sym_addr`, `addressof`, `ptr_add`, `sizeof` |
| Real FFI dispatch | 3529-3606 | Replaced stubs with `dlsym`-based dispatch for int/void/double/long/string returns, up to 3 args |
| Runtime functions | 4827-4878 | Added `sage_ptr_to_int`, `sage_ffi_sym`, `sage_ffi_sym_addr`, `sage_addressof`, `sage_ptr_add`, `sage_sizeof` |
| Arg order fix | 2461-2478 | Fixed `ffi_call(lib, name, ret_type, args?)` to match interpreter convention |
| Top-level return | 2921-2932 | `return` in `main()` emits `return 0;` instead of `return sage_nil();` |
| Target guard | 3529-3606 | Desktop gets `dlopen` FFI; Pico targets get stubs (platform bridge overrides) |
| String+int return | 3590-3592 | Added `const char* (*fn)(int)` variant for string return with integer arg |
| Builtin list | 463-472 | Added FFI functions to suggestion list |

---

## 14. What Remains C vs What is Ported to Sage

### Still in C (not accessible from `.sage` code)

| Component | Reason |
|-----------|--------|
| `hstx_display.h` | Register-level HSTX control — too low-level for FFI |
| `rvvm.h` | VM interpreter — performance-critical inner loop |
| `gfx_vm.h` | VM integration — ties C components together |
| `flash_store.h` | Flash programming — requires careful interrupt disable |
| `pio_bridge.h` | PIO state machine control — timing-sensitive |
| `dma_bridge.h` | DMA configuration — register-level |
| `pico_port.h` | Direct C wrappers — zero-overhead for C patch code |
| `sage_bridge.h` | FFI dispatch table — must be in C for SageValue access |
| Arch init headers | Hardware reset/IRQ — called before Sage runtime |
| `sagecom_tty.c` | POSIX termios — must be in C for pointer safety |
| Shims | POSIX stubs — C ABI compatibility |

### Ported to Sage

| Component | How |
|-----------|-----|
| `elf2uf2.sage` | Pure Sage ELF parser + UF2 block builder |
| `gfx_vm.sage` | RISC-V assembler + Graphics VM API in pure Sage |
| `sagecom.sage` | Serial terminal logic in Sage, delegates I/O to C helper |
| `blink.sage` | GPIO blink demo using FFI |
| `hello.sage` | Firmware entry point using FFI |
| All 37 FFI functions | Accessible from Sage via `ffi_call(pico, "name", args)` |

---

## 15. Pico-SDK Libraries — Remaining Porting Work

### High Priority (functional gaps in current system)

| Library | What's Missing | FFI Functions to Add |
|---------|---------------|---------------------|
| `hardware_i2c` | `i2c_write`/`i2c_read` are FFI stubs | `i2c_write(addr, data_bytes)`, `i2c_read(addr, len)` — port `i2c_write_blocking`/`i2c_read_blocking` to SageValue wrappers |
| `hardware_spi` | `spi_xfer` is FFI stub | `spi_transfer(tx_data)` / `spi_transfer_full(tx, rx, len)` — port `spi_write_read_blocking` |
| `hardware_rtc` | Not ported | `rtc_init()`, `rtc_get_datetime()`, `rtc_set_datetime()`, `rtc_set_alarm()` |
| `hardware_watchdog` | `watchdog_reboot` used in C bridge only | `wdg_enable(timeout)`, `wdg_kick()`, `wdg_reboot()`, `wdg_get_scratch(n)` / `wdg_set_scratch(n, val)` |

### Medium Priority (system control)

| Library | FFI Functions to Add |
|---------|---------------------|
| `hardware_clocks` | `clk_get_hz(clock)`, `clk_set_divider(clock, div)`, `clk_gpout_init(pin, src, div)` |
| `hardware_interp` | Interpolator lane control: accumulator, base, mask, shift, pop results |
| `hardware_irq` | `irq_set_enabled(n, bool)`, `irq_get_priority(n)` |
| `hardware_powman` | `sleep_goto_sleep_until(edges)`, `sleep_run_from_xosc()`, dormant mode |
| `hardware_resets` | `reset_block(bits)`, `unreset_block(bits)`, `unreset_block_wait(bits)` |
| `hardware_sha256` | `sha256_start()`, `sha256_update(data, len)`, `sha256_finish(hash_out)` |
| `hardware_sync` | `save_and_disable_interrupts()`, `restore_interrupts()`, `spin_lock_claim(n)` |
| `hardware_timer` | `timer_hw->timelw/timehw` read, `add_alarm_in_ms(ms, callback)` |
| `hardware_vreg` | `vreg_set_voltage(voltage)`, `vreg_get_voltage()` |
| `hardware_xip_cache` | `xip_ctrl_hw->ctrl` read/write for cache enable/disable/flush |
| `hardware_exception` | Exception handler installation |

### Low Priority (specialized/niche)

| Library | Notes |
|---------|-------|
| `hardware_dcp` | Dual-core FIFO + doorbell |
| `hardware_pll` | PLL frequency configuration |
| `hardware_rcp` | RP2350 Realtime Coprocessor |
| `hardware_ticks` | Microsecond tick utilities |
| `hardware_xosc` | Crystal oscillator configuration |
| `hardware_hazard3` | RISC-V Hazard3 custom CSRs |
| `hardware_riscv` | RISC-V platform features |
| `hardware_riscv_platform_timer` | RISC-V mtime/mtimecmp |
| `hardware_sync_spin_lock` | Low-level spin lock primitives |
| `hardware_boot_lock` | Boot sequencing for dual-core |

---

## 16. Known Issues & Limitations

### Critical Bugs Fixed

| Bug | Root Cause | Fix | Commit |
|-----|-----------|-----|--------|
| USB CDC silent (7 duplicate render loops) | `re.sub(r'(\n return 0;\n\}\n)')` matched 7 function endings, injecting infinite printf loops that starved USB IRQ | Targeted `data.replace('sage_repl();')` injection | `b3ac8cf` |
| REPL `1+1` returns nil | `sage_repl_parse_ident` accepted digit-first identifiers, parsing `1+1` as variable `1` + variable `1` | Require identifier first char to be letter or underscore | `9b6d9e4` |
| Output had literal `\n` text | Quadruple-escaped `\\n` in printf strings → literal backslash-n in output | Fixed escape sequences to single `\n` | `3b05b3d` |
| Horizontal cursor jumping (`\r` artifacts) | Pico SDK `PICO_STDIO_DEFAULT_CRLF=1` adds `\r` before every `\n` | Set `PICO_STDIO_DEFAULT_CRLF=0` in CMake | `008a98b` |
| BSS 328K (potential heap collision) | 256K static framebuffer in BSS | Heap-allocate via `malloc` in `display_init()` | `c28bbe9` |
| FFI pointer truncation (SIGSEGV) | 64-bit pointers passed as 32-bit `int` args in FFI | Created `libsagecom_tty.so` C helper, all pointer ops in C | `e6f595e` |

### Known Limitations

- **I2C/SPI FFI stubs**: `i2c_write`/`i2c_read`/`spi_xfer` in FFI dispatch are stubs. Use `pico_port.h` C bridge functions directly for now.
- **Graphics VM**: LINE and SPRITE instructions are reserved but not implemented. FILL/BLIT/FLIP/VSYNC work.
- **REPL**: No control flow (`if`/`while`/`proc` in multi-line). Programs stored as raw text, executed line-by-line.
- **Flash store**: Maximum key size 63 bytes, value size 255 bytes per entry. Compaction may pause briefly.
- **Display**: Framebuffer is 256-color indexed (8bpp), not true color. Color fidelity limited.
- **RISC-V**: Larger binary size (208K vs 164K ARM). Same feature set.
- **sagecom**: Requires `libsagecom_tty.so` installed. Pointer operations must stay in C helper.

---

## 17. Build Sizes

| Target | Text | BSS | Dec | UF2 |
|--------|------|-----|-----|-----|
| ARM Cortex-M33 | 87,576 | 72,804 | 160,380 | 164K |
| RISC-V Hazard3 | ~105,000 | ~72,000 | ~177,000 | 208K |

BSS breakdown (both architectures):
- `disp_fb`: 256KB (heap, not BSS — allocated via `malloc`)
- VM RAM (`rvvm`): 64KB (BSS)
- Sage runtime globals: ~8KB (BSS)
- USB/CDC buffers: ~1KB (BSS)
- Flash store state: negligible

Flash usage (ARM):
- Firmware: ~164K UF2 (~87K actual)
- Flash store region: 2MB reserved (upper flash)
- Free: ~5.8MB available for user programs/data

---

*Generated from SagePico commit `2871659` — SageLang v3.9.2, Feather RP2350 dual-architecture*
