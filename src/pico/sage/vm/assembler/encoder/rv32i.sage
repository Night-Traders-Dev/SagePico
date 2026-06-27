## encoder/rv32i.sage — Full RV32I instruction encoder
## Encodes R/I/S/B/U/J-type instructions from AST nodes.
## Supports all RV32I base instructions, M-extension, Zicsr, pseudonyms.

class RV32IEncoder:
    proc init(self):
        self.output = []    # list of 32-bit integers
        self.offset = 0     # current byte offset
        self.symbols = SymbolTable()
        self.errors = []

    ## ---- Basic encoders ----
    proc encode_r_type(self, funct7, rs2, rs1, funct3, rd, opcode):
        return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode

    proc encode_i_type(self, imm12, rs1, funct3, rd, opcode):
        return ((imm12 & 0xFFF) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode

    proc encode_i_csr(self, csr12, rs1, funct3, rd, opcode):
        return ((csr12 & 0xFFF) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode

    proc encode_i_shamt(self, shamt, rs1, funct3, rd, opcode):
        return ((shamt & 0x1F) << 20) | ((0x8 if shamt > 31 else 0) << 26) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode

    proc encode_s_type(self, imm12, rs2, rs1, funct3, opcode):
        let imm_11_5 = (imm12 >> 5) & 0x7F
        let imm_4_0  = imm12 & 0x1F
        return (imm_11_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (imm_4_0 << 7) | opcode

    proc encode_b_type(self, imm13, rs2, rs1, funct3, opcode):
        let imm = imm13 & 0x1FFE
        let bit12 = (imm >> 12) & 1
        let bit11 = (imm >> 11) & 1
        let bit10_5 = (imm >> 5) & 0x3F
        let bit4_1  = (imm >> 1) & 0xF
        return (bit12 << 31) | (bit10_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (bit4_1 << 8) | (bit11 << 7) | opcode

    proc encode_u_type(self, imm20, rd, opcode):
        return (imm20 & 0xFFFFF000) | (rd << 7) | opcode

    proc encode_j_type(self, imm21, rd, opcode):
        let imm = imm21 & 0x1FFFFE
        let bit20   = (imm >> 20) & 1
        let bit10_1 = (imm >> 1) & 0x3FF
        let bit11   = (imm >> 11) & 1
        let bit19_12 = (imm >> 12) & 0xFF
        return (bit20 << 31) | (bit10_1 << 21) | (bit11 << 20) | (bit19_12 << 12) | (rd << 7) | opcode

    ## ---- Register resolution ----
    proc reg_num(self, s):
        if dict_has(REGISTERS, s):
            return REGISTERS[s]
        return 0

    ## ---- Resolve label or immediate for branch/jump targets ----
    proc resolve_target(self, op):
        if op.kind == "label":
            return self.symbols.reference(op.value, nil)
        return op.value

    ## ---- Encode an instruction node ----
    proc encode(self, node):
        if node == nil:
            return

        if node.kind == "label":
            self.symbols.define(node.name, self.offset)
            return

        if node.kind != "instruction":
            return

        let m = node.mnemonic
        let o = node.operands

        ## ----- R-type (OP = 0x33, OP-32 = 0x3B on RV64) -----
        if m == "add":              let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "sub":              let inst = self.encode_r_type(0x20, self.reg_num(o[2].value), self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "xor":              let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 4, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "or":               let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 6, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "and":              let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 7, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "sll":              let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 1, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "srl":              let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 5, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "sra":              let inst = self.encode_r_type(0x20, self.reg_num(o[2].value), self.reg_num(o[1].value), 5, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "slt":              let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 2, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "sltu":             let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 3, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- RV32M (OP = 0x33, funct7 = 0x01) -----
        if m == "mul":              let inst = self.encode_r_type(0x01, self.reg_num(o[2].value), self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "mulh":             let inst = self.encode_r_type(0x01, self.reg_num(o[2].value), self.reg_num(o[1].value), 1, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "mulhsu":           let inst = self.encode_r_type(0x01, self.reg_num(o[2].value), self.reg_num(o[1].value), 2, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "mulhu":            let inst = self.encode_r_type(0x01, self.reg_num(o[2].value), self.reg_num(o[1].value), 3, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "div":              let inst = self.encode_r_type(0x01, self.reg_num(o[2].value), self.reg_num(o[1].value), 4, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "divu":             let inst = self.encode_r_type(0x01, self.reg_num(o[2].value), self.reg_num(o[1].value), 5, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "rem":              let inst = self.encode_r_type(0x01, self.reg_num(o[2].value), self.reg_num(o[1].value), 6, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "remu":             let inst = self.encode_r_type(0x01, self.reg_num(o[2].value), self.reg_num(o[1].value), 7, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- I-type ALU (OP-IMM = 0x13) -----
        if m == "addi":             let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "xori":             let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 4, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "ori":              let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 6, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "andi":             let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 7, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "slli":             let inst = self.encode_i_type(o[2].value & 0x1F, self.reg_num(o[1].value), 1, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "srli":             let inst = self.encode_i_type(o[2].value & 0x1F, self.reg_num(o[1].value), 5, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "srai":             let inst = self.encode_i_type((o[2].value & 0x1F) | 0x400, self.reg_num(o[1].value), 5, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "slti":             let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 2, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "sltiu":            let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 3, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- I-type loads (LOAD = 0x03) -----
        if m == "lb":               let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x03); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "lh":               let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 1, self.reg_num(o[0].value), 0x03); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "lw":               let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 2, self.reg_num(o[0].value), 0x03); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "lbu":              let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 4, self.reg_num(o[0].value), 0x03); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "lhu":              let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 5, self.reg_num(o[0].value), 0x03); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- S-type stores (STORE = 0x23) -----
        if m == "sb":               let inst = self.encode_s_type(o[2].value, self.reg_num(o[0].value), self.reg_num(o[1].value), 0, 0x23); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "sh":               let inst = self.encode_s_type(o[2].value, self.reg_num(o[0].value), self.reg_num(o[1].value), 1, 0x23); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "sw":               let inst = self.encode_s_type(o[2].value, self.reg_num(o[0].value), self.reg_num(o[1].value), 2, 0x23); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- B-type branches (BRANCH = 0x63) -----
        if m == "beq":              let inst = self.encode_b_type(self.resolve_target(o[2]) - self.offset, self.reg_num(o[1].value), self.reg_num(o[0].value), 0, 0x63); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "bne":              let inst = self.encode_b_type(self.resolve_target(o[2]) - self.offset, self.reg_num(o[1].value), self.reg_num(o[0].value), 1, 0x63); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "blt":              let inst = self.encode_b_type(self.resolve_target(o[2]) - self.offset, self.reg_num(o[1].value), self.reg_num(o[0].value), 4, 0x63); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "bge":              let inst = self.encode_b_type(self.resolve_target(o[2]) - self.offset, self.reg_num(o[1].value), self.reg_num(o[0].value), 5, 0x63); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "bltu":             let inst = self.encode_b_type(self.resolve_target(o[2]) - self.offset, self.reg_num(o[1].value), self.reg_num(o[0].value), 6, 0x63); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "bgeu":             let inst = self.encode_b_type(self.resolve_target(o[2]) - self.offset, self.reg_num(o[1].value), self.reg_num(o[0].value), 7, 0x63); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- U-type -----
        if m == "lui":              let inst = self.encode_u_type(o[1].value << 12, self.reg_num(o[0].value), 0x37); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "auipc":            let inst = self.encode_u_type(o[1].value << 12, self.reg_num(o[0].value), 0x17); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- J-type -----
        if m == "jal":              let inst = self.encode_j_type(self.resolve_target(o[1]) - self.offset, self.reg_num(o[0].value), 0x6F); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "jalr":             let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x67); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- System (SYSTEM = 0x73) -----
        if m == "ecall":            let inst = self.encode_i_type(0, 0, 0, 0, 0x73); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "ebreak":           let inst = self.encode_i_type(1, 0, 0, 0, 0x73); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "mret":             let inst = self.encode_i_type(0x302, 0, 0, 0, 0x73); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- Fence (MISC-MEM = 0x0F) -----
        if m == "fence":            let inst = self.encode_i_type(0, 0, 0, 0, 0x0F); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- Zicsr -----
        if m == "csrrw":            let inst = self.encode_i_type(o[1].value, self.reg_num(o[2].value), 1, self.reg_num(o[0].value), 0x73); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "csrrs":            let inst = self.encode_i_type(o[1].value, self.reg_num(o[2].value), 2, self.reg_num(o[0].value), 0x73); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "csrrc":            let inst = self.encode_i_type(o[1].value, self.reg_num(o[2].value), 3, self.reg_num(o[0].value), 0x73); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "csrrwi":           let inst = self.encode_i_type(o[1].value, o[2].value, 5, self.reg_num(o[0].value), 0x73); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "csrrsi":           let inst = self.encode_i_type(o[1].value, o[2].value, 6, self.reg_num(o[0].value), 0x73); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "csrrci":           let inst = self.encode_i_type(o[1].value, o[2].value, 7, self.reg_num(o[0].value), 0x73); push(self.output, inst); self.offset = self.offset + 4; return;

        ## ----- Pseudonyms -----
        if m == "nop":              let inst = self.encode_i_type(0, 0, 0, 0, 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "li":               let inst = self.encode_i_type(o[1].value, 0, 0, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "mv":               let inst = self.encode_i_type(0, self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "not":              let inst = self.encode_i_type(-1, self.reg_num(o[1].value), 4, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "neg":              let inst = self.encode_r_type(0x20, self.reg_num(o[1].value), 0, 0, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "seqz":             let inst = self.encode_i_type(1, self.reg_num(o[1].value), 3, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "snez":             let inst = self.encode_r_type(0x00, self.reg_num(o[1].value), 0, 3, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "sltz":             let inst = self.encode_i_type(0, self.reg_num(o[1].value), 2, self.reg_num(o[0].value), 0x13); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "sgtz":             let inst = self.encode_r_type(0x00, self.reg_num(o[1].value), 0, 2, self.reg_num(o[0].value), 0x33); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "ret":              let inst = self.encode_i_type(0, 1, 0, 0, 0x67); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "j":                let inst = self.encode_j_type(self.resolve_target(o[0]) - self.offset, 0, 0x6F); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "jr":               let inst = self.encode_i_type(0, self.reg_num(o[0].value), 0, 0, 0x67); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "call":             let inst = self.encode_j_type(self.resolve_target(o[0]) - self.offset, 1, 0x6F); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "tail":             let inst = self.encode_i_type(0, self.reg_num(o[1].value), 0, 0, 0x67); push(self.output, inst); self.offset = self.offset + 4; return;
        if m == "la":
            # la rd, symbol → auipc rd, %hi(symbol); addi rd, rd, %lo(symbol)
            let sym_addr = self.resolve_target(o[1])
            let hi = (sym_addr >> 12) & 0xFFFFF
            let lo = sym_addr & 0xFFF
            let inst1 = self.encode_u_type(hi << 12, self.reg_num(o[0].value), 0x17)
            let inst2 = self.encode_i_type(lo, self.reg_num(o[0].value), 0, self.reg_num(o[0].value), 0x13)
            push(self.output, inst1); push(self.output, inst2)
            self.offset = self.offset + 8
            return

        push(self.errors, "unknown mnemonic: " + m)

    ## Encode entire program
    proc encode_program(self, prog):
        for node in prog.nodes:
            self.encode(node)
        return self.output
