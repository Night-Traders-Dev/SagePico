#!/bin/bash
# SagePico Build Script for Feather RP2350
# Supports both RISC-V (Hazard3) and ARM (Cortex-M33) targets
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SAGE="${SCRIPT_DIR}/deps/sagelang/sage"
SAGEVM="${SCRIPT_DIR}/deps/SageVM/sagevm"
PICO_SDK="${SCRIPT_DIR}/deps/pico-sdk"
RISCV_TOOLCHAIN="${SCRIPT_DIR}/deps/pico-sdk-tools/build/riscv-install/bin"

BOARD="adafruit_feather_rp2350"
PROGRAM="hello"

SRC_DIR="${SCRIPT_DIR}/src"
BUILD_DIR="${SCRIPT_DIR}/build"

# Parse args
ARCH="${1:-arm}"  # "arm" or "riscv"

case "$ARCH" in
    riscv|rv|hazard3)
        PLATFORM="rp2350-riscv"
        TOOLCHAIN_PATH="$RISCV_TOOLCHAIN"
        CHIP_NAME="Hazard3 RISC-V"
        ARCH_DIR="riscv"
        ;;
    arm|cortex|m33)
        PLATFORM="rp2350-arm-s"
        TOOLCHAIN_PATH=""
        CHIP_NAME="Cortex-M33 ARM"
        ARCH_DIR="arm"
        ;;
    *)
        echo "Usage: $0 [arm|riscv]"
        echo "  arm   - Cortex-M33 ARM (default)"
        echo "  riscv - Hazard3 RISC-V"
        exit 1
        ;;
esac

TMP_DIR="${SCRIPT_DIR}/.tmp/${PROGRAM}-${ARCH}"

echo "=== SagePico: Building for Feather RP2350 (${CHIP_NAME}) ==="
echo ""

# --- Path 1: Sage -> C -> Pico SDK -> UF2 ---
echo "--- Path 1: Baremetal C compilation ---"
echo ""

echo "[1/5] Generating C code from Sage..."
mkdir -p "$TMP_DIR"
"$SAGE" --emit-pico-c "${SCRIPT_DIR}/examples/hello.sage" -o "${TMP_DIR}/hello.c"

echo "[2/5] Patching generated C for baremetal..."
sed -i 's|#include <dlfcn.h>|#include "dlfcn.h"|' "${TMP_DIR}/hello.c"
sed -i 's|#include <semaphore.h>|#include "semaphore.h"|' "${TMP_DIR}/hello.c"
sed -i 's|#include <stdatomic.h>|#include "stdatomic.h"|' "${TMP_DIR}/hello.c"
# Replace pthread.h with our shim (avoids newlib TLS init issues)
sed -i 's|#include <pthread.h>|#include "pthread.h"|' "${TMP_DIR}/hello.c"
echo "  Patched includes"

# Fix: fputs/fputc crash on Pico (newlib fd layer broken). Replace with printf.
python3 "${SCRIPT_DIR}/patch_stdio.py" "${TMP_DIR}/hello.c"
echo "  Patched stdio functions"

python3 "${SCRIPT_DIR}/patch_main.py" "${TMP_DIR}/hello.c" "$ARCH"
echo "  Patched main() for baremetal"

echo "[3/5] Writing CMakeLists.txt..."
mkdir -p "$TMP_DIR"

# Both ARM and RISC-V need stubs (pthread, nanosleep implementations)  
STUBS_OPT="\"${SCRIPT_DIR}/shims/stubs.c\""
SHIMS_OPT="target_include_directories(hello PRIVATE \"${SCRIPT_DIR}/shims\")"

cat > "${TMP_DIR}/CMakeLists.txt" << CMAKEEOF
cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD "${BOARD}" CACHE STRING "Board")
set(PICO_PLATFORM "${PLATFORM}" CACHE STRING "Platform")
include("${PICO_SDK}/external/pico_sdk_import.cmake")
project(hello LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
add_executable(hello "${TMP_DIR}/hello.c" ${STUBS_OPT})
target_link_libraries(hello pico_stdlib hardware_adc hardware_pwm hardware_i2c hardware_spi hardware_pio hardware_dma)
target_include_directories(hello PRIVATE "${SCRIPT_DIR}/src/pico")
target_include_directories(hello PRIVATE "${SCRIPT_DIR}/src/${ARCH_DIR}")
pico_enable_stdio_usb(hello 1)
pico_enable_stdio_uart(hello 1)
target_compile_definitions(hello PRIVATE PICO_STDIO_DEFAULT_CRLF=0)
${SHIMS_OPT}
pico_add_extra_outputs(hello)
CMAKEEOF

echo "[4/5] Running CMake configure..."
rm -rf "${TMP_DIR}/build"
TOOLCHAIN_ARG=""
[ -n "$TOOLCHAIN_PATH" ] && TOOLCHAIN_ARG="-DPICO_TOOLCHAIN_PATH=${TOOLCHAIN_PATH}"

PICO_SDK_PATH="${PICO_SDK}" \
PICO_TOOLCHAIN_PATH="${TOOLCHAIN_PATH}" \
cmake -S "${TMP_DIR}" -B "${TMP_DIR}/build" \
    -DPICO_SDK_PATH="${PICO_SDK}" \
    ${TOOLCHAIN_ARG} \
    -DPICO_BOARD="${BOARD}" \
    -DPICO_PLATFORM="${PLATFORM}" \
    2>&1 | grep -E "Pico Platform|compiler|Build files" || true

echo "[5/5] Building..."
cmake --build "${TMP_DIR}/build" 2>&1 | tail -3

# Copy UF2 to build/
mkdir -p "${BUILD_DIR}"
UF2_OUT="${BUILD_DIR}/hello-${ARCH}.uf2"
if [ -f "${TMP_DIR}/build/hello.uf2" ]; then
    cp "${TMP_DIR}/build/hello.uf2" "${UF2_OUT}"
    echo ""
    echo "=== SUCCESS ==="
    echo "    ${UF2_OUT} ($(ls -lh ${UF2_OUT} | awk '{print $5}'))"
else
    find "${TMP_DIR}/build" -name "*.uf2" -exec cp {} "${UF2_OUT}" \; 2>/dev/null || true
    if [ -f "${UF2_OUT}" ]; then
        echo ""
        echo "=== SUCCESS ==="
        echo "    ${UF2_OUT} ($(ls -lh ${UF2_OUT} | awk '{print $5}'))"
    fi
fi

# --- Path 2: SageVM SRVM ---
echo ""
echo "--- Path 2: SageVM SRVM bytecode ---"
"$SAGEVM" compile "${SCRIPT_DIR}/examples/hello.sage" "${SCRIPT_DIR}/examples/hello.sgvm" 2>/dev/null
"$SAGEVM" compile --riscv "${SCRIPT_DIR}/examples/hello.sage" "${SCRIPT_DIR}/examples/hello.sgrv" 2>/dev/null
echo "  SGVM: examples/hello.sgvm ($(ls -lh ${SCRIPT_DIR}/examples/hello.sgvm | awk '{print $5}'))"
echo "  SRVM: examples/hello.sgrv ($(ls -lh ${SCRIPT_DIR}/examples/hello.sgrv | awk '{print $5}'))"

echo ""
echo "=== Build Complete ==="
echo "Flash: picotool load -f ${UF2_OUT}"
echo "Serial: cat /dev/ttyACM0"
