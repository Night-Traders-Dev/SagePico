#!/bin/bash
# Build and install sagecom to /usr/local/bin
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== sagecom install ==="

# 1. Create the launcher script  
LAUNCHER="${SCRIPT_DIR}/sagecom"
SAGECOM_SRC="${PROJECT_DIR}/src/tools/sage/sagecom.sage"
SAGE_BIN="${PROJECT_DIR}/deps/sagelang/sage"

# Ensure the Sage interpreter exists
if [ ! -f "$SAGE_BIN" ]; then
    echo "Error: Sage interpreter not found at $SAGE_BIN"
    exit 1
fi

# 2. Make launcher executable and update paths
chmod +x "$LAUNCHER"

# 3. Install to /usr/local/bin
INSTALL_PATH="/usr/local/bin/sagecom"
echo "Installing to $INSTALL_PATH..."

if [ -w /usr/local/bin ]; then
    cp "$LAUNCHER" "$INSTALL_PATH"
else
    sudo cp "$LAUNCHER" "$INSTALL_PATH"
    sudo chmod +x "$INSTALL_PATH"
fi

echo ""
echo "sagecom installed successfully!"
echo "Usage: sagecom [--port /dev/ttyACM0] [--baud 115200]"
echo ""
echo "Note: sagecom uses the Sage interpreter (${SAGE_BIN})."
echo "      For native compilation, the Sage --compile backend"
echo "      must add FFI support (ptr_to_int, mem_read, etc)."
echo "      Track: https://github.com/Night-Traders-Dev/SageLang"
