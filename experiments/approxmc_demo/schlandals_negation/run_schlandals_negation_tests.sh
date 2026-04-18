#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BIN="${ROOT}/build/src/souffle-problog-graph-query"
CASES_DIR="${ROOT}/experiments/approxmc_demo/schlandals_negation/cases"
SIDE_JSON="${ROOT}/problog-benchmark/.worktree/full-artifact/side_channel_full/P6/output/derivation.json"

if [[ ! -x "${BIN}" ]]; then
  echo "missing binary: ${BIN}" >&2
  echo "build with: cmake --build build --target souffle-problog-graph-query -j4" >&2
  exit 1
fi

if [[ ! -f "${SIDE_JSON}" ]]; then
  echo "missing side-channel bundle: ${SIDE_JSON}" >&2
  exit 1
fi

run_query() {
  local json="$1"
  local query="$2"
  local backend="$3"
  "${BIN}" --json "${json}" --query "${query}" --backend "${backend}" 2>&1
}

extract_result() {
  local output="$1"
  printf '%s\n' "${output}" | sed -n 's/^\[result\] .* = //p' | tail -n 1
}

assert_close() {
  local lhs="$1"
  local rhs="$2"
  local label="$3"
  python3 - "$lhs" "$rhs" "$label" <<'PY'
import math
import sys
lhs = float(sys.argv[1])
rhs = float(sys.argv[2])
label = sys.argv[3]
if not math.isclose(lhs, rhs, rel_tol=1e-7, abs_tol=1e-8):
    raise SystemExit(f"{label}: {lhs} != {rhs}")
PY
}

assert_exact() {
  local value="$1"
  local expected="$2"
  local label="$3"
  python3 - "$value" "$expected" "$label" <<'PY'
import math
import sys
value = float(sys.argv[1])
expected = float(sys.argv[2])
label = sys.argv[3]
if not math.isclose(value, expected, rel_tol=0.0, abs_tol=1e-12):
    raise SystemExit(f"{label}: {value} != {expected}")
PY
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  local label="$3"
  if [[ "${haystack}" != *"${needle}"* ]]; then
    echo "${label}: missing expected substring" >&2
    echo "expected: ${needle}" >&2
    echo "actual output:" >&2
    printf '%s\n' "${haystack}" >&2
    exit 1
  fi
}

echo "[case] positive regression: KEY_IND(85)"
bdd_out="$(run_query "${SIDE_JSON}" 'KEY_IND(85)' bdd)"
sch_out="$(run_query "${SIDE_JSON}" 'KEY_IND(85)' schlandals)"
bdd_val="$(extract_result "${bdd_out}")"
sch_val="$(extract_result "${sch_out}")"
assert_close "${bdd_val}" "${sch_val}" "KEY_IND(85) bdd vs schlandals"
echo "  bdd=${bdd_val} schlandals=${sch_val}"

echo "[case] legal stratified negation: pass(1)"
bdd_out="$(run_query "${CASES_DIR}/stratified_negation.json" 'pass(1)' bdd)"
sch_out="$(run_query "${CASES_DIR}/stratified_negation.json" 'pass(1)' schlandals)"
bdd_val="$(extract_result "${bdd_out}")"
sch_val="$(extract_result "${sch_out}")"
assert_exact "${bdd_val}" "0.7" "pass(1) bdd"
assert_close "${bdd_val}" "${sch_val}" "pass(1) bdd vs schlandals"
echo "  bdd=${bdd_val} schlandals=${sch_val}"

echo "[case] legal stratified negation guard-blocking: blocked_pass(1)"
bdd_out="$(run_query "${CASES_DIR}/stratified_negation.json" 'blocked_pass(1)' bdd)"
sch_out="$(run_query "${CASES_DIR}/stratified_negation.json" 'blocked_pass(1)' schlandals)"
bdd_val="$(extract_result "${bdd_out}")"
sch_val="$(extract_result "${sch_out}")"
assert_exact "${bdd_val}" "0.0" "blocked_pass(1) bdd"
assert_close "${bdd_val}" "${sch_val}" "blocked_pass(1) bdd vs schlandals"
echo "  bdd=${bdd_val} schlandals=${sch_val}"

echo "[case] rejection: negative recursion"
set +e
negrec_out="$(run_query "${CASES_DIR}/negative_recursion.json" 'a(1)' schlandals)"
negrec_rc=$?
set -e
if [[ ${negrec_rc} -eq 0 ]]; then
  echo "negative recursion unexpectedly succeeded" >&2
  printf '%s\n' "${negrec_out}" >&2
  exit 1
fi
assert_contains "${negrec_out}" "does not support non-stratified negation or recursion through negation" "negative recursion rejection"
echo "  rejected as expected"

echo "[case] rejection: probabilistic negated lower stratum"
set +e
probneg_out="$(run_query "${CASES_DIR}/probabilistic_negation.json" 'a(1)' schlandals)"
probneg_rc=$?
set -e
if [[ ${probneg_rc} -eq 0 ]]; then
  echo "probabilistic negation unexpectedly succeeded" >&2
  printf '%s\n' "${probneg_out}" >&2
  exit 1
fi
assert_contains "${probneg_out}" "currently supports negation only over deterministic lower-stratum relations" "probabilistic negation rejection"
echo "  rejected as expected"

echo "[ok] schlandals negation experiment checks passed"
