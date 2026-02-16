---
name: souffle-test-case
description: Create or update maintained regression test cases under `tests/regression` (static-first case assets, per-case dynamic generators when needed), wire case registration/timeouts, and verify with `ctest -L regression`.
---

# Souffle Test Case

Use this skill when the task is to add, modify, or debug one or more maintained
regression test cases.

Load `references/test-case-context.md` for templates and the update checklist.

## Workflow

1. Scope the case change.
- Identify target case id(s) and expected semantic/property coverage.
- Confirm whether `inc-regional` is involved (single-round only for now).

2. Prefer static case assets.
- Put case files under `tests/regression/cases/<case-id>/`.
- Default layout:
  - `compute.dl`
  - `input/*.facts`
  - optional `input/*.prob`

3. Use dynamic generation only when needed.
- If input is large/derived, add `tests/regression/cases/<case-id>/generate.py`.
- Generator must accept `--out-dir` and write `compute.dl` + `input/*` there.

4. Wire case execution in runner + CTest.
- Add/adjust case function in `tests/regression/run_regression_case.py`.
- Register in `CASES` map.
- Register ctest entry in `tests/regression/CMakeLists.txt` via `add_regression_case(...)`.

5. Set timeouts deliberately.
- Every case must have a CTest timeout in `tests/regression/CMakeLists.txt`.
- Keep timeouts tight enough to fail fast on hangs but loose enough for CI variance.

6. Preserve regression assertions.
- Compare tuple-key sets exactly.
- Compare probabilities with deterministic tolerance (`abs_tol=1e-9`).
- Validate expected artifact contracts when relevant (dump/log/json/dot).

7. Verify and report.
- Run: `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`
- If needed: `cmake --build build --target check-regression`
- Report per-command status (ran/skipped/failed) with short reasons.

8. Keep docs in sync when case inventory or workflow changes.
- `docs/TESTING.md`
- `docs/topics/testing/README.regression.md`

## Guardrails

- Always use repo-built Souffle from CMake/CTest wiring (`$<TARGET_FILE:souffle>`).
- Do not re-enable or rely on removed legacy suites.
- Do not commit generated outputs from `build/` or case runtime artifacts.
