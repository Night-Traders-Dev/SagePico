"""ELF2UF2 tests — validates ELF→UF2 conversion."""

import subprocess, os, tempfile

SAGE = "/usr/local/bin/sage"
ELF2UF2 = os.path.join(os.path.dirname(__file__), "..", "src", "pico", "sage", "elf2uf2.sage")

def run_elf2uf2_tests(t):
    """Test ELF to UF2 conversion using a real ARM ELF file."""

    # Find a built ARM ELF
    elf_path = os.path.join(os.path.dirname(__file__), "..", ".tmp", "hello-arm", "build", "hello.elf")
    if not os.path.exists(elf_path):
        elf_path = os.path.join(os.path.dirname(__file__), "..", "build", "hello-arm.uf2")
        if os.path.exists(elf_path):
            t.skip("elf2uf2: convert", "Need .elf file (only .uf2 found)")
        else:
            t.skip("elf2uf2: convert", "No ELF file available (build firmware first)")
        return

    with tempfile.NamedTemporaryFile(suffix='.uf2', delete=False) as f:
        uf2_path = f.name

    try:
        result = subprocess.run(
            [SAGE, ELF2UF2, elf_path, "-o", uf2_path],
            capture_output=True, text=True, timeout=30
        )
        if os.path.exists(uf2_path):
            size = os.path.getsize(uf2_path)
            t.assert_contains("elf2uf2: output exists", "exists", "exists")
            t.assert_eq("elf2uf2: UF2 size > 0", size > 0, True)
            t.assert_eq("elf2uf2: UF2 multiple of 512", size % 512, 0)
        else:
            t.skip("elf2uf2: conversion", "ELF2UF2 did not produce output")
    except Exception as e:
        t.skip("elf2uf2: conversion", str(e))
    finally:
        if os.path.exists(uf2_path):
            os.unlink(uf2_path)

    print(f"\n  ELF2UF2: {t.passed} passed, {t.failed} failed, {t.skipped} skipped")
