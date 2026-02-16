# Regression Context

## Canonical Commands

From repo root:

```bash
cmake -S . -B build
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake --build build -j${JOBS}
ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}
```

Alternatives:

```bash
cmake --build build --target check-regression
sh/run_regression_tests.sh
```

## Case Map (`tests/regression`)

- `smoke_full_only`
- `dred_mix_naive_vs_full`
- `dred_hub_rederive_naive_vs_full`
- `detopt_inc_naive_combo_vs_full`
- `detopt_inc_regional_single_round_vs_full`
- `rewrite_split_modes_equiv`
- `full_det_modes`
- `dump_outputs_contract`

## Assertion Rules

- Compare tuple keys exactly.
- Compare probabilities with `abs_tol=1e-9`.
- Treat missing expected output files as failures.

## Known Constraints

- `inc-regional` currently supports only single-round validation in this suite.
- This suite is for correctness gates, not benchmark/performance scoring.

## Key Files

- `tests/regression/CMakeLists.txt`
- `tests/regression/run_regression_case.py`
- `docs/TESTING.md`
- `docs/topics/testing/README.regression.md`
