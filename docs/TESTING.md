# Testing

Use this page to choose validation commands for the incremental artifact branch.
The maintained regression label is `regression`.

## Build Check

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

Run this after C++ or CMake changes.

## Regression Check

```bash
ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}
```

Equivalent CMake entry point:

```bash
cmake --build build --target check-regression
```

## What the Regression Label Covers

- Multi-turn DRed-style delete/rederive correctness.
- `inc-naive` versus `full-hard` on maintained mixed-update witnesses.
- `inc-regional` single-turn correctness and multi-turn state-machine fallback.
- The non-recursive mixed-update `@post_delete_*` timestamp witness.
- Canonical online CLI controls for `setmode`, `set sem-mode`, `set fc-mode`,
  dump toggles, profile-stage toggles, and elastic fallback behavior.

The regression runner uses the compiler built in `build/src/souffle`, so it
does not depend on a system Souffle binary.

## Artifact-Level Checks

Benchmark validation lives in the companion `problog-benchmark` artifact on
branch `CAV-INC`. The normal entry point from this repo is:

```bash
sh/run_artifact_inc.sh
```

That workflow generates side-channel workspaces, compiles benchmark binaries,
runs the maintained incremental modes, and collects per-turn JSON/TSV outputs.

## Source Map

- [../tests/regression/CMakeLists.txt](../tests/regression/CMakeLists.txt):
  regression case registration.
- [../tests/regression/run_regression_case.py](../tests/regression/run_regression_case.py):
  case runner and output checker.
- [topics/evaluation/README.artifact.inc.md](topics/evaluation/README.artifact.inc.md):
  benchmark-facing artifact workflow.
