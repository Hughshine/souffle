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
- canonical compiler/runtime/online-CLI flag surfaces
- standalone graph-query exact replay flag surfaces
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
- `regression.canonical_compile` (`canonical_compile_defaults_contract`):
  - canonical compiler-surface defaults (`--sem-mode`, `--fc-mode`,
    `--rewrite-engine`, `--det-mode`, `--dd-backend`, `--dump`,
    `--profile-stage`) propagate into the generated binary and preserve
    `facts.prob`.
- `regression.canonical_cli` (`canonical_online_cli_surface`):
  - canonical online CLI surface (`show config`, `set sem-mode`, `set fc-mode`,
    `set/unset dump`, `set/unset profile-stage`) plus startup-only guardrails
    and `elastic -> inc-naive` fallback coverage.
- `regression.graph_query_canonical` (`graph_query_canonical_surface`):
  - standalone `souffle-problog-graph-query` accepts canonical exact/rewrite
    flags, reproduces a small exact replay result, and rejects unsupported
    canonical combinations like `--full-evaluator=scbf`.
- `regression.side_channel_full_pipeline` (`side_channel_full_pipeline_rewrite`):
  - benchmark-derived side-channel full case (`P1`, trimmed ruleset) generated
    on the fly, then checked across no-rewrite / legacy rewrite / implicit /
    iterative implicit full runs for both `facts.prob` and emitted `.csv`
    relation outputs.
- `regression.scbf_rewrite_lane` (`scbf_rewrite_runtime_lane`):
  - small full-only smoke that keeps `--scbf --rewrite` on the maintained path
    and checks runtime-lane telemetry against the actual combined execution path.
- `regression.side_channel_inc_pipeline` (`side_channel_incremental_pipeline_modes`):
  - benchmark-derived side-channel incremental case (`P1`, trimmed ruleset)
    with generated deltas; compares `inc-naive` and single-round
    `inc-regional` against `full-hard`.
- `regression.taint_pipeline` (`taint_stage_pipeline_compile_smoke`):
  - benchmark-derived taint stage chain (`andors-trail`, bundle v2) compiled
    and run stage-by-stage with reduced stage inputs to guard compile/runtime
    reliability of the staged taint workflow.
- `regression.datarace_pipeline` (`datarace_stage_pipeline_smoke`):
  - reduced multi-stage data-race pipeline smoke inspired by the
    `checkExcludedM -> parallel -> escaping -> datarace` stage sequence.

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
- Benchmark-derived regression cases intentionally use reduced inputs/sample
  counts so they stay CI-sized while still preserving semantically rich stage
  sequencing, rewrite/query-output contracts, and the same compile/runtime
  surfaces as the larger benchmark families.

## When To Run
- Always run before committing runtime/compiler changes that can affect semantics.
- Recommended minimum pre-commit check:
  - `ctest --test-dir build -L regression --output-on-failure --progress`

## Related commits
- `UNCOMMITTED` — test(regression): add maintained ctest workflow and cases
- `UNCOMMITTED` — docs(testing): document maintained regression runbook
- `UNCOMMITTED` — test(regression): cover canonical flag surfaces and graph-query replay
- `UNCOMMITTED` — test(regression): add benchmark-derived side-channel, taint, and data-race pipeline cases
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
