#!/bin/bash

echo "=== SagePico RP2350 Test Suite ==="
echo

# Test 1: Check if Sage interpreter works
echo "Test 1: Sage interpreter"
./deps/sagelang/sage src/hello.sage
echo

# Test 2: Check if SageVM compilation works
echo "Test 2: SageVM compilation"
./deps/SageVM/sagevm compile src/hello.sage
echo

# Test 3: Check if SageVM execution works
echo "Test 3: SageVM execution"
./deps/SageVM/sagevm run src/hello.sgvm
echo

# Test 4: Check if Pico C generation works
echo "Test 4: Pico C generation"
./deps/sagelang/sage --emit-pico-c src/hello.sage -o test_pico.c
if [ -f test_pico.c ]; then
    echo "✓ Generated test_pico.c"
    head -5 test_pico.c
    rm test_pico.c
else
    echo "✗ Failed to generate test_pico.c"
fi
echo

echo "=== Test Complete ==="
echo
echo "To build for RP2350 baremetal, install ARM toolchain:"
echo "  sudo apt-get install gcc-arm-none-eabi"
echo "Then run: make pico"