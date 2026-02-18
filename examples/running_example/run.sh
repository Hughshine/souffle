#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INPUT_DIR="$ROOT_DIR/input"
OUTPUT_DIR="$ROOT_DIR/output"
BUILD_DIR="$ROOT_DIR/build"
COMPUTE_DL="$ROOT_DIR/compute.dl"
COMPUTE_BIN="$BUILD_DIR/compute"

SOUFFLE_BIN="${SOUFFLE_BIN:-}"
SOUFFLE_COMPILE_OPTS="${SOUFFLE_COMPILE_OPTS:-}"
SOUFFLE_RUN_OPTS="${SOUFFLE_RUN_OPTS:-}"

if [[ -z "$SOUFFLE_BIN" ]]; then
  if [[ -x "$ROOT_DIR/../../cmake-build-release/src/souffle" ]]; then
    SOUFFLE_BIN="$ROOT_DIR/../../cmake-build-release/src/souffle"
  elif command -v souffle >/dev/null 2>&1; then
    SOUFFLE_BIN="$(command -v souffle)"
  else
    echo "Souffle binary not found. Set SOUFFLE_BIN or build it first." >&2
    exit 1
  fi
fi

mkdir -p "$OUTPUT_DIR" "$BUILD_DIR"

"$SOUFFLE_BIN" $SOUFFLE_COMPILE_OPTS -F "$INPUT_DIR" -D "$OUTPUT_DIR" "$COMPUTE_DL" -o "$COMPUTE_BIN"
"$COMPUTE_BIN" $SOUFFLE_RUN_OPTS -F "$INPUT_DIR" -D "$OUTPUT_DIR"
