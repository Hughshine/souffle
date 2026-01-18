#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROBLOG_BENCH="${PROBLOG_BENCH:-$ROOT/../problog-benchmark}"
BASE_DIR="${BASE_DIR:-$PROBLOG_BENCH/side_channel_inc_artifact}"
CASES="${CASES:-P12,P13,P14,P15,P16,P17,P18,P19,P20}"
RULE_SET="${RULE_SET:-full}"
CHANGE_SPEC="${CHANGE_SPEC:-inc0p1=0.001,inc0p3=0.003,inc0p5=0.005}"
DELTA_LABELS="${DELTA_LABELS:-inc0p1,inc0p3,inc0p5}"
DELTA_SAMPLES="${DELTA_SAMPLES:-1}"
TIMEOUT="${TIMEOUT:-1200}"

if [[ ! -x "$ROOT/build/src/souffle" ]]; then
  echo "Missing $ROOT/build/src/souffle. Build first (cmake -S . -B build; cmake --build build)." >&2
  exit 1
fi

if [[ ! -d "$PROBLOG_BENCH" ]]; then
  echo "Missing problog-benchmark at $PROBLOG_BENCH. Set PROBLOG_BENCH or clone it first." >&2
  exit 1
fi

if git -C "$PROBLOG_BENCH" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  BRANCH="$(git -C "$PROBLOG_BENCH" rev-parse --abbrev-ref HEAD)"
  if [[ "$BRANCH" != "inc-artifact" ]]; then
    echo "Expected problog-benchmark branch 'inc-artifact' but found '$BRANCH'." >&2
  fi
fi

export PATH="$ROOT/build/src:$PATH"

pushd "$PROBLOG_BENCH" >/dev/null
python3 side_channel_inc.py generate \
  --base-dir "$BASE_DIR" \
  --cases "$CASES" \
  --rule-set "$RULE_SET" \
  --seed 0 \
  --cleanup

python3 side_channel_inc.py delta \
  --base-dir "$BASE_DIR" \
  --cases "$CASES" \
  --change-spec "$CHANGE_SPEC" \
  --sets "$DELTA_SAMPLES" \
  --seed 0 \
  --cleanup

python3 side_channel_inc.py compile \
  --base-dir "$BASE_DIR" \
  --cases "$CASES" \
  --timeout "$TIMEOUT"

python3 side_channel_inc.py run \
  --base-dir "$BASE_DIR" \
  --cases "$CASES" \
  --delta-labels "$DELTA_LABELS" \
  --delta-samples "$DELTA_SAMPLES" \
  --timeout "$TIMEOUT" \
  --run-arg=--det-opt

python3 side_channel_inc.py collect --base-dir "$BASE_DIR"
popd >/dev/null
