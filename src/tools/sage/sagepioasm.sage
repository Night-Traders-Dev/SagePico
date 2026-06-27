#!/usr/bin/env sage
# sagepioasm — PIO Assembly Compiler in pure Sage
# Compiles RP2350 PIO assembly to 16-bit binary instructions.
# Usage: sage sagepioasm.sage input.pio [-o output.h] [--c]

import sys
import io

# ---- Instruction encoding tables ----
let CONDITIONS = {"":0, "!x":1, "x--":2, "!y":3, "y--":4, "x!=y":5, "pin":6, "!osre":7}
let WAIT_SRC   = {"gpio":0, "pin":1, "irq":2}
let IN_SRC     = {"pins":0, "x":1, "y":2, "null":3, "isr":6, "osr":7}
let OUT_DST    = {"pins":0, "x":1, "y":2, "null":3, "pindirs":4, "pc":5, "isr":6, "exec":7}
let MOV_DST    = {"pins":0, "x":1, "y":2, "exec":4, "pc":5, "isr":6, "osr":7}
let MOV_SRC    = {"pins":0, "x":1, "y":2, "null":3, "status":5, "isr":6, "osr":7}
let MOV_OP     = {"":0, "!":1, "~":1, "reverse":0, "invert":1, "bitrev":2}
let SET_DST    = {"pins":0, "x":1, "y":2, "pindirs":4}
let IRQ_MOD    = {"":0, "set":0, "nowait":1, "clear":2, "wait":1, "clr":2}

proc dict_get(d, key, defval):
    if dict_has(d, key):
        return d[key]
    return defval
let instructions = []
let labels = {}
let defines = {}
let sideset_count = 0
let errors = []

proc error(msg):
    push(errors, msg)

proc parse_num(s):
    if startswith(s, "0x"):
        return tonumber(s)
    if startswith(s, "0b"):
        let v = 0
        let i = 2
        while i < len(s):
            v = v * 2
            if s[i] == "1":
                v = v + 1
            i = i + 1
        return v
    return tonumber(s)

proc parse_ds(suffix):
    let delay = 0
    let sideset = 0
    if startswith(suffix, "[") and contains(suffix, "]"):
        let inner = ""
        let i = 1
        while i < len(suffix) and suffix[i] != "]":
            inner = inner + suffix[i]
            i = i + 1
        let parts = split(inner, ",")
        if len(parts) >= 1:
            delay = parse_num(strip(parts[0]))
        if len(parts) >= 2:
            sideset = parse_num(strip(parts[1]))
    return [delay, sideset]

# ---- Encoders ----
proc ss(d, s):
    if sideset_count > 0:
        return s * 0x20 + d
    return d

proc encode_jmp(cond, addr, d, s):
    return 0x0000 + dict_get(CONDITIONS, cond, 0) * 0x1000 + addr * 0x40 + ss(d, s)

proc encode_wait(pol, src, idx, d, s):
    return 0x2000 + pol * 0x1000 + dict_get(WAIT_SRC, src, 0) * 0x800 + idx * 0x40 + ss(d, s)

proc encode_in(src, bits, d, s):
    return 0x4000 + dict_get(IN_SRC, src, 0) * 0x800 + bits * 0x40 + ss(d, s)

proc encode_out(dst, bits, d, s):
    return 0x6000 + dict_get(OUT_DST, dst, 0) * 0x800 + bits * 0x40 + ss(d, s)

proc encode_push(iffull, block, d, s):
    return 0x8000 + iffull * 0x1000 + block * 0x800 + ss(d, s)

proc encode_pull(ifempty, block, d, s):
    return 0x8000 + ifempty * 0x1000 + block * 0x800 + 0x80 + ss(d, s)

proc encode_mov(dst, op, src, d, s):
    return 0xa000 + dict_get(MOV_DST, dst, 0) * 0x800 + dict_get(MOV_OP, op, 0) * 0x400 + dict_get(MOV_SRC, src, 3) * 0x40 + ss(d, s)

proc encode_irq(num, mod, d, s):
    return 0xc000 + num * 0x800 + dict_get(IRQ_MOD, mod, 0) * 0x400 + ss(d, s)

proc encode_set(dst, data, d, s):
    return 0xe000 + dict_get(SET_DST, dst, 0) * 0x800 + data * 0x40 + ss(d, s)

proc last(arr):
    return arr[len(arr)-1]

