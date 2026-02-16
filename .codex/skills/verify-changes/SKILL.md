---
name: verify-changes
description: Run or document verification steps and produce a concise verification summary.
---

# Verify Changes

Use this for any change that should include verification evidence.

## Steps
1. Pick the smallest relevant checks:
   - Style: `sh/run_test_format.sh` (run only if explicitly requested)
   - Build: `cmake -S . -B build`
   - Build: `cmake --build build -j${JOBS}` (set `JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)`)
   - Smoke run: `SOUFFLE_BIN=./build/src/souffle examples/running_example/run.sh`
   - Regression tests: `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`
   - Regression target: `cmake --build build --target check-regression`
   - One-shot script: `sh/run_regression_tests.sh`
2. If a command is skipped or fails, record the reason and how a maintainer can run it.
3. Report a verification summary with each command and its status (ran/skipped).
4. Always keep Skills and docs up to date; if they diverge, update both.
5. Docs sync checkpoint: if behavior changed, ask whether to update docs; if yes,
   ensure touched docs include `## Source references` (markdown links) and a
   Related commits section (latest 3).

## Related commits
- `UNCOMMITTED` — docs(testing): switch verify-changes skill to maintained regression workflow
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
- `e28b76ffe` — chore(repo): add codex metadata
