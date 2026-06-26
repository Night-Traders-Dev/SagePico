# SagePico Graphics Virtual Machine

A register-based RISC-V ISA virtual machine for the Feather RP2350. Executes RV32I bytecode with custom graphics extensions, using PIO cores for hardware-accelerated rendering and 1MB of flash for program storage.

## Architecture

```
┌─────────────────────────────────────────────────┐
│              Sage Frontend (gfx_vm.sage)        │
│  RISC-V assembler + high-level graphics API     │
│  asm_line() / asm_program() / words_to_bytes()  │
└────────────────────┬────────────────────────────┘
                     │ ffi_call("flash_save", ...)
┌────────────────────▼────────────────────────────┐
│         Flash Storage (flash_store.h)           │
│  Programs stored as key-value pairs ("gfx_*")   │
│  Up to 255 bytes of bytecode per program        │
└────────────────────┬────────────────────────────┘
                     │ flash_store_get()
┌────────────────────▼────────────────────────────┐
│        Graphics VM (gfx_vm.h)                   │
│  VM lifecycle: init → load → run_frame          │
│  MMIO bridge: GPU registers → framebuffer ops   │
└──────┬─────────────────────┬────────────────────┘
       │                     │
┌──────▼────────┐   ┌───────▼────────────────────┐
│  RISC-V Core  │   │   MMIO GPU Registers        │
│  (rvvm.h)     │   │   0x20000000: VBLANK        │
│  32 registers │   │   0x20000004: FLIP          │
│  RV32I ISA    │   │   0x20000008: PIO_FILL      │
│  + graphics   │   │   0x20000010: DMA_BLIT      │
└──────┬────────┘   └─────────────────────────────┘
       │
┌──────▼──────────────────────────────────────────┐
│              Memory Map (64KB RAM)               │
│  0x00000000 - 0x0000FFFF : VM RAM (code + data)  │
│  0x10000000 - 0x1009FFFF : Framebuffer (640x400) │
│  0x20000000 - 0x200000FF : MMIO registers        │
│  0x30000000 - 0x300FFFFF : Flash (1MB read-only) │
└─────────────────────────────────────────────────┘
```

## Memory Map

| Address Range | Size | Access | Description |
|---------------|------|--------|-------------|
| `0x00000000` - `0x0000FFFF` | 64KB | R/W | VM RAM (program code, stack, data) |
| `0x10000000` - `0x1009FFFF` | 256KB | R/W | Framebuffer (640×400, 1 byte/pixel palette index) |
| `0x20000000` - `0x200000FF` | 256B | R/W | MMIO GPU control registers |
| `0x30000000` - `0x300FFFFF` | 1MB | R | Flash storage (XIP read-only) |

## MMIO GPU Registers

| Address | Size | Access | Name | Description |
|---------|------|--------|------|-------------|
| `0x20000000` | 4 | R | `VBLANK` | 1 = in vertical blanking, 0 = active |
| `0x20000004` | 4 | W | `FLIP` | Write 1 to trigger framebuffer swap |
| `0x20000008` | 4 | W | `PIO_FILL_COLOR` | Set fill color for PIO accelerator |
| `0x2000000C` | 4 | W | `PIO_FILL_RECT` | Write x|y<<16 to trigger PIO fill |
| `0x20000010` | 4 | W | `DMA_BLIT_SRC` | Source address for DMA blit |
| `0x20000014` | 4 | W | `DMA_BLIT_DST` | Dest address in framebuffer |
| `0x20000018` | 4 | W | `DMA_BLIT_SIZE` | Write w|h<<16 to trigger DMA transfer |
| `0x2000001C` | 4 | R | `VM_CYCLES` | Cycles executed this frame |
| `0x20000020` | 4 | R | `VM_STATUS` | 0 = halted, 1 = running |

## Instruction Set

### RV32I Subset (Standard Opcodes)

