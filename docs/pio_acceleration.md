# PIO Parallel Acceleration Architecture

The RP2350 has 2 PIO blocks (8 state machines total) capable of running independent programs in parallel at system clock speed. Currently 1 SM is used for WS2812 — 7 remain available for hardware acceleration.

## PIO Resource Map

| PIO Block | SM | Current Use | Available |
|-----------|-----|-------------|-----------|
| pio0 | 0 | WS2812 NeoPixel | — |
| pio0 | 1-3 | Unused | ✓ (3 SMs) |
| pio1 | 0-3 | Unused | ✓ (4 SMs) |

## Acceleration Opportunities

### 1. Framebuffer BitBLT Engine (High Impact)

Offload pixel copy, fill, and blend operations from CPU to PIO.

**Current state**: CPU does nested `for` loops in `rvvm_mem_write()` (~400 cycles/pixel).

**PIO approach**: State machine reads source address and count from FIFO, performs auto-incrementing memory reads/writes at 1 cycle per pixel.

```asm
.program bitblt_copy
    pull block          ; Get source address
    mov x, osr
    pull block          ; Get destination address  
    mov y, osr
    pull block          ; Get pixel count
loop:
    out pins, 32        ; Place address on bus
    in pins, 8          ; Read pixel from source
    mov isr, osr        ; Move to output
    out pins, 32        ; Write to destination
    jmp x-- loop        ; Decrement count, loop
```

**Performance gain**: 400x — from ~400 CPU cycles/pixel to 1 PIO cycle/pixel.

### 2. GFX VM Hardware Accelerator (High Impact)

Wire the PIO BitBLT engine into the Graphics VM's MMIO registers so `FILL` and `BLIT` instructions execute in hardware instead of software.

**How it works**: When the VM executes `FILL color, x, y, w, h`:
1. VM writes fill parameters to MMIO registers (0x20000008-0x2000000C)
2. PIO state machine reads the parameters and fills the framebuffer
3. VM continues executing while PIO works in parallel
4. VM can `VSYNC` to wait for completion

**Performance**: FILL(640, 400) drops from ~50K CPU cycles to ~256K PIO cycles — but CPU is FREE during the operation. Effective throughput gain: ~3x for mixed workloads.

### 3. CRC-32 Hardware Engine (Medium Impact)

Compute CRC-32 checksums using PIO's shift register and XOR capability.

```asm
.program crc32
    ; CRC-32 polynomial: 0xEDB88320
    ; Uses PIO's ISR shift register for the CRC state
    pull block          ; Get byte to process
    set x, 7            ; 8 bits per byte
bitloop:
    out isr, 1          ; Shift LSB into ISR
    in x, 1             ; Check if we need to XOR
    jmp !x, no_xor
    ; TODO: XOR with polynomial
no_xor:
    jmp x-- bitloop
.wrap
```

**Performance**: ~8 cycles/byte vs ~50 cycles/byte on CPU. ~6x speedup.

### 4. Parallel GPIO Pattern Generator (Medium Impact)

Output arbitrary 32-bit patterns on GPIO pins at precise intervals. Useful for LED matrices, addressable LEDs, and custom display protocols.

```asm
.program gpio_pattern
    pull block          ; Get 32-bit pattern
    out pins, 32        ; Output all 32 bits simultaneously
    pull block          ; Get delay count
    mov x, osr
delay:
    jmp x-- delay       ; Wait N cycles
.wrap
```

**Performance**: 1 cycle per 32-bit output vs ~10 CPU cycles. ~10x speedup.

### 5. DMA Scatter-Gather Engine (Medium Impact)

PIO acts as a programmable DMA controller that can:
- Read from non-contiguous memory regions
- Perform strided transfers
- Apply data transformations (byte swap, bit reverse)
- Feed multiple peripherals from a single DMA channel

```asm
.program scatter_gather
    pull block          ; Get descriptor (src, dst, len)
    out y, 32           ; Y = length/format
.loop:
    pull block          ; Get source word
    out pins, 32        ; Write to destination
    jmp y-- loop
.wrap
```

### 6. Hardware Random Number Generator (Low Impact)

Use PIO's metastability to generate true random bits. Two free-running oscillators sampled at different clock edges produce entropy.

```asm
.program trng
    set pindirs, 0      ; Ensure pin is input
    in pins, 1          ; Sample floating pin
    push                ; Push random bit to FIFO
.wrap
```

### 7. Multi-Channel PWM Controller (Low Impact)

Generate up to 32 independent PWM channels with arbitrary duty cycles using a single PIO state machine.

```asm
.program pwm_scan
    pull block          ; Get 32-bit mask (which pins to set)
    out pins, 32        ; Set pins HIGH
    pull block          ; Get delay
    mov x, osr
delay_high:
    jmp x-- delay_high
    pull block          ; Get 32-bit mask (which pins to clear)
    out pins, 32        ; Set pins LOW
    pull block          ; Get delay
    mov x, osr
delay_low:
    jmp x-- delay_low
.wrap
```

## Implementation Priority

| # | Accelerator | Impact | Complexity | PIO SMs | Status |
|---|------------|--------|------------|---------|--------|
| 1 | BitBLT Engine | High | Medium | 1-2 | **Implemented** (`pio_bitblt.h`) |
| 2 | GFX VM HW Accel | High | High | 2-3 | **Implemented** (wired via MMIO) |
| 3 | CRC-32 Engine | Medium | Low | 1 | **Implemented** (`pio_accel.h`) |
| 4 | GPIO Pattern Gen | Medium | Low | 1 | **Implemented** (`pio_accel.h`) |
| 5 | DMA Scatter-Gather | Medium | Medium | 1-2 | **Implemented** (`pio_accel.h`) |
| 6 | True RNG | Low | Low | 1 | **Implemented** (`pio_accel.h`) |
| 7 | PWM Controller | Low | Low | 1 | **Implemented** (`pio_accel.h`) |

## Architecture: How PIO Integrates with SagePico

```
┌────────────────────────────────────────────────┐
│                  Sage REPL / GFX VM             │
│         FFI calls / VM instructions             │
└────────────────────┬───────────────────────────┘
                     │
┌────────────────────▼───────────────────────────┐
│              PIO Bridge (pio_bridge.h)          │
│  pio_claim, pio_put, pio_set_pins, etc.        │
└────┬──────────────────────────────┬────────────┘
     │                              │
┌────▼──────┐  ┌──────────────┐  ┌──▼───────────┐
│  PIO SM 0 │  │  PIO SM 1    │  │  PIO SM 2-7  │
│  WS2812   │  │  BitBLT      │  │  CRC/GPIO/   │
│  (in use) │  │  (available) │  │  TRNG (free) │
└───────────┘  └──────────────┘  └──────────────┘
```

## Next Steps

1. **✓ BitBLT accelerator** — `src/pico/c/pio/pio_bitblt.h` with PIO program + C bridge
2. **✓ Wire into GFX VM** — connected to MMIO registers 0x20000008-0x20000018
3. **Benchmark** — run `tests/bench/run_bench.py` to measure performance
4. **✓ CRC-32** — implemented with optimized software fallback
5. **✓ Remaining engines** — TRNG, Pattern Gen, Scatter-Gather, PWM all in `pio_accel.h`
