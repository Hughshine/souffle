---
name: souffle-regression
description: Run and maintain the fork's regression suite under tests/regression with the repo-built Souffle binary. Use when users ask to run pre-commit regression checks, add/adjust regression cases, or debug ctest -L regression failures.
---

# Souffle Regression

## Overview

Use this skill for the maintained regression workflow in this fork.

Load `references/regression-context.md` for the current case map and command matrix.

## Workflow

1. Confirm scope:
   - run regression checks, or
   - add/update regression cases, or
   - debug regression failures.
2. Always use repo-built Souffle (`build/src/souffle`) via CMake/CTest wiring:
   - `tests/regression/CMakeLists.txt` passes `$<TARGET_FILE:souffle>` to the runner.
3. Prefer the canonical entry points:
   - `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`
   - `cmake --build build --target check-regression`
   - `sh/run_regression_tests.sh`
4. When adding/changing cases:
   - update `tests/regression/run_regression_case.py` and `tests/regression/CMakeLists.txt` together,
   - keep assertions deterministic (tuple keys + probability tolerance),
   - preserve the `inc-regional` single-round constraint until runtime support expands.
5. If failures appear, compare against `full-hard` baseline outputs first; then inspect dump/log artifacts from the failing case directory.
6. Keep docs and skills synchronized:
   - `docs/TESTING.md`
   - `docs/topics/testing/README.regression.md`
   - `docs/INDEX.md` when docs are added/renamed.
7. Report verification with explicit command status (ran/skipped/failed) and short reasons.

## Related commits
- `UNCOMMITTED` — test(regression): add maintained ctest regression suite
- `UNCOMMITTED` — docs(testing): add regression workflow runbook
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
