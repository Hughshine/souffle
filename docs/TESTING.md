# Testing

Use this page to choose validation commands for changes to the artifact branch.
For artifact evaluation, the maintained regression label is `regression`.

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

- Exact probabilistic inference smoke tests.
- ProbLog string, numeric, and aggregate round-trips.
- Deterministic-relation analysis through `--det-opt`.
- Rewrite dispatcher equivalence on maintained cases.
- Output and JSON log contracts.

The regression runner uses the compiler built in `build/src/souffle`, so it
does not depend on a system Souffle binary.

## Artifact-Level Checks

For benchmark validation, run paired plain and rewrite commands from
[docs/USAGE.md](USAGE.md) on cases from the companion `problog-benchmark`
artifact. Compare the generated `facts.prob` files with the benchmark checker.

Symbolization comparisons allow absolute probability error up to `1e-8`.
Side-channel and taint comparisons should match exactly.

## Source Map

- [../tests/regression/CMakeLists.txt](../tests/regression/CMakeLists.txt):
  regression case registration.
- [../tests/regression/run_regression_case.py](../tests/regression/run_regression_case.py):
  case runner and output checker.
