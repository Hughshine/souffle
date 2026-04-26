# Incremental Regression Suite

## Source References

- [../../../tests/regression/CMakeLists.txt:22](../../../tests/regression/CMakeLists.txt#L22): registered regression cases.
- [../../../tests/regression/CMakeLists.txt:33](../../../tests/regression/CMakeLists.txt#L33): `check-regression` target.
- [../../../tests/regression/run_regression_case.py:153](../../../tests/regression/run_regression_case.py#L153): compile helper.
- [../../../tests/regression/run_regression_case.py:357](../../../tests/regression/run_regression_case.py#L357): deterministic mixed-update case.
- [../../../tests/regression/run_regression_case.py:421](../../../tests/regression/run_regression_case.py#L421): regional multi-turn state-machine case.
- [../../../tests/regression/run_regression_case.py:631](../../../tests/regression/run_regression_case.py#L631): canonical CLI surface case.
- [../../../tests/regression/run_regression_case.py:901](../../../tests/regression/run_regression_case.py#L901): case dispatch table.

## Case Groups

- `regression.dred_mix` and `regression.dred_hub`: DRed delete/rederive behavior against `full`.
- `regression.deterministic_combo`: deterministic-relation analysis across mixed insert/delete turns.
- `regression.deterministic_regional`: single-turn `inc-regional` against `full`.
- `regression.inc_regional_calibration`: single-interface regional update with boundary calibration.
- `regression.inc_regional_shared_delta_join`: shared-delta join closure for regional analysis.
- `regression.deterministic_regional_multiturn`: regional state-machine fallback and re-entry.
- `regression.deterministic_regional_degenerate`: regional turns that should remain regional.
- `regression.deterministic_derivation_guard`: recursive delete/rederive guard.
- `regression.negated_absent_tuple`: negated absent tuple grounding.
- `regression.nonrecursive_timestamp_views`: `@post_delete_*` timestamp views.
- `regression.canonical_cli`: canonical mode/output CLI surface.

## Run

```bash
ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}
cmake --build build --target check-regression
```

Each case compiles a generated runtime using the repo-built `souffle` target,
executes scripted CLI turns, and compares incremental outputs against
`full` probability files.
