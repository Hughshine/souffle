# Regression Testing Workflow

## Source references
- [../../../CMakeLists.txt](../../../CMakeLists.txt)
- [../../../tests/regression/CMakeLists.txt](../../../tests/regression/CMakeLists.txt)
- [../../../tests/regression/run_regression_case.py](../../../tests/regression/run_regression_case.py)
- [../../../sh/run_regression_tests.sh](../../../sh/run_regression_tests.sh)
- [../../TESTING.md](../../TESTING.md)

## Scope
This document is the detailed runbook for the maintained regression suite under
`tests/regression/`.

Use this suite for correctness regression checks before merging changes to:
- full-mode probabilistic execution
- derivation graph construction, pruning, and rewrite
- deterministic-relation analysis (`--det-opt`)
- string/numeric/aggregate output handling
- dump/log output contracts

## Canonical Entry Points
All commands below use the repo build and avoid system-wide `souffle` binaries.

1. One-shot script:
   - `sh/run_regression_tests.sh`
2. CMake target:
   - `cmake --build build --target check-regression`
3. Direct ctest label:
   - `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`

## Cases and Coverage
Current maintained cases:
- `regression.smoke` (`smoke_full_only`):
  - compile/run smoke in full mode.
- `regression.rewrite_dispatch` (`rewrite_dispatch_equiv`):
  - bare `--rewrite` dispatcher consistency against no rewrite.
- `regression.full_det_modes` (`full_det_modes`):
  - `--det-opt` equivalence to baseline.
- `regression.problog_string_roundtrip`:
  - string tuple rendering and probability-key stability.
- `regression.problog_symbol_aggregate_roundtrip`:
  - symbol/aggregate round-trip behavior.
- `regression.problog_sum_exact_roundtrip`:
  - aggregate `sum` support in probabilistic graph construction.
- `regression.dump_contract` (`dump_outputs_contract`):
  - verifies dump/log/stats artifact creation contracts.

## Case Asset Model
- Case data lives under `tests/regression/cases/<case-id>/`.
- Static-first:
  - keep `compute.dl` and `input/*.facts` plus optional `input/*.prob` as
    checked-in files.
- Dynamic fallback for larger/derived inputs:
  - add `tests/regression/cases/<case-id>/generate.py`.
  - the runner invokes this per-case generator with `--out-dir <work-case-dir>`.

## Assertions
The runner enforces:
- tuple-key equality between compared outputs
- numeric probability closeness (`abs_tol=1e-9`)
- expected dump/log file existence for contract checks

The suite is a correctness gate, not a performance benchmark.  Heavy
`CAV-FULL` benchmark workflows remain separate.

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
