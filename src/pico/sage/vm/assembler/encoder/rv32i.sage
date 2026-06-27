## encoder/rv32i.sage — Full RV32I instruction encoder
## Encodes R/I/S/B/U/J-type instructions from AST nodes.
## Includes pseudonym expansion (NOP, MV, LI, JMP, RET, etc.)

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
        return (bit12 << 31) | (bit10_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (bit4_1 << 7) | (bit11 << 7) | opcode

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

    ## ---- Encode an instruction node ----
    proc encode(self, node):
        if node == nil:
            return

        # Label — record offset
        if node.kind == "label":
            self.symbols.define(node.name, self.offset)
            return

        # Instruction
        if node.kind == "instruction":
            let m = node.mnemonic
            let o = node.operands

            ## ----- RV32I Base -----
            # R-type
            if m == "add":
                let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "sub":
                let inst = self.encode_r_type(0x20, self.reg_num(o[2].value), self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "xor":
                let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 4, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "or":
                let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 6, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "and":
                let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 7, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "sll":
                let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 1, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "srl":
                let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 5, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "sra":
                let inst = self.encode_r_type(0x20, self.reg_num(o[2].value), self.reg_num(o[1].value), 5, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "slt":
                let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 2, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "sltu":
                let inst = self.encode_r_type(0x00, self.reg_num(o[2].value), self.reg_num(o[1].value), 3, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            # M-extension
            else: if m == "mul":
                let inst = self.encode_r_type(0x01, self.reg_num(o[2].value), self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x33)
                push(self.output, inst); self.offset = self.offset + 4

            # I-type
            else: if m == "addi":
                let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x13)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "xori":
                let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 4, self.reg_num(o[0].value), 0x13)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "ori":
                let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 6, self.reg_num(o[0].value), 0x13)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "andi":
                let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 7, self.reg_num(o[0].value), 0x13)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "slli":
                let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 1, self.reg_num(o[0].value), 0x13)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "srli":
                let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 5, self.reg_num(o[0].value), 0x13)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "srai":
                let inst = self.encode_i_type(o[2].value | 0x400, self.reg_num(o[1].value), 5, self.reg_num(o[0].value), 0x13)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "slti":
                let inst = self.encode_i_type(o[2].value, self.reg_num(o[1].value), 2, self.reg_num(o[0].value), 0x13)
                push(self.output, inst); self.offset = self.offset + 4

            # I-type (loads)
            else: if m == "lw":
                let inst = self.encode_i_type(o[1].value, self.reg_num(o[0].value), 2, self.reg_num(o[0].value), 0x03)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "lb":
                let inst = self.encode_i_type(o[1].value, self.reg_num(o[0].value), 0, self.reg_num(o[0].value), 0x03)
                push(self.output, inst); self.offset = self.offset + 4

            # S-type (stores)
            else: if m == "sw":
                let inst = self.encode_s_type(o[1].value, self.reg_num(o[0].value), self.reg_num(o[0].value), 2, 0x23)
                push(self.output, inst); self.offset = self.offset + 4

            # U-type
            else: if m == "lui":
                let inst = self.encode_u_type(o[1].value, self.reg_num(o[0].value), 0x37)
                push(self.output, inst); self.offset = self.offset + 4

            # J-type
            else: if m == "jal":
                let addr = 0
                if o[1].kind == "label":
                    addr = self.symbols.reference(o[1].value, node)
                else:
                    addr = o[1].value
                let inst = self.encode_j_type(addr, self.reg_num(o[0].value), 0x6F)
                push(self.output, inst); self.offset = self.offset + 4

            # Pseudonyms
            else: if m == "nop":
                let inst = self.encode_i_type(0, 0, 0, 0, 0x13)  # addi x0, x0, 0
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "li":
                # li rd, imm → addi rd, x0, imm
                let inst = self.encode_i_type(o[1].value, 0, 0, self.reg_num(o[0].value), 0x13)
                push(self.output, inst); self.offset = self.offset + 4

            else: if m == "mv":
                # mv rd, rs → addi rd, rs, 0
                let inst = self.encode_i_type(0, self.reg_num(o[1].value), 0, self.reg_num(o[0].value), 0x13)
                push(self.output, inst); self.offset = self.offset + 4

            else:
                push(self.errors, "unknown mnemonic: " + m)

    ## Encode entire program
    proc encode_program(self, prog):
        for node in prog.nodes:
            self.encode(node)
        return self.output