**R-Type** (opcode `0x33`): `ADD`, `SUB`, `SLL`, `SLT`, `SLTU`, `XOR`, `SRL`, `SRA`, `OR`, `AND`
```
Format: funct7[31:25] | rs2[24:20] | rs1[19:15] | funct3[14:12] | rd[11:7] | 0x33

ADD rd, rs1, rs2    # rd = rs1 + rs2
SUB rd, rs1, rs2    # rd = rs1 - rs2
SLL rd, rs1, rs2    # rd = rs1 << (rs2 & 0x1f)
SLT rd, rs1, rs2    # rd = (rs1 < rs2 signed) ? 1 : 0
SLTU rd, rs1, rs2   # rd = (rs1 < rs2 unsigned) ? 1 : 0
XOR rd, rs1, rs2    # rd = rs1 ^ rs2
SRL rd, rs1, rs2    # rd = rs1 >> (rs2 & 0x1f) logical
SRA rd, rs1, rs2    # rd = rs1 >> (rs2 & 0x1f) arithmetic
OR  rd, rs1, rs2    # rd = rs1 | rs2
AND rd, rs1, rs2    # rd = rs1 & rs2
```

**I-Type** (opcode `0x13`): `ADDI`, `SLLI`, `SLTI`, `SLTIU`, `XORI`, `SRLI`, `SRAI`, `ORI`, `ANDI`
```
Format: imm[31:20] | rs1[19:15] | funct3[14:12] | rd[11:7] | 0x13

ADDI rd, rs1, imm   # rd = rs1 + sign_extend(imm)
SLLI rd, rs1, shamt # rd = rs1 << shamt
ORI  rd, rs1, imm   # rd = rs1 | sign_extend(imm)
ANDI rd, rs1, imm   # rd = rs1 & sign_extend(imm)
```

**Load** (opcode `0x03`): `LB`, `LH`, `LW`, `LBU`, `LHU`
```
Format: imm[31:20] | rs1[19:15] | funct3[14:12] | rd[11:7] | 0x03

LW  rd, offset(rs1)  # rd = mem32[rs1 + offset]
LH  rd, offset(rs1)  # rd = sign_extend(mem16[rs1 + offset])
LB  rd, offset(rs1)  # rd = sign_extend(mem8[rs1 + offset])
```

**Store** (opcode `0x23`): `SB`, `SH`, `SW`
```
Format: imm[11:5] | rs2[24:20] | rs1[19:15] | funct3[14:12] | imm[4:0] | 0x23

SW rs2, offset(rs1)  # mem32[rs1 + offset] = rs2
SH rs2, offset(rs1)  # mem16[rs1 + offset] = rs2
SB rs2, offset(rs1)  # mem8[rs1 + offset] = rs2
```

**Branch** (opcode `0x63`): `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, `BGEU`
```
Format: imm[12|10:5] | rs2[24:20] | rs1[19:15] | funct3[14:12] | imm[4:1|11] | 0x63

BEQ rs1, rs2, offset   # branch if rs1 == rs2
BNE rs1, rs2, offset   # branch if rs1 != rs2
BLT rs1, rs2, offset   # branch if rs1 < rs2 (signed)
```

**Jump** (opcode `0x6F`/`0x67`): `JAL`, `JALR`
```
JAL rd, offset    # rd = pc+4; pc += offset
JALR rd, rs1, imm # rd = pc+4; pc = rs1 + imm
```

**Upper Immediate** (opcode `0x37`/`0x17`): `LUI`, `AUIPC`
```
LUI rd, imm     # rd = imm << 12
AUIPC rd, imm   # rd = pc + (imm << 12)
```

**System** (opcode `0x0F`): `ECALL` — halts VM execution

### Graphics Extensions (opcode `0x0B`)

Custom instructions encoded in the unused custom-0 opcode space, funct3-differentiated:

| Instruction | funct3 | Format | Description |
|-------------|--------|--------|-------------|
| `FILL` | 0 | `FILL color_reg, x_reg, y_reg, w, h` | Fill rectangle with color |
| `LINE` | 1 | `LINE x0, y0, x1, y1, color` | Draw line (reserved) |
| `BLIT` | 2 | `BLIT dst_xy_reg, src_reg, w_h_reg` | Copy pixels from RAM to framebuffer |
| `SPRITE` | 3 | `SPRITE x, y, sprite_id` | Draw sprite (reserved) |
| `FLIP` | 4 | `FLIP` | Trigger framebuffer swap via MMIO |
| `VSYNC` | 5 | `VSYNC` | Wait for vertical blank |

```
FILL a0, a1, a2, 91, 50
  # Fill 91×50 rectangle at (a1, a2) with color a0

BLIT a0, a1, a2
  # a0 = dst_x | (dst_y << 16)
  # a1 = source RAM address
  # a2 = width | (height << 16)
  # Copies w×h pixels from RAM to framebuffer

