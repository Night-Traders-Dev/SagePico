## tokens.sage — Token types for the RISC-V assembler
## Defines all token kinds used by the lexer and parser.

# Token kinds as integer constants
let TOK_EOF        = 0
let TOK_LABEL      = 1    # loop:
let TOK_MNEMONIC   = 2    # ADDI, JMP, etc.
let TOK_REGISTER   = 3    # x0, a0, sp, etc.
let TOK_INTEGER    = 4    # 42, 0xFF, 0b1010
let TOK_STRING     = 5    # "hello"
let TOK_COMMA      = 6    # ,
let TOK_LPAREN     = 7    # (
let TOK_RPAREN     = 8    # )
let TOK_LBRACKET   = 9    # [
let TOK_RBRACKET   = 10   # ]
let TOK_PLUS       = 11   # +
let TOK_MINUS      = 12   # -
let TOK_STAR       = 13   # *
let TOK_SLASH      = 14   # /
let TOK_DIRECTIVE  = 15   # .text, .data, .global
let TOK_IDENTIFIER = 16   # generic identifier
let TOK_NEWLINE    = 17
let TOK_COMMENT    = 18   # ; comment
let TOK_HASH       = 19   # # comment
let TOK_SEMI       = 20   # ;
let TOK_COLON      = 21   # :
let TOK_EQU        = 22   # =
let TOK_EXCLAM     = 23   # !
let TOK_TILDE      = 24   # ~
let TOK_AMPERSAND  = 25   # &
let TOK_PIPE       = 26   # |
let TOK_CARET      = 27   # ^

# Token struct
class Token:
    proc init(self, kind, text, line, col):
        self.kind = kind
        self.text = text
        self.line = line
        self.col = col
        self.value = nil  # parsed numeric value for integers

# Token names for debugging
let TOKEN_NAMES = {
    0:"EOF", 1:"LABEL", 2:"MNEMONIC", 3:"REGISTER", 4:"INTEGER",
    5:"STRING", 6:"COMMA", 7:"LPAREN", 8:"RPAREN", 9:"LBRACKET",
    10:"RBRACKET", 11:"PLUS", 12:"MINUS", 13:"STAR", 14:"SLASH",
    15:"DIRECTIVE", 16:"IDENTIFIER", 17:"NEWLINE", 18:"COMMENT"
}

# Valid RISC-V register names
let REGISTERS = {
    "zero":0, "ra":1, "sp":2, "gp":3, "tp":4,
    "t0":5, "t1":6, "t2":7, "s0":8, "fp":8,
    "s1":9, "a0":10, "a1":11, "a2":12, "a3":13,
    "a4":14, "a5":15, "a6":16, "a7":17,
    "s2":18, "s3":19, "s4":20, "s5":21,
    "s6":22, "s7":23, "s8":24, "s9":25,
    "s10":26, "s11":27, "t3":28, "t4":29, "t5":30, "t6":31,
    "x0":0, "x1":1, "x2":2, "x3":3, "x4":4, "x5":5, "x6":6, "x7":7,
    "x8":8, "x9":9, "x10":10, "x11":11, "x12":12, "x13":13,
    "x14":14, "x15":15, "x16":16, "x17":17, "x18":18, "x19":19,
    "x20":20, "x21":21, "x22":22, "x23":23, "x24":24, "x25":25,
    "x26":26, "x27":27, "x28":28, "x29":29, "x30":30, "x31":31
}

# RISC-V instruction mnemonics
let MNEMONICS = {
    "add":1, "sub":1, "sll":1, "slt":1, "sltu":1, "xor":1, "srl":1, "sra":1, "or":1, "and":1,
    "addi":1, "slli":1, "slti":1, "sltiu":1, "xori":1, "srli":1, "srai":1, "ori":1, "andi":1,
    "lb":1, "lh":1, "lw":1, "lbu":1, "lhu":1, "sb":1, "sh":1, "sw":1,
    "beq":1, "bne":1, "blt":1, "bge":1, "bltu":1, "bgeu":1,
    "jal":1, "jalr":1, "lui":1, "auipc":1, "ecall":1, "ebreak":1, "fence":1,
    "mul":1, "mulh":1, "div":1, "rem":1, "nop":1,
    "jmp":1, "jr":1, "ret":1, "call":1, "tail":1, "mv":1, "not":1, "neg":1, "seqz":1, "snez":1, "sltz":1, "sgtz":1,
    "li":1, "la":1,
    # Graphics ISA
    "fill":1, "line":1, "blit":1, "sprite":1, "flip":1, "vsync":1,
    "pixel":1, "rect":1, "circle":1, "triangle":1, "clear":1,
    "text":1, "print":1, "tile":1, "map":1, "camera":1, "scroll":1,
    "palette":1, "dma_copy":1, "dma_fill":1
}

# Check if a string is a valid register name
proc is_register(s):
    return dict_has(REGISTERS, s)

# Check if a string is a valid mnemonic
proc is_mnemonic(s):
    return dict_has(MNEMONICS, s)
