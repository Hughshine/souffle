# Testing

## Source References

- [../CMakeLists.txt:93](../CMakeLists.txt#L93): CMake testing option.
- [../CMakeLists.txt:302](../CMakeLists.txt#L302): regression subdirectory wiring.
- [../tests/regression/CMakeLists.txt:22](../tests/regression/CMakeLists.txt#L22): maintained case registration.
- [../tests/regression/CMakeLists.txt:37](../tests/regression/CMakeLists.txt#L37): `check-regression` target.
- [../tests/regression/run_regression_case.py:153](../tests/regression/run_regression_case.py#L153): maintained compile invocation.
- [topics/testing/README.regression.md:1](topics/testing/README.regression.md#L1): case-level scope.

## Commands

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}
cmake --build build --target check-regression
```

## Scope

The maintained suite is the incremental CTest regression label. It compares
incremental modes against `full` on small programs covering:

- DRed delete/rederive behavior.
- deterministic-relation analysis under mixed updates.
- `inc-regional` correctness and multi-turn fallback.
- boundary calibration profile details for `inc-regional` single-interface updates.
- shared-delta join closure for `inc-regional` regional analysis.
- independent regional boundary updates that skip unnecessary overlap closure.
- deterministic-chain boundary anchors for `inc-regional`.
- rejection of deterministic-chain anchors whose path crosses the current region.
- recursive derivation guards.
- negated absent tuple grounding.
- `@post_delete_*` timestamp views for non-recursive mixed updates.
- canonical CLI mode switching, dumps, profiles, and output naming.
