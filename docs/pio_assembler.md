# sagepioasm — Pure Sage PIO Assembler

Compiles RP2350 PIO assembly to 16-bit binary instructions. Written entirely in Sage.

## Usage

```bash
sage src/tools/sage/sagepioasm.sage input.pio [--c] [-o output.h]
```

### Options

| Flag | Description |
|------|-------------|
| `--c` | Output as C array (`static const uint16_t pio_program[] = {...}`) |
| `-o FILE` | Write output to file instead of stdout |

## Supported Instructions

All 9 PIO instruction types with full operand support:

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| `JMP` | `jmp [cond,] addr [delay]` | Jump to address, optionally with condition |
| `WAIT` | `wait pol src idx [delay]` | Wait for GPIO/IRQ/pin state |
| `IN` | `in src, bits [delay]` | Shift data from source into ISR |
| `OUT` | `out dst, bits [delay]` | Shift data from OSR to destination |
| `PUSH` | `push [iffull] [block] [delay]` | Push ISR to RX FIFO |
| `PULL` | `pull [ifempty] [block] [delay]` | Pull from TX FIFO to OSR |
| `MOV` | `mov dst, [!~]src [delay]` | Move/transform data between registers |
| `IRQ` | `irq num [mod] [delay]` | Set/clear/wait for IRQ |
| `SET` | `set dst, data [delay]` | Write immediate to destination |
| `NOP` | `nop` | No operation (encodes as `mov y, y`) |

### Conditions (JMP)

`!x`, `x--`, `!y`, `y--`, `x!=y`, `pin`, `!osre` — or omit for unconditional jump.

### Sources/Destinations

`pins`, `x`, `y`, `null`, `pindirs`, `pc`, `isr`, `osr`, `status`, `exec`

### IRQ Modifiers

`set`, `nowait`/`wait`, `clear`/`clr`

### MOV Operations

`!` (invert), `~` (invert), `::` (bit-reverse), `reverse`, `invert`, `bitrev`

## Directives

| Directive | Description |
|-----------|-------------|
| `.program NAME` | Program name (informational) |
| `.side_set N [opt]` | Configure N-bit side-set (optional) |
| `.wrap_target` | Mark wrap target (beginning of loop) |
| `.wrap` | Mark end of wrap region |
| `.define NAME VALUE` | Define a constant |
| `.origin OFFSET` | Set origin offset |

## Labels

Labels are defined with a trailing colon and referenced by name:

```asm
.program blink
    set x, 10         # loop counter
loop:
    set pins, 1       # LED on
    jmp loop          # jump back
```

## Delay and Side-Set

Append delay/side-set in brackets at the end of any instruction:

```asm
    nop [5]           # 5 cycle delay
    out pins, 1 [2]   # 2 cycle delay
    nop side 1 [3]    # side-set bit 1, 3 cycle delay
    jmp loop side 0   # side-set bit 0
```

With `.side_set N`, the side-set bit occupies bit N-1 of the delay field. The maximum delay value depends on the number of side-set bits: `max_delay = 31 >> sideset_count`.

## Examples

### WS2812 NeoPixel Driver

```asm
.program ws2812
.side_set 1

.wrap_target
    out x, 1    side 0 [2]   ; T0H=2 cycles, T1H=4 cycles
    jmp !x, skip side 1 [1] ; branch if bit=0
    jmp next    side 1 [4]   ; T1L=4 cycles
skip:
    nop         side 0 [4]   ; T0L=4 cycles
next:
.wrap
```

Compile:
```bash
sage sagepioasm.sage ws2812.pio --c -o ws2812.h
```

Output:
```c
static const uint16_t pio_program[] = {
    0x6221, 0x1123, 0x1402, 0xa442
};
```

### Blink LED

```asm
.program blink
    set pindirs, 1      ; pin 0 as output
loop:
    set pins, 1         ; LED on
    set x, 31       [7] ; delay ~8 cycles
    set pins, 0         ; LED off
    set x, 31       [7]
    jmp loop
```

### UART TX (8N1)

```asm
.program uart_tx
.side_set 1 opt

    pull block          ; wait for data
    set x, 9            ; 1 start + 8 data bits
    set y, 7            ; bit count
    mov osr, osr    [3] ; prep
bitloop:
    out pins, 1
    jmp x-- bitloop [1]
    nop                 ; stop bit
.wrap
```

## Encoding Notes

PIO instructions are 16-bit little-endian words. The opcode occupies bits 15:13:

| Opcode | Bits 15:13 | Instruction Class |
|--------|------------|-------------------|
| 0 | 000 | JMP |
| 1 | 001 | WAIT |
| 2 | 010 | IN |
| 3 | 011 | OUT |
| 4 | 100 | PUSH/PULL |
| 5 | 101 | MOV |
| 6 | 110 | IRQ |
| 7 | 111 | SET |

PUSH and PULL are differentiated by bit 7 (0 = PUSH, 1 = PULL).

## Integration with SagePico Firmware

The `--c` output can be directly included in SagePico C bridges:

```c
// In your pio_bridge.h or custom firmware:
static const uint16_t my_program[] = {
    // paste --c output here
};
uint offset = pio_add_program(pio0, (const pio_program_t*)my_program);
```

Or use the existing `sagepioasm` tool to generate the header at build time.

## Known Limitations

- Side-set encoding with multi-bit side-sets may need adjustment for exact hardware matching
- `.origin` directive is recognized but padding not implemented
- WAIT instruction index field limited to simple values
- File I/O uses Sage's `io.readfile`/`io.writefile` — supports text mode only
