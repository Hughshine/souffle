#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
JOBS="${JOBS:-$(nproc || sysctl -n hw.ncpu || echo 2)}"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j"$JOBS"
ctest --test-dir "$BUILD_DIR" -L regression --output-on-failure --progress -j"$JOBS"
