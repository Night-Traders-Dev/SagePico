#!/bin/bash
# SagePico Test Suite - Desktop verification (no hardware required)

SAGE="./deps/sagelang/sage"
SAGEVM="./deps/SageVM/sagevm"
PASS=0
FAIL=0

check() {
    local name="$1"
    shift
    echo -n "  $name ... "
    if "$@" >/dev/null 2>&1; then
        echo "PASS"
        ((PASS++))
    else
        echo "FAIL"
        ((FAIL++))
    fi
}

echo "=== SagePico Test Suite ==="
echo ""

echo "--- Sage Interpreter ---"
check "Interpret hello.sage"      "$SAGE" src/hello.sage

echo ""
echo "--- SageVM ---"
check "Compile to SGVM"           "$SAGEVM" compile src/hello.sage
check "Run SGVM bytecode"         "$SAGEVM" run src/hello.sgvm
check "Compile to SRVM (RISC-V)"  "$SAGEVM" compile --riscv src/hello.sage

echo ""
echo "--- Pico C Generation ---"
check "Generate Pico C code"      "$SAGE" --emit-pico-c src/hello.sage -o /tmp/test_pico.c
if [ -f /tmp/test_pico.c ]; then
    echo "    Lines: $(wc -l < /tmp/test_pico.c)"
    rm -f /tmp/test_pico.c
fi

echo ""
echo "--- Build (UF2) ---"
if [ -f build/hello.uf2 ]; then
    SIZE=$(ls -lh build/hello.uf2 | awk '{print $5}')
    echo "  UF2 image: build/hello.uf2 ($SIZE)"
    ((PASS++))
else
    echo "  UF2 not built - run ./build.sh first"
    ((FAIL++))
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && echo "All tests passed."
