# SagePico Graphics VM Frontend — RISC-V assembler + high-level graphics API
# Runs on-device via FFI. Provides:
#   1. RISC-V assembler (text mnemonics → bytecode)
#   2. VM control (init, load, run, reset)
#   3. High-level graphics primitives (fill, sprite, blit)

let pico = ffi_open("pico")

# ============================================================
# RISC-V RV32I Assembler (subset)
# ============================================================

# Register name → number mapping
let REGS = {"zero":0, "ra":1, "sp":2, "gp":3, "tp":4,
            "t0":5, "t1":6, "t2":7, "s0":8, "fp":8,
            "s1":9, "a0":10, "a1":11, "a2":12, "a3":13,
            "a4":14, "a5":15, "a6":16, "a7":17,
            "s2":18, "s3":19, "s4":20, "s5":21,
            "s6":22, "s7":23, "s8":24, "s9":25,
            "s10":26, "s11":27, "t3":28, "t4":29, "t5":30, "t6":31}

# Opcode map
let OPCODES = {"LUI":0x37, "AUIPC":0x17, "JAL":0x6f, "JALR":0x67,
               "BEQ":0x63, "BNE":0x63, "BLT":0x63, "BGE":0x63,
               "BLTU":0x63, "BGEU":0x63,
               "LB":0x03, "LH":0x03, "LW":0x03, "LBU":0x03, "LHU":0x03,
               "SB":0x23, "SH":0x23, "SW":0x23,
               "ADDI":0x13, "SLTI":0x13, "SLTIU":0x13,
               "XORI":0x13, "ORI":0x13, "ANDI":0x13,
               "SLLI":0x13, "SRLI":0x13, "SRAI":0x13,
               "ADD":0x33, "SUB":0x33, "SLL":0x33, "SLT":0x33, "SLTU":0x33,
               "XOR":0x33, "SRL":0x33, "SRA":0x33, "OR":0x33, "AND":0x33,
               "ECALL":0x0f,
               "FILL":0x0b, "LINE":0x0b, "BLIT":0x0b,
               "SPRITE":0x0b, "FLIP":0x0b, "VSYNC":0x0b}

# Funct3 values
let FUNCT3 = {"BEQ":0, "BNE":1, "BLT":4, "BGE":5, "BLTU":6, "BGEU":7,
              "LB":0, "LH":1, "LW":2, "LBU":4, "LHU":5,
              "SB":0, "SH":1, "SW":2,
              "ADDI":0, "SLTI":2, "SLTIU":3, "XORI":4, "ORI":6, "ANDI":7,
              "SLLI":1, "SRLI":5, "SRAI":5,
              "ADD":0, "SUB":0, "SLL":1, "SLT":2, "SLTU":3,
              "XOR":4, "SRL":5, "SRA":5, "OR":6, "AND":7,
              "FILL":0, "LINE":1, "BLIT":2, "SPRITE":3, "FLIP":4, "VSYNC":5}

# Funct7 values (for R-type differentiation)
let FUNCT7 = {"ADD":0x00, "SUB":0x20, "SLL":0x00, "SLT":0x00, "SLTU":0x00,
              "XOR":0x00, "SRL":0x00, "SRA":0x20, "OR":0x00, "AND":0x00,
              "SRLI":0x00, "SRAI":0x20}

# Immediate fit check
proc imm_fits(val, bits):
    let max = 1
    let i = 0
    while i < bits - 1:
        max = max * 2
        i = i + 1
    return val >= -max and val < max

# Encode R-type: ADD rd, rs1, rs2
proc asm_r_type(op, rd, rs1, rs2):
    let funct7_val = dict_get(FUNCT7, op, 0)
    let funct3_val = dict_get(FUNCT3, op, 0)
    let opcode = dict_get(OPCODES, op, 0)
    return funct7_val*0x2000000 + rs2*0x100000 + rs1*0x8000 + funct3_val*0x1000 + rd*0x80 + opcode

# Encode I-type: ADDI rd, rs1, imm
proc asm_i_type(op, rd, rs1, imm):
    let funct3_val = dict_get(FUNCT3, op, 0)
    let opcode = dict_get(OPCODES, op, 0)
    let imm_encoded = imm & 0xfff
    if imm < 0:
        imm_encoded = imm & 0xfff
    return imm_encoded*0x100000 + rs1*0x8000 + funct3_val*0x1000 + rd*0x80 + opcode

