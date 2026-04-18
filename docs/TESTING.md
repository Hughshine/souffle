# Testing

## Source references
- [CMakeLists.txt](CMakeLists.txt)
- [tests/regression/CMakeLists.txt](tests/regression/CMakeLists.txt)
- [tests/regression/run_regression_case.py](tests/regression/run_regression_case.py)
- [sh/run_regression_tests.sh](sh/run_regression_tests.sh)
- [docs/topics/testing/README.regression.md](docs/topics/testing/README.regression.md)
- [cmake/CTestDisabled.cmake](cmake/CTestDisabled.cmake)
- [sh/run_test_format.sh](sh/run_test_format.sh)


## Status in This Fork
Maintained regression tests are enabled through CTest labels (`regression`).
Legacy test suites have been removed from this fork.
Two heavier benchmark-derived stage-pipeline smokes are maintained separately
under the `stage-pipeline-heavy` label and are not part of the default regression
gate.

`cmake/CTestDisabled.cmake` is now only used when explicitly configured with:
`-DSOUFFLE_DISABLE_CTEST=ON`.

## CI Source-of-Truth Commands
These commands are defined in CI workflows and scripts:
- Format/style: `sh/run_test_format.sh`
- Build:
  - `cmake -S . -B build`
  - `cmake --build build -j${JOBS}`
- Regression tests:
  - `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`
  - `cmake --build build --target check-regression`
  - `ctest --test-dir build -L stage-pipeline-heavy --output-on-failure --progress -j1`
  - `cmake --build build --target check-regression-heavy`
  - `sh/run_regression_tests.sh`

CI sets `JOBS` using:
```
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
```

## Local Validation (Recommended)
Pick the smallest set of checks that match your change:
- Style check: `sh/run_test_format.sh` (requires `clang-format`).
- Build: `cmake -S . -B build` then `cmake --build build -j${JOBS}`.
- Smoke run: `SOUFFLE_BIN=./build/src/souffle examples/running_example/run.sh`.
- Regression run (pre-commit default for runtime/compiler changes):
  - `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`
  - Detailed case map: `docs/topics/testing/README.regression.md`
- Opt-in heavy stage-pipeline run:
  - `ctest --test-dir build -L stage-pipeline-heavy --output-on-failure --progress -j1`
- Experiment workflows: follow `docs/topics/evaluation/README.eval.md` or `docs/topics/evaluation/README.eval.inc.md`.
- Historical evaluation logs are in `docs/historical/` and indexed by `docs/topics/evaluation/README.md`.

## Regression Suite Scope
- Online incremental correctness checks compare:
  - `inc-naive` vs `full-hard`
  - `inc-regional` vs `full-hard` (single-round only)
- Coverage includes:
  - DRed-sensitive deletion/rederive scenarios
  - det-opt/default-on and `--no-det-opt` with incremental/full mode combinations
  - rewrite split-mode equivalence
  - dirty-frontier rewrite detection vs forced full-detect equivalence
  - canonical compiler/runtime/online-CLI flag surfaces
  - standalone graph-query exact replay and canonical parser validation
  - benchmark-derived side-channel full/inc compile-run workflows, including
    `.csv` relation-output checks on the maintained full rewrite case
  - `--scbf --rewrite` lane telemetry on a maintained smoke case
  - dump/log artifact contracts
- Separate opt-in `stage-pipeline-heavy` coverage includes:
  - staged taint pipeline compile/run reliability
  - staged data-race pipeline smoke coverage
- The regression runner always compiles with the repo-built binary passed from
  CMake (`$<TARGET_FILE:souffle>`), avoiding accidental `/usr/local/bin/souffle`.
- Regression inputs are organized under `tests/regression/cases/`:
  - static `compute.dl` + `input/*.facts/*.prob` by default
  - optional per-case `generate.py` for larger derived inputs
- The default correctness gate runs the `regression` label only.
- Heavy stage-pipeline smokes live under `stage-pipeline-heavy`, run serially, and
  use larger CTest timeouts because the generated host C++ compile step is much
  more sensitive to machine load than the ordinary maintained cases.

## Legacy/Experimental Tests
- Historical legacy suites were removed; use `docs/historical/` notes for past workflows.

## Related commits
- `UNCOMMITTED` — test(regression): move heavy stage-pipeline smokes behind opt-in stage-pipeline-heavy label
- `UNCOMMITTED` — test(regression): add maintained CTest regression workflow
- `UNCOMMITTED` — docs(testing): add dedicated regression runbook under docs/topics/testing
- `UNCOMMITTED` — docs(testing): document regression labels and cmake targets
- `UNCOMMITTED` — test(regression): add canonical flag and graph-query maintained coverage
- `UNCOMMITTED` — test(regression): add benchmark-derived side-channel, taint, and data-race stage coverage
- `aaa18c137` — docs(repo): add core docs
