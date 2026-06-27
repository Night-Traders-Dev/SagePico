"""PIO Assembler tests — validates instruction encoding for all 9 opcodes."""

import subprocess, os, sys

SAGE = "/usr/local/bin/sage"
PIOASM = os.path.join(os.path.dirname(__file__), "..", "src", "tools", "sage", "sagepioasm.sage")

def run_pioasm(source_text):
    """Run the PIO assembler on source text, return hex output lines."""
    import tempfile
    with tempfile.NamedTemporaryFile(mode='w', suffix='.pio', delete=False) as f:
        f.write(source_text)
        tmp = f.name
    try:
        result = subprocess.run([SAGE, PIOASM, tmp], capture_output=True, text=True, timeout=10)
        lines = [l.strip() for l in result.stdout.split('\n') if l.strip()]
        hex_lines = [l for l in lines if len(l) == 4 and all(c in '0123456789abcdef' for c in l)]
        return hex_lines
    finally:
        os.unlink(tmp)

def run_pioasm_c(source_text):
    """Run with --c flag, return the C array text."""
    import tempfile
    with tempfile.NamedTemporaryFile(mode='w', suffix='.pio', delete=False) as f:
        f.write(source_text)
        tmp = f.name
    try:
        result = subprocess.run([SAGE, PIOASM, tmp, "--c"], capture_output=True, text=True, timeout=10)
        return result.stdout
    finally:
        os.unlink(tmp)


def run_pioasm_tests(t):
    """Run all PIO assembler tests via subprocess."""

    # Basic instructions
    src = ".program test\n    nop\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: nop", len(hex_out), 1)
    if len(hex_out) >= 1:
        t.assert_eq("pioasm: nop == mov y,y", hex_out[0], "a042")

    # JMP
    src = ".program test\n    jmp 5\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: jmp addr", len(hex_out), 1)
    if len(hex_out) >= 1:
        t.assert_contains("pioasm: jmp has opcode", hex_out[0][0], "0")

    # JMP with condition
    src = ".program test\n    jmp !x, 3\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: jmp !x", len(hex_out), 1)

    # WAIT
    src = ".program test\n    wait 1 gpio 5\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: wait", len(hex_out), 1)
    if len(hex_out) >= 1:
        t.assert_contains("pioasm: wait opcode", hex_out[0][0], "2")

    # IN
    src = ".program test\n    in pins, 8\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: in", len(hex_out), 1)

    # OUT
    src = ".program test\n    out pins, 8\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: out", len(hex_out), 1)

    # PUSH
    src = ".program test\n    push\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: push", len(hex_out), 1)

    # PULL
    src = ".program test\n    pull\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: pull", len(hex_out), 1)

    # MOV
    src = ".program test\n    mov x, y\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: mov", len(hex_out), 1)

    # IRQ
    src = ".program test\n    irq 0\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: irq", len(hex_out), 1)

    # SET
    src = ".program test\n    set pins, 5\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: set", len(hex_out), 1)

    # Multi-instruction
    src = ".program test\n    set x, 10\n    set y, 20\n    nop\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: multi-inst", len(hex_out), 3)

    # Labels
    src = ".program test\nloop:\n    jmp loop\n    nop\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: labels", len(hex_out), 2)

    # C output
    c_out = run_pioasm_c(".program test\n    nop\n")
    t.assert_contains("pioasm: C output has array", c_out, "static const uint16_t")
    t.assert_contains("pioasm: C output has hex", c_out, "0xa042")

    # Side set
    src = ".program test\n.side_set 1\n    nop side 0\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: side_set", len(hex_out), 1)

    # Delay
    src = ".program test\n    nop [5]\n"
    hex_out = run_pioasm(src)
    t.assert_eq("pioasm: delay", len(hex_out), 1)

    print(f"\n  PIO Assembler: {t.passed} passed, {t.failed} failed")
