#!/bin/bash
# SagePico Build Script for Feather RP2350 (Hazard3 RISC-V)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SAGE="${SCRIPT_DIR}/deps/sagelang/sage"
SAGEVM="${SCRIPT_DIR}/deps/SageVM/sagevm"
PICO_SDK="${SCRIPT_DIR}/deps/pico-sdk"
TOOLCHAIN_DIR="${SCRIPT_DIR}/deps/pico-sdk-tools/build/riscv-install/bin"

BOARD="adafruit_feather_rp2350"
PLATFORM="rp2350-riscv"
CHIP="rp2350-riscv"
PROGRAM="hello"

SRC_DIR="${SCRIPT_DIR}/src"
BUILD_DIR="${SCRIPT_DIR}/build"
TMP_DIR="${SCRIPT_DIR}/.tmp/${PROGRAM}"

echo "=== SagePico: Building Hello from Sage for Feather RP2350 ==="

# --- Path 1: Sage -> C -> Pico SDK -> UF2 ---
echo ""
echo "--- Path 1: Baremetal C compilation ---"
echo ""

echo "[1/4] Generating C code from Sage..."
"$SAGE" --emit-pico-c "${SRC_DIR}/hello.sage" -o "${TMP_DIR}/hello.c"

echo "[2/5] Patching generated C for baremetal..."
# Fix includes that don't exist on baremetal RISC-V
sed -i 's|#include <dlfcn.h>|#include "dlfcn.h"|' "${TMP_DIR}/hello.c"
sed -i 's|#include <semaphore.h>|#include "semaphore.h"|' "${TMP_DIR}/hello.c"
sed -i 's|#include <stdatomic.h>|#include "stdatomic.h"|' "${TMP_DIR}/hello.c"
# Enable POSIX thread + timer declarations in newlib
sed -i 's|#define _POSIX_C_SOURCE 200809L|#define _POSIX_C_SOURCE 200809L\n#define _POSIX_THREADS 1\n#define _POSIX_TIMERS 1|' "${TMP_DIR}/hello.c"
# Add infinite loop after print to keep USB CDC alive
python3 -c "
import sys
data = open('${TMP_DIR}/hello.c').read()
# Replace only the first 'sage_gc_shutdown();' block (in main)
old = '    sage_gc_shutdown();\n    return 0;\n}'
new = '    sage_gc_shutdown();\n    while (1) { tight_loop_contents(); }\n    return 0;\n}'
data = data.replace(old, new, 1)
# Add early printf before Sage runtime init for debug
old2 = '    stdio_init_all();\n    sleep_ms(2000);'
new2 = '    stdio_init_all();\n    printf(\"\\\\n=== SagePico booting... ===\\\\n\");\n    sleep_ms(2000);'
data = data.replace(old2, new2)
open('${TMP_DIR}/hello.c', 'w').write(data)
"
echo "  Patched problematic includes"

echo "[3/5] Writing CMakeLists.txt..."
mkdir -p "$TMP_DIR"
cat > "${TMP_DIR}/CMakeLists.txt" << 'CMAKE_EOF'
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD "adafruit_feather_rp2350" CACHE STRING "RP2350 board")
set(PICO_PLATFORM "rp2350-riscv" CACHE STRING "RP2350 RISC-V platform")
include("__PICO_SDK_IMPORT__")
project(hello LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
add_executable(hello "__SOURCE_PATH__/hello.c" "__STUBS_PATH__")
target_include_directories(hello PRIVATE "__SHIMS_DIR__")
target_link_libraries(hello pico_stdlib)
pico_enable_stdio_usb(hello 1)
pico_enable_stdio_uart(hello 0)
pico_add_extra_outputs(hello)
CMAKE_EOF

# Fill in absolute paths
PKG="${PICO_SDK}/external/pico_sdk_import.cmake"
sed -i "s|__PICO_SDK_IMPORT__|${PKG}|" "${TMP_DIR}/CMakeLists.txt"
sed -i "s|__SOURCE_PATH__|${TMP_DIR}|" "${TMP_DIR}/CMakeLists.txt"
sed -i "s|__SHIMS_DIR__|${SCRIPT_DIR}/shims|" "${TMP_DIR}/CMakeLists.txt"
sed -i "s|__STUBS_PATH__|${SCRIPT_DIR}/shims/stubs.c|" "${TMP_DIR}/CMakeLists.txt"

echo "[4/5] Running CMake configure (RISC-V toolchain)..."
rm -rf "${TMP_DIR}/build"
PICO_SDK_PATH="${PICO_SDK}" \
PICO_TOOLCHAIN_PATH="${TOOLCHAIN_DIR}" \
cmake -S "${TMP_DIR}" -B "${TMP_DIR}/build" \
    -DPICO_SDK_PATH="${PICO_SDK}" \
    -DPICO_TOOLCHAIN_PATH="${TOOLCHAIN_DIR}" \
    -DPICO_BOARD="${BOARD}" \
    -DPICO_PLATFORM="${PLATFORM}"

echo "[5/5] Building..."
cmake --build "${TMP_DIR}/build"

# Copy UF2 to build/
mkdir -p "${BUILD_DIR}"
if [ -f "${TMP_DIR}/build/hello.uf2" ]; then
    cp "${TMP_DIR}/build/hello.uf2" "${BUILD_DIR}/"
    echo ""
    echo "=== SUCCESS: Bootable image created ==="
    echo "    ${BUILD_DIR}/hello.uf2"
    ls -lh "${BUILD_DIR}/hello.uf2"
else
    echo ""
    echo "Looking for UF2 file..."
    find "${TMP_DIR}/build" -name "*.uf2" -exec cp {} "${BUILD_DIR}/" \; -exec echo "  -> {}" \;
fi

# --- Path 2: SageVM SRVM ---
echo ""
echo "--- Path 2: SageVM SRVM bytecode ---"
echo ""

echo "[1/2] Compiling Sage to SageVM with RISC-V backend..."
"$SAGEVM" compile "${SRC_DIR}/hello.sage" "${SRC_DIR}/hello.sgvm"

echo "[2/2] Translating to SRVM (RISC-V bytecode)..."
"$SAGEVM" compile --riscv "${SRC_DIR}/hello.sage" "${SRC_DIR}/hello.sgrv" 2>/dev/null || {
    echo "Note: SRVM direct compile not available in this version"
    echo "Running SGVM on desktop to verify..."
    "$SAGEVM" run "${SRC_DIR}/hello.sgvm"
}

echo ""
echo "=== Build Complete ==="
echo "UF2 image: ${BUILD_DIR}/hello.uf2"
echo "SGVM bytecode: ${SRC_DIR}/hello.sgvm"
echo ""
echo "To flash: Copy build/hello.uf2 to the RPI-RP2 drive (bootloader mode)"
