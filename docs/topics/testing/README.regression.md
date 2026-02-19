# Regression Testing Workflow (Maintained)

## Source references
- [CMakeLists.txt](CMakeLists.txt)
- [tests/regression/CMakeLists.txt](tests/regression/CMakeLists.txt)
- [tests/regression/run_regression_case.py](tests/regression/run_regression_case.py)
- [tests/regression/cases/README.md](tests/regression/cases/README.md)
- [sh/run_regression_tests.sh](sh/run_regression_tests.sh)
- [docs/TESTING.md](docs/TESTING.md)


## Scope
This document is the detailed runbook for the maintained regression suite under
`tests/regression/`.

Use this suite for correctness regression checks before merging changes to:
- incremental CLI behavior (`--setmode`, `insert/delete/commit`)
- DRed / apply-delta behavior
- det-opt/no-det-opt and related mixed-mode options
- rewrite split-mode behavior
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
  - compile/run smoke in full-only mode.
- `regression.dred_mix` (`dred_mix_naive_vs_full`):
  - multi-turn mixed updates; compares `inc-naive` to `full-hard`.
- `regression.dred_hub` (`dred_hub_rederive_naive_vs_full`):
  - higher fan-in/fan-out rederive pressure; compares `inc-naive` to `full-hard`.
- `regression.detopt_combo` (`detopt_inc_naive_combo_vs_full`):
  - `--det-opt --post-del --no-reuse-var-index --no-single-rand-fast` with incremental turns
    (explicit `--det-opt` kept for coverage; default is on).
- `regression.detopt_regional` (`detopt_inc_regional_single_round_vs_full`):
  - `inc-regional + det-opt` correctness check against `full-hard` (default-on behavior).
- `regression.detopt_derivation_guard` (`detopt_recursive_derivation_guard_vs_full`):
  - det-opt recursive delete/rederive guard for multi-support tuples (`inc-naive` vs `full-hard`).
- `regression.rewrite_split` (`rewrite_split_modes_equiv`):
  - rewrite/no-rewrite consistency across split modes.
- `regression.rewrite_dirty_detect` (`rewrite_dirty_detect_equiv`):
  - dirty-frontier SISO detection correctness/equivalence against forced full detection.
- `regression.full_det_modes` (`full_det_modes`):
  - det-opt (default-on) equivalence to baseline and `det-force` all-ones contract.
- `regression.dump_contract` (`dump_outputs_contract`):
  - verifies dump/log/stats artifact creation contracts.

## Case Asset Model
- Case data now lives under `tests/regression/cases/<case-id>/`.
- Static-first:
  - keep `compute.dl` and `input/*.facts` (and optional `input/*.prob`) as checked-in files.
- Dynamic fallback for larger/derived inputs:
  - add `tests/regression/cases/<case-id>/generate.py`.
  - the runner invokes this per-case generator with `--out-dir <work-case-dir>`.

## Assertions
The runner enforces:
- tuple-key equality between compared outputs
- numeric probability closeness (`abs_tol=1e-9`)
- expected dump/log file existence for contract checks

## Current Limits
- `inc-regional` is currently validated in single-round form only.
  - Multi-round cases are intentionally deferred until runtime support is ready.
- The suite is a correctness gate, not a performance benchmark.
  - Heavy `problog-benchmark` workflows remain separate.

## When To Run
- Always run before committing runtime/compiler changes that can affect semantics.
- Recommended minimum pre-commit check:
  - `ctest --test-dir build -L regression --output-on-failure --progress`

## Related commits
- `UNCOMMITTED` — test(regression): add maintained ctest workflow and cases
- `UNCOMMITTED` — docs(testing): document maintained regression runbook
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
