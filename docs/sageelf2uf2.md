# sageelf2uf2 — Pure Sage ELF→UF2 Converter

Converts ARM/RISC-V ELF binaries to UF2 firmware images for RP2040/RP2350. Written entirely in Sage — zero C dependencies.

## Usage

```bash
sage src/pico/sage/elf2uf2.sage input.elf [-o output.uf2]
```

## How It Works

### 1. Parse ELF Header

Reads the 32-bit ELF header to locate program headers:

```
Offset  Size  Field
0x00    4     Magic (0x7f 'E' 'L' 'F')
0x10    2     Type (2 = executable)
0x12    2     Machine (0x28 = ARM, 0xF3 = RISC-V)
0x1C    4     Program header offset (e_phoff)
0x2C    2     Program header entry size (e_phentsize)
0x30    2     Program header count (e_phnum)
```

### 2. Extract PT_LOAD Segments

Each program header describes a memory segment:

```
Offset  Size  Field
0x00    4     Type (1 = PT_LOAD)
0x04    4     Offset in file
0x08    4     Virtual address
0x10    4     File size
```

Only `PT_LOAD` segments with non-zero file size are extracted.

### 3. Pack into UF2 Blocks

Each 256-byte chunk of payload becomes a 512-byte UF2 block:

```
Offset  Size  Value
0x00    4     0x0A324655  (magic start 0)
0x04    4     0x9E5D5157  (magic start 1)
0x08    4     0x00002000  (flags: familyID present)
0x0C    4     address     (flash target address)
0x10    4     size        (256 bytes)
0x14    4     block_no    (sequential block number)
0x18    4     num_blocks  (total blocks)
0x1C    4     0xe48bff59  (family ID: RP2350 ARM)
0x20    256   data        (payload, zero-padded)
0x1DC   4     0x0AB16F30  (magic end)
```

### 4. Write UF2 File

Output is written block-by-block in sequential order. Each block is exactly 512 bytes.

## ELF Support

| Architecture | Machine ID | Status |
|-------------|------------|--------|
| ARM (Cortex-M) | 0x28 | Supported |
| RISC-V (Hazard3) | 0xF3 | Supported |

## Example

```bash
# Convert ARM ELF to UF2
sage sageelf2uf2.sage build/hello.elf -o build/hello.uf2

# Check output
ls -lh build/hello.uf2
# → 164K
```

## Integration

Used standalone on the desktop. Not part of the on-device firmware (requires ELF parsing).

## Limitations

- 32-bit ELF only (no 64-bit support)
- Single binary output (no combined multi-core images)
- Little-endian only (matches RP2350)
- No checksum verification
- Maximum file size limited by available host memory