# ---- Line parser ----
proc parse_line(line):
    let semi = indexof(line, ";")
    if semi >= 0:
        line = line[:semi]
    line = strip(line)
    if line == "":
        return -1

    if endswith(line, ":"):
        labels[line[:len(line)-1]] = len(instructions)
        return -1

    if startswith(line, "."):
        let parts = split(line, " ")
        let dir = strip(parts[0])
        if dir == ".program":
            return -1
        if dir == ".wrap_target":
            return -1
        if dir == ".wrap":
            return -1
        if dir == ".side_set":
            if len(parts) >= 2:
                sideset_count = parse_num(strip(parts[1]))
            return -1
        if dir == ".define":
            if len(parts) >= 3:
                defines[parts[1]] = parse_num(strip(parts[2]))
            return -1
        if dir == ".origin":
            return -1
        error("unknown directive: " + dir)
        return -1

    let tokens = split(line, " ")
    let clean = []
    for t in tokens:
        let tok = strip(t)
        if tok != "":
            push(clean, tok)
    if len(clean) == 0:
        return -1

    let op = clean[0]
    let delay = 0
    let sideset = 0

    if len(clean) > 0 and startswith(clean[len(clean)-1], "["):
        let ds = parse_ds(clean[len(clean)-1])
        delay = ds[0]
        sideset = ds[1]
        pop(clean)

    if op == "jmp":
        let cond = ""
        let addr = 0
        if len(clean) >= 2:
            let arg = clean[1]
            if dict_has(CONDITIONS, arg):
                cond = arg
                if len(clean) >= 3:
                    let a = clean[2]
                    if dict_has(labels, a):
                        addr = labels[a]
                    else:
                        addr = parse_num(a)
            else:
                if dict_has(labels, arg):
                    addr = labels[arg]
                else:
                    addr = parse_num(arg)
        return encode_jmp(cond, addr, delay, sideset)

    if op == "wait":
        let pol = 0
        let src = "gpio"
        let idx = 0
        if len(clean) >= 2:
            let arg = clean[1]
            if arg == "0" or arg == "1":
                pol = parse_num(arg)
                if len(clean) >= 3:
                    src = clean[2]
                if len(clean) >= 4:
                    idx = parse_num(clean[3])
            else:
                src = arg
                if len(clean) >= 3:
                    idx = parse_num(clean[2])
        return encode_wait(pol, src, idx, delay, sideset)

    if op == "in":
        let src = "pins"
        let bits = 1
        if len(clean) >= 2:
            src = clean[1]
        if len(clean) >= 3:
            bits = parse_num(clean[2])
        return encode_in(src, bits, delay, sideset)

    if op == "out":
        let dst = "pins"
        let bits = 1
        if len(clean) >= 2:
            dst = clean[1]
        if len(clean) >= 3:
            bits = parse_num(clean[2])
        return encode_out(dst, bits, delay, sideset)

    if op == "push":
        let iffull = 0
        let block = 0
        if len(clean) >= 2:
            if clean[1] == "iffull":
                iffull = 1
        if len(clean) >= 2:
            if clean[1] == "block":
                block = 1
        if len(clean) >= 3:
            if clean[2] == "block":
                block = 1
        return encode_push(iffull, block, delay, sideset)

    if op == "pull":
        let ifempty = 0
        let block = 0
        if len(clean) >= 2:
            if clean[1] == "ifempty":
                ifempty = 1
        if len(clean) >= 2:
            if clean[1] == "block":
                block = 1
        if len(clean) >= 3:
            if clean[2] == "block":
                block = 1
        return encode_pull(ifempty, block, delay, sideset)

    if op == "mov":
        let dst = "x"
        let op_mov = ""
        let src = "x"
        if len(clean) >= 2:
            dst = clean[1]
        if len(clean) >= 3:
            let m = clean[2]
            if dict_has(MOV_OP, m):
                op_mov = m
                if len(clean) >= 4:
                    src = clean[3]
            else:
                src = m
        return encode_mov(dst, op_mov, src, delay, sideset)

    if op == "irq":
        let num = 0
        let mod = ""
        if len(clean) >= 2:
            num = parse_num(clean[1])
        if len(clean) >= 3:
            mod = clean[2]
        return encode_irq(num, mod, delay, sideset)

    if op == "set":
        let dst = "pins"
        let data = 0
        if len(clean) >= 2:
            dst = clean[1]
        if len(clean) >= 3:
            data = parse_num(clean[2])
        return encode_set(dst, data, delay, sideset)

    if op == "nop":
        return encode_mov("y", "", "y", 0, 0)

    error("unknown opcode: " + op)
    return -1

# ---- Assemble ----
proc assemble(source):
    instructions = []
    labels = {}
    defines = {}
    errors = []
    sideset_count = 0

    let lines = split(source, "\n")
    for line in lines:
        let inst = parse_line(strip(line))
        if inst >= 0:
            push(instructions, inst)

    if len(errors) > 0:
        for e in errors:
            print e
        return []
    return instructions

# ---- Output ----
proc hex4(val):
    let h = "0123456789abcdef"
    return h[(val>>12)&0xf] + h[(val>>8)&0xf] + h[(val>>4)&0xf] + h[val&0xf]

proc to_c_array(prog):
    let out = "static const uint16_t pio_program[] = {\n    "
    let i = 0
    for inst in prog:
        out = out + "0x" + hex4(inst)
        if i < len(prog) - 1:
            out = out + ", "
        i = i + 1
        if i % 8 == 0 and i < len(prog):
            out = out + "\n    "
    return out + "\n};"

# ---- CLI ----
let args = sys.args()
let input_file = ""
let output_file = ""
let output_c = false

let i = 1
while i < len(args):
    let arg = args[i]
    if arg == "-o":
        if i + 1 < len(args):
            i = i + 1
            output_file = args[i]
    else:
        if arg == "--c":
            output_c = true
        else:
            if not startswith(arg, "-"):
                input_file = arg
    i = i + 1

if input_file == "":
    print "sagepioasm — PIO Assembly Compiler for RP2350"
    print "Usage: sage sagepioasm.sage input.pio [-o output.h] [--c]"
    return

let source = io.readfile(input_file)
if source == "" or source == nil:
    print "Error: cannot read " + input_file
    return

let prog = assemble(source)
if len(errors) > 0 or len(prog) == 0:
    print "Compilation failed."
    if len(errors) > 0:
        for e in errors:
            print "ERROR: " + e
    return

print "Compiled: " + str(len(prog)) + " instructions"

if output_c:
    let code = to_c_array(prog)
    if output_file != "":
        io.writefile(output_file, code)
        print "C header written to " + output_file
    else:
        print code
else:
    for inst in prog:
        print hex4(inst)