# Encode U-type: LUI rd, imm
proc asm_u_type(op, rd, imm):
    let opcode = dict_get(OPCODES, op, 0)
    return (imm & 0xfffff000) + rd*0x80 + opcode

# Encode S-type: SW rs2, offset(rs1)
proc asm_s_type(op, rs2, rs1, imm):
    let funct3_val = dict_get(FUNCT3, op, 0)
    let opcode = dict_get(OPCODES, op, 0)
    let imm_enc = imm & 0xfff
    let imm_11_5 = (imm_enc >> 5) & 0x7f
    let imm_4_0 = imm_enc & 0x1f
    return imm_11_5*0x2000000 + rs2*0x100000 + rs1*0x8000 + funct3_val*0x1000 + imm_4_0*0x80 + opcode

# Encode B-type: BEQ rs1, rs2, offset
proc asm_b_type(op, rs1, rs2, offset):
    let funct3_val = dict_get(FUNCT3, op, 0)
    let opcode = dict_get(OPCODES, op, 0)
    let off = offset & 0x1ffe
    let bit12 = (off >> 12) & 1
    let bit11 = (off >> 11) & 1
    let bit10_5 = (off >> 5) & 0x3f
    let bit4_1 = (off >> 1) & 0xf
    return bit12*0x80000000 + bit10_5*0x2000000 + rs2*0x100000 + rs1*0x8000 + funct3_val*0x1000 + bit4_1*0x80 + bit11*0x80 + opcode

# Parse one token into reg number or immediate
proc parse_reg(s):
    return dict_get(REGS, s, -1)

# Assemble a single instruction string → 32-bit word
proc asm_line(line):
    # Split by whitespace and commas
    let parts = split(line, " ")
    if len(parts) == 0:
        return nil

    let op = strip(parts[0])
    if op == "" or startswith(op, "#"):
        return nil

    if op == "ECALL":
        return [0x00000073]

    # Parse operands
    let operands = []
    let i = 1
    while i < len(parts):
        let tok = strip(parts[i])
        if tok != "" and tok != ",":
            push(operands, tok)
        i = i + 1

    if op == "LUI" or op == "AUIPC":
        return [asm_u_type(op, parse_reg(operands[0]), tonumber(operands[1]))]

    if op == "JAL" or op == "JALR":
        # JAL rd, offset  or  JALR rd, rs1, offset
        return [asm_i_type(op, parse_reg(operands[0]), parse_reg(operands[1]), tonumber(operands[2]))]

    # R-type: rd, rs1, rs2
    if dict_has(FUNCT7, op):
        return [asm_r_type(op, parse_reg(operands[0]), parse_reg(operands[1]), parse_reg(operands[2]))]

    # I-type: rd, rs1, imm
    if op == "ADDI" or op == "SLTI" or op == "SLTIU" or op == "XORI" or op == "ORI" or op == "ANDI" or op == "SLLI" or op == "SRLI" or op == "SRAI":
        return [asm_i_type(op, parse_reg(operands[0]), parse_reg(operands[1]), tonumber(operands[2]))]

    # Load: rd, offset(rs1) or rd, offset, rs1
    if op == "LB" or op == "LH" or op == "LW" or op == "LBU" or op == "LHU":
        return [asm_i_type(op, parse_reg(operands[0]), parse_reg(operands[1]), tonumber(operands[2]))]

    # Store: rs2, offset(rs1)
    if op == "SB" or op == "SH" or op == "SW":
        return [asm_s_type(op, parse_reg(operands[0]), parse_reg(operands[1]), tonumber(operands[2]))]

    # Branch: rs1, rs2, offset
    if op == "BEQ" or op == "BNE" or op == "BLT" or op == "BGE" or op == "BLTU" or op == "BGEU":
        return [asm_b_type(op, parse_reg(operands[0]), parse_reg(operands[1]), tonumber(operands[2]))]

    # Graphics custom: FILL color_reg, x_reg, y_reg, w, h
    #                 BLIT dst_xy_reg, src_reg, w_h_reg
    if op == "FILL":
        let color = parse_reg(operands[0])
        let x = parse_reg(operands[1])
        let y = parse_reg(operands[2])
        let w = tonumber(operands[3])
        let h = tonumber(operands[4])
        let imm = (h*65536) + w
        return [asm_i_type(op, color, x, imm)]  # rs2=y implicit
    if op == "BLIT":
        let dst_xy = parse_reg(operands[0])
        let src = parse_reg(operands[1])
        let w_h = parse_reg(operands[2])
        return [asm_r_type(op, dst_xy, src, w_h)]
    if op == "FLIP" or op == "VSYNC":
        return [asm_i_type(op, 0, 0, 0)]

    return nil

