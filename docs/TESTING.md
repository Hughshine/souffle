# Testing

## Source references
- [CMakeLists.txt](../CMakeLists.txt)
- [tests/regression/CMakeLists.txt](../tests/regression/CMakeLists.txt)
- [tests/regression/run_regression_case.py](../tests/regression/run_regression_case.py)
- [sh/run_regression_tests.sh](../sh/run_regression_tests.sh)
- [docs/topics/testing/README.regression.md](topics/testing/README.regression.md)
- [cmake/CTestDisabled.cmake](../cmake/CTestDisabled.cmake)
- [sh/run_test_format.sh](../sh/run_test_format.sh)

## Status in This Fork
Maintained regression tests are enabled through CTest labels (`regression`).
Legacy test suites have been removed from this AE branch.

`cmake/CTestDisabled.cmake` is only used when explicitly configured with:
`-DSOUFFLE_DISABLE_CTEST=ON`.

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
- End-to-end artifact validation: run a generated case from the companion
  `CAV-FULL` benchmark artifact with and without `--rewrite`.

## Regression Suite Scope
- Full-mode probabilistic smoke tests.
- ProbLog string, numeric, and aggregate round-trips.
- `--det-opt` equivalence to baseline.
- Bare `--rewrite` dispatcher equivalence to no-rewrite on maintained cases.
- Dump/log artifact contracts.

The regression runner always compiles with the repo-built binary passed from
CMake (`$<TARGET_FILE:souffle>`), avoiding accidental system `souffle` binaries.

## Legacy/Experimental Tests
Historical legacy suites are not part of the AE branch.