FLIP
  # Writes 1 to MMIO 0x20000004

VSYNC
  # Spins until MMIO 0x20000000 == 0
```

### Registers

| Register | ABI Name | Use |
|----------|----------|-----|
| x0 | zero | Hardwired zero |
| x1 | ra | Return address |
| x2 | sp | Stack pointer |
| x5-x7 | t0-t2 | Temporaries |
| x8 | s0/fp | Saved register / frame pointer |
| x10-x17 | a0-a7 | Function arguments / return values |
| x28-x31 | t3-t6 | Temporaries |

## Using the Graphics VM

### From the Sage REPL

```bash
# Initialize the VM (binds to current framebuffer)
>>> gfx_init()

# Compile and store a program
>>> let prog = gfx_build("
... ADDI a0, zero, 4       # color = 4 (blue)
... ADDI a1, zero, 100     # x = 100
... ADDI a2, zero, 50      # y = 50
... FILL a0, a1, a2, 200, 100
... ECALL
... ")

# Run the VM for one frame
>>> gfx_run()
```

### From Sage Source

```sage
# gfx_vm.sage provides high-level example programs
print example_pattern()

# Or use the assembler directly
let asm = "
    ADDI a0, zero, 3       # color 3 = green
    ADDI a1, zero, 0       # x = 0
    ADDI a2, zero, 0       # y = 0
    FILL a0, a1, a2, 640, 400
    ECALL
"
let words = asm_program(asm)
let bytes = words_to_bytes(words)
```

### Example: Pattern Generator

```asm
# Draw 7 colored bars across the top of the screen
    ADDI a3, zero, 0       # bar index = 0
bar_loop:
    ADDI a1, zero, 0       # x = bar_index * 91
    ADDI a0, a3, 1         # color = index + 1
    ADDI a2, zero, 0       # y = 0
    FILL a0, a1, a2, 91, 50
    ADDI a3, a3, 1         # next bar
    ADDI t0, zero, 7
    BLT a3, t0, bar_loop
    ECALL
```

## Implementation Notes

### VM Performance
- Each RV32I instruction takes ~10-20 CPU cycles
- Graphics instructions (FILL) use `memset` for hardware acceleration
- BLIT copies pixel-by-pixel; future DMA-accelerated version planned
- 50,000 cycle budget per frame (~2.5ms at 252 MHz)
- VM runs on the ARM/RISC-V CPU, not PIO (PIO reserved for future pixel ops)

### Flash Storage
- Programs are stored via `flash_store_put("gfx_<name>", ...)`
- Each program: 4-byte size + 4-byte entry point + bytecode (max 247 bytes)
- Loaded with `gfx_load("name")` via the FFI
- Survives power cycles (wear-leveled log-structured flash)

### Framebuffer
- The VM's framebuffer IS the HSTX display framebuffer (`disp_fb`)
- Changes made by VM are visible on the DVI output in real-time
- The render loop continues running; VM output appears as an overlay

### Limitations
- Programs max 247 bytes (flash_store value limit)
- No floating-point (RV32F not implemented)
- No interrupts or exceptions (ECALL only halts)
- No MMU or privilege levels
- Single program at a time (no multitasking)

## Files

| File | Purpose |
|------|---------|
| `src/pico/rvvm.h` | RISC-V RV32I interpreter core |
| `src/pico/gfx_vm.h` | Graphics VM integration (flash + MMIO + framebuffer) |
| `src/pico/gfx_vm.sage` | Sage frontend: assembler + high-level API |
| `src/pico/dma_bridge.h` | DMA channel control |
| `src/pico/flash_store.h` | Flash-backed key-value store |
| `src/pico/pio_bridge.h` | PIO state machine + WS2812 driver |
| `src/pico/sage_bridge.h` | FFI dispatch table + native bindings |
| `src/pico/hstx_display.h` | HSTX DVI display + framebuffer console |

## Future Enhancements

- **PIO BitBLT accelerator**: offload pixel copy/fill to PIO state machines
- **DMA-accelerated BLIT**: use DMA channels for fast memory-to-framebuffer transfers
- **Hardware sprites**: sprite rendering via PIO with transparency
- **Floating-point support**: RV32F extension for 3D math
- **Program chaining**: run multiple programs sequentially per frame
- **Sound**: PWM audio output via VM instructions
- **On-device assembler**: compile assembly text to bytecode without host Sage compiler
