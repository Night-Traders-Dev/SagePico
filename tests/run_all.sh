#!/bin/bash
# SagePico Test Suite — runs all tests against connected Feather
# Usage: ./run_all.sh [--port /dev/ttyACM0] [test_filter]
# Requires: python3, pyserial

set -euo pipefail
cd "$(dirname "$0")"

PORT="${1:-/dev/ttyACM0}"
FILTER="${2:-}"

echo "=== SagePico Test Suite ==="
echo "Port: $PORT"
echo ""

python3 test_runner.py --port "$PORT" $FILTER
