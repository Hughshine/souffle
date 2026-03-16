#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN_DIR="${ROOT_DIR}/bin"
TMP_DIR="${ROOT_DIR}/tmp"

mkdir -p "${BIN_DIR}" "${TMP_DIR}"

echo "[build] weighted_conversion_demo (no CMake)"
g++ -std=c++20 -O2 \
  -I"${ROOT_DIR}/../../src/include" \
  "${ROOT_DIR}/weighted_conversion.cpp" \
  "${ROOT_DIR}/weighted_conversion_demo.cpp" \
  -o "${BIN_DIR}/weighted_conversion_demo"

echo "[test] toy weighted conversion"
TOY_OUT="$("${BIN_DIR}/weighted_conversion_demo" --input "${ROOT_DIR}/toy_weighted.cnf")"
echo "${TOY_OUT}"

echo "${TOY_OUT}" | rg -q '^valid=true$'
echo "${TOY_OUT}" | rg -q '^unsat=false$'
echo "${TOY_OUT}" | rg -q '^output vars/clauses=9/9$'
echo "${TOY_OUT}" | rg -q '^divideExp=8$'

echo "[test] tilt guard should fail"
if "${BIN_DIR}/weighted_conversion_demo" \
    --input "${ROOT_DIR}/toy_weighted.cnf" \
    --tilt-max 2 \
    --fail-on-tilt >"${TMP_DIR}/tilt_fail.out" 2>&1; then
  echo "Expected tilt guard to fail, but it succeeded."
  exit 1
fi
rg -q '^valid=false$' "${TMP_DIR}/tilt_fail.out"
rg -q 'Tilt exceeds configured threshold' "${TMP_DIR}/tilt_fail.out"
echo "Tilt failure output:"
cat "${TMP_DIR}/tilt_fail.out"

echo "[test] preprocessing with forced assignment"
cat > "${TMP_DIR}/forced_weight.cnf" <<'EOF'
p cnf 2 1
c p show 1 2 0
1 2 0
c p weight 1 1.0 0
EOF

FORCED_OUT="$("${BIN_DIR}/weighted_conversion_demo" --input "${TMP_DIR}/forced_weight.cnf")"
echo "${FORCED_OUT}"
echo "${FORCED_OUT}" | rg -q '^valid=true$'
echo "${FORCED_OUT}" | rg -q '^forced assignments=1$'

echo "[done] converter experiment passed."

if [[ -n "${APPROXMC_PREFIX:-}" ]]; then
  echo "[build] weighted_appmc_demo (with approxmc from APPROXMC_PREFIX=${APPROXMC_PREFIX})"
  g++ -std=c++20 -O2 \
    -I"${ROOT_DIR}/../../src/include" \
    "${ROOT_DIR}/weighted_conversion.cpp" \
    "${ROOT_DIR}/weighted_appmc.cpp" \
    "${ROOT_DIR}/weighted_appmc_demo.cpp" \
    -I"${APPROXMC_PREFIX}/include" \
    -L"${APPROXMC_PREFIX}/lib" \
    -Wl,-rpath,"${APPROXMC_PREFIX}/lib" \
    -lapproxmc \
    -lgmpxx \
    -lgmp \
    -o "${BIN_DIR}/weighted_appmc_demo"

  echo "[run] weighted_appmc_demo"
  LD_LIBRARY_PATH="${APPROXMC_PREFIX}/lib:${LD_LIBRARY_PATH:-}" \
    "${BIN_DIR}/weighted_appmc_demo" --input "${ROOT_DIR}/toy_weighted.cnf" --tilt-max 100
else
  echo "[skip] weighted_appmc_demo build skipped (set APPROXMC_PREFIX to enable)."
fi
