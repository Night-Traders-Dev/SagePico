# elf2uf2.sage — ELF to UF2 converter for RP2350 (SageLang port)
# Usage: call convert(input_path, output_path, family_id)

# UF2 constants
let UF2_PAGE_SIZE = 256
let UF2_MAGIC_START0 = 0x0A324655
let UF2_MAGIC_START1 = 0x9E5D5157
let UF2_MAGIC_END = 0x0AB16F30
let UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000

let RP2350_RISCV_ID = 0xE48BFF57
let RP2350_ARM_ID = 0xE48BFF59

# ELF constants (32-bit)
let E_TYPE_OFF = 16
let E_MACHINE_OFF = 18
let E_ENTRY_OFF = 24
let E_PHOFF_OFF = 28
let E_PHENTSIZE_OFF = 42
let E_PHNUM_OFF = 44
let ELF_MAGIC = 0x464C457F

let PT_LOAD = 1

# Program header fields (32-bit)
let PH_TYPE = 0
let PH_OFFSET = 4
let PH_VADDR = 8
let PH_PADDR = 12
let PH_FILESZ = 16
let PH_MEMSZ = 20

# ---------------------------------------------------------------------------
# Helper: read little-endian u32/u16 from byte array of ints
# ---------------------------------------------------------------------------

proc u32(data, off):
    return int(data[off]) | (int(data[off+1]) << 8) | (int(data[off+2]) << 16) | (int(data[off+3]) << 24)

proc u16(data, off):
    return int(data[off]) | (int(data[off+1]) << 8)

proc w32(buf, off, val):
    let v = int(val)
    buf[off] = v & 0xFF
    buf[off+1] = (v >> 8) & 0xFF
    buf[off+2] = (v >> 16) & 0xFF
    buf[off+3] = (v >> 24) & 0xFF

# ---------------------------------------------------------------------------
# UF2 block builder
# ---------------------------------------------------------------------------

proc make_uf2_block(target, block_no, total, payload, family):
    let block = []
    let i = 0
    while i < 512:
        push(block, 0)
        i = i + 1

    w32(block, 0,  UF2_MAGIC_START0)
    w32(block, 4,  UF2_MAGIC_START1)
    w32(block, 8,  UF2_FLAG_FAMILY_ID_PRESENT)
    w32(block, 12, target)
    w32(block, 16, UF2_PAGE_SIZE)
    w32(block, 20, block_no)
    w32(block, 24, total)
    w32(block, 28, family)

    i = 0
    while i < UF2_PAGE_SIZE and i < len(payload):
        block[32 + i] = payload[i]
        i = i + 1

    w32(block, 508, UF2_MAGIC_END)
    return block

# ---------------------------------------------------------------------------
# Convert ELF bytes → UF2
# ---------------------------------------------------------------------------

proc convert(elf_data, family_id):
    # elf_data: array of ints (bytes)
    # Returns: array of ints (UF2 file bytes), or nil on error

    if len(elf_data) < 52:
        print "elf2uf2: ELF too small"
        return nil

    let magic = u32(elf_data, 0)
    if magic != ELF_MAGIC:
        print "elf2uf2: invalid ELF magic"
        return nil

    let machine = u16(elf_data, 18)
    let entry = u32(elf_data, 24)
    let phoff = u32(elf_data, 28)
    let phentsize = u16(elf_data, 42)
    let phnum = u16(elf_data, 44)

    print "elf2uf2: machine=" + str(machine) + " entry=0x" + str(entry) + " segments=" + str(phnum)

    let blocks = []
    let block_no = 0
    let si = 0

    while si < phnum:
        let po = int(phoff) + si * int(phentsize)
        if po + 32 > len(elf_data):
            si = si + 1
            continue

        let ptype = u32(elf_data, po + PH_TYPE)
        if ptype != PT_LOAD:
            si = si + 1
            continue

        let paddr = u32(elf_data, po + PH_PADDR)
        let filesz = u32(elf_data, po + PH_FILESZ)
        let memsz = u32(elf_data, po + PH_MEMSZ)
        let fileoff = u32(elf_data, po + PH_OFFSET)

        if memsz == 0:
            si = si + 1
            continue

        let size = filesz
        if size == 0:
            si = si + 1
            continue

        let offset = fileoff
        let addr = paddr
        let remaining = size

        while remaining > 0:
            let page_base = addr - (addr % UF2_PAGE_SIZE)
            let page_off = addr % UF2_PAGE_SIZE
            let chunk = remaining
            if chunk > UF2_PAGE_SIZE - page_off:
                chunk = UF2_PAGE_SIZE - page_off

            # Build 256-byte payload
            let payload = []
            let j = 0
            while j < 256:
                push(payload, 0)
                j = j + 1

            j = 0
            while j < chunk and (offset + j) < len(elf_data):
                payload[page_off + j] = int(elf_data[offset + j])
                j = j + 1

            push(blocks, make_uf2_block(page_base, block_no, 0, payload, family_id))
            block_no = block_no + 1
            addr = addr + chunk
            offset = offset + chunk
            remaining = remaining - chunk

        si = si + 1

    if len(blocks) == 0:
        print "elf2uf2: no loadable segments found"
        return nil

    # Update block count in all blocks
    let i = 0
    while i < len(blocks):
        w32(blocks[i], 24, len(blocks))
        i = i + 1

    # Flatten to byte array
    let out = []
    i = 0
    while i < len(blocks):
        let j = 0
        while j < 512:
            push(out, blocks[i][j])
            j = j + 1
        i = i + 1

    print "elf2uf2: " + str(len(blocks)) + " UF2 blocks, " + str(len(out)) + " bytes"
    return out

# ---------------------------------------------------------------------------
# High-level: file → file
# ---------------------------------------------------------------------------

proc elf_to_uf2(elf_path, uf2_path, family_id):
    # Read ELF
    let raw = io.readbytes(elf_path)
    if raw == nil:
        print "elf2uf2: cannot read " + elf_path
        return false

    # Convert bytes to int array
    let data = []
    let i = 0
    while i < len(raw):
        push(data, int(raw[i]))
        i = i + 1

    let uf2_data = convert(data, family_id)
    if uf2_data == nil:
        return false

    # Convert int array to string for io.writefile
    let s = ""
    i = 0
    while i < len(uf2_data):
        s = s + chr(uf2_data[i])
        i = i + 1

    io.writefile(uf2_path, s)
    print "elf2uf2: wrote " + uf2_path
    return true
