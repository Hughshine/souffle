#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RESEARCH="$ROOT/research"
EXTERNAL="$RESEARCH/external"
BUILD="$EXTERNAL/build"
JOBS="${JOBS:-4}"

want_check=0
want_pepin=0
want_schlandals=0

usage() {
  cat <<'EOF'
Usage: sh/setup/setup_approx_external_deps.sh [--check] [--pepin] [--schlandals] [--all]

Prepare optional external dependencies for the experimental standalone approx
backends. The script only writes under this repo's research/external/ and the
local Schlandals FFI target directory. It does not run sudo and does not delete
existing checkouts.

Options:
  --check        Print detected dependency state.
  --pepin       Clone/build pepin under research/external.
  --schlandals  Clone Schlandals and fetch Rust dependencies for the FFI wrapper.
  --all         Run --pepin and --schlandals.
  -h, --help    Show this help.
EOF
}

if [[ $# -eq 0 ]]; then
  usage
  exit 0
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --check)
      want_check=1
      ;;
    --pepin)
      want_pepin=1
      ;;
    --schlandals)
      want_schlandals=1
      ;;
    --all)
      want_pepin=1
      want_schlandals=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing required command: $1" >&2
    exit 1
  fi
}

ensure_research() {
  if [[ ! -d "$RESEARCH/.git" ]]; then
    echo "missing research repo: $RESEARCH" >&2
    echo "clone it first, for example:" >&2
    echo "  git clone git@github.com:Hughshine/psouffle-research.git \"$RESEARCH\"" >&2
    exit 1
  fi
  mkdir -p "$EXTERNAL" "$BUILD"
}

clone_or_report() {
  local url="$1"
  local dst="$2"
  if [[ -d "$dst/.git" ]]; then
    echo "exists: $dst"
  else
    git clone "$url" "$dst"
  fi
}

patch_pepin_if_needed() {
  local header="$EXTERNAL/pepin/src/pepin-int.h"
  if [[ ! -f "$header" ]]; then
    return
  fi
  if grep -q 'assert(num' "$header"; then
    echo "patching local pepin fill_unset assert typo"
    perl -0pi -e 's/assert\(num/assert\(nvars/g' "$header"
  fi
}

setup_pepin() {
  ensure_research
  need_cmd git
  need_cmd cmake
  need_cmd perl
  clone_or_report "https://github.com/meelgroup/pepin" "$EXTERNAL/pepin"
  patch_pepin_if_needed
  cmake -S "$EXTERNAL/pepin" -B "$BUILD/pepin" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$BUILD/pepin" -j"$JOBS"
}

setup_schlandals() {
  ensure_research
  need_cmd git
  need_cmd cargo
  clone_or_report "https://github.com/aia-uclouvain/schlandals" "$EXTERNAL/schlandals"
  cargo fetch --manifest-path "$ROOT/src/problog/approx/schlandals_ffi/Cargo.toml"
}

check_state() {
  echo "repo: $ROOT"
  echo "research: $RESEARCH"
  echo
  if [[ -x "$BUILD/pepin/pepin" ]]; then
    echo "pepin: direct build present at $BUILD/pepin/pepin"
  elif [[ -d "$EXTERNAL/pepin" ]]; then
    echo "pepin: source present, build missing at $BUILD/pepin/pepin"
  else
    echo "pepin: missing"
  fi
  if [[ -f "$EXTERNAL/schlandals/Cargo.toml" ]]; then
    echo "schlandals: source present at $EXTERNAL/schlandals"
  else
    echo "schlandals: missing"
  fi
  if [[ -x "$BUILD/approxmc-v6/approxmc" ]]; then
    echo "approxmc cli: present at $BUILD/approxmc-v6/approxmc"
  else
    echo "approxmc cli: not found at $BUILD/approxmc-v6/approxmc"
  fi
  if [[ -d "$HOME/.local/approxmc-stack" ]]; then
    echo "approxmc stack prefix: $HOME/.local/approxmc-stack"
  else
    echo "approxmc stack prefix: missing at $HOME/.local/approxmc-stack"
  fi
  echo
  echo "after setup, rerun:"
  echo "  cmake -S . -B build"
  echo "  cmake --build build --target souffle-problog-graph-query -j\${JOBS:-4}"
}

if [[ "$want_pepin" -eq 1 ]]; then
  setup_pepin
fi

if [[ "$want_schlandals" -eq 1 ]]; then
  setup_schlandals
fi

if [[ "$want_check" -eq 1 || ( "$want_pepin" -eq 0 && "$want_schlandals" -eq 0 ) ]]; then
  check_state
fi