# Assemble multi-line program → array of 32-bit words
proc asm_program(source):
    let bytes = []
    let lines = split(source, "\n")
    for line in lines:
        let word = asm_line(strip(line))
        if word != nil and len(word) > 0:
            push(bytes, word[0])
    return bytes

# Convert array of words to flat byte array
proc words_to_bytes(words):
    let bytes = []
    for w in words:
        push(bytes, w & 0xff)
        push(bytes, (w >> 8) & 0xff)
        push(bytes, (w >> 16) & 0xff)
        push(bytes, (w >> 24) & 0xff)
    return bytes


# ============================================================
# Graphics VM API (via FFI)
# ============================================================

# Store compiled program in flash
proc gfx_store(name, source):
    let words = asm_program(source)
    let bytecode = words_to_bytes(words)
    let size = len(bytecode)
    if size == 0:
        print "Error: empty program"
        return nil

    # Pack size + bytecode as string (for flash_store)
    let key = "gfx_" + name
    ffi_call(pico, "flash_save", [key, string_from_bytes(bytecode)])
    print "Stored: " + name + " (" + str(size) + " bytes)"
    return key

# Build and return bytecode array (for direct RAM loading)
proc gfx_build(source):
    let words = asm_program(source)
    let bytecode = words_to_bytes(words)
    let size = len(bytecode)
    print "Compiled: " + str(len(words)) + " instructions, " + str(size) + " bytes"
    return bytecode

# Helper: convert byte array to string for flash storage
proc string_from_bytes(bytes):
    let s = ""
    for b in bytes:
        s = s + chr(b)
    return s


# ============================================================
# Example Graphics Programs
# ============================================================

# Clear screen to color
proc example_clear_screen(color_idx):
    return "
        LUI t0, 0x10000      # t0 = framebuffer base
        ADDI t1, zero, " + str(color_idx) + "  # t1 = color
        ADDI t2, zero, 0      # t2 = offset counter
        ADDI t3, zero, 640     # t3 = width
        ADDI t4, zero, 400     # t4 = height
    loop:
        SW t1, 0(t0)          # store color pixel
        ADDI t0, t0, 1        # advance ptr
        ADDI t2, t2, 1        # counter++
        BNE t2, t3, loop      # loop until row done
        ADDI t2, zero, 0      # reset counter
        ADDI t4, t4, -1       # decrement height
        BNE t4, zero, loop    # loop rows
        ECALL
    "

# Draw a filled rectangle using FILL instruction
proc example_fill_rect(x, y, w, h, color):
    return "
        ADDI a0, zero, " + str(color) + "  # a0 = color
        ADDI a1, zero, " + str(x)     + "  # a1 = x
        ADDI a2, zero, " + str(y)     + "  # a2 = y
        FILL a0, a1, a2, " + str(w) + ", " + str(h) + "
        ECALL
    "

# Draw a pattern of colored squares
proc example_pattern():
    return "
        # Draw 7 colored bars across the top
        ADDI a3, zero, 0       # bar index
    bar_loop:
        ADDI a1, zero, 0       # x = bar_index * 91
        ADDI a0, a3, 1         # color = index + 1
        ADDI a2, zero, 0       # y = 0
        FILL a0, a1, a2, 91, 50
        ADDI a3, a3, 1         # next bar
        ADDI t0, zero, 7
        BLT a3, t0, bar_loop
        ECALL
    "

print "Graphics VM Sage Frontend loaded."
print "  gfx_build(source) → bytecode array"
print "  gfx_store(name, source) → flash key"
print "  example_clear_screen(color)"
print "  example_fill_rect(x, y, w, h, color)"
print "  example_pattern()"
