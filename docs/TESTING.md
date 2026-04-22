# Testing

## Source References
- [CMakeLists.txt](../CMakeLists.txt)
- [tests/regression/CMakeLists.txt](../tests/regression/CMakeLists.txt)
- [tests/regression/run_regression_case.py](../tests/regression/run_regression_case.py)
- [sh/run_regression_tests.sh](../sh/run_regression_tests.sh)
- [sh/run_test_format.sh](../sh/run_test_format.sh)

## Status in This Fork
CTest label `regression` selects the maintained regression tests.

## CI Source-of-Truth Commands
- Format/style: `sh/run_test_format.sh`
- Build:
  - `cmake -S . -B build`
  - `cmake --build build -j${JOBS}`
- Regression tests:
  - `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`
  - `cmake --build build --target check-regression`
  - `sh/run_regression_tests.sh`

CI sets `JOBS` using:
```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
```

## Local Validation
Pick the smallest set of checks that match your change:
- Style check: `sh/run_test_format.sh` (requires `clang-format`).
- Build: `cmake -S . -B build` then `cmake --build build -j${JOBS}`.
- Regression run for runtime/compiler changes:
  `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`.
- End-to-end artifact validation: run plain and rewrite generated cases from
  the companion `CAV-FULL` benchmark artifact.

## Regression Suite Scope
- Full-mode probabilistic smoke tests.
- ProbLog string, numeric, and aggregate round-trips.
- `--det-opt` equivalence to baseline.
- Bare `--rewrite` dispatcher matches plain output on maintained cases.
- Dump/log artifact contracts.

The regression runner compiles with the repo-built binary from CMake
(`$<TARGET_FILE:souffle>`), avoiding accidental system `souffle` binaries.
