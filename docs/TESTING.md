# Testing

## Status in This Fork
`ctest` is intentionally disabled via `cmake/CTestDisabled.cmake` because the test
suite is out of date. Any `ctest` invocation will fail with a clear error message.

## CI Source-of-Truth Commands
These commands are defined in CI workflows and scripts:
- Format/style: `sh/run_test_format.sh`
- Build:
  - `cmake -S . -B build`
  - `cmake --build build -j${JOBS}`
- Tests (disabled in this fork):
  - `ctest --test-dir build -I "<range>" --output-on-failure --progress -j${JOBS}`

CI sets `JOBS` using:
```
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
```

## Local Validation (Recommended)
Pick the smallest set of checks that match your change:
- Style check: `sh/run_test_format.sh` (requires `clang-format`).
- Build: `cmake -S . -B build` then `cmake --build build -j${JOBS}`.
- Smoke run: `SOUFFLE_BIN=./build/src/souffle examples/running_example/run.sh`.
- Experiment workflows: follow `README.eval.md` or `README.eval.inc.md`.

## Legacy/Experimental Tests
- `tests/` and `tests-old/` scripts were written for older backends (for example
  they use `--inc`) and expect specific build paths. Treat them as legacy.

TODO: define a supported, up-to-date test suite for this fork and update CI accordingly.
