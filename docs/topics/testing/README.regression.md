# Incremental Regression Workflow

This page documents the maintained regression suite for the incremental
artifact branch.

## Canonical Commands

```bash
ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}
cmake --build build --target check-regression
```

Both commands use the repo-built compiler and generated runtimes from the
current build tree.

## Maintained Cases

- `regression.dred_mix`:
  mixed updates against `full-hard`.
- `regression.dred_hub`:
  higher fan-in/fan-out delete/rederive pressure.
- `regression.detopt_combo`:
  `--det-opt --post-del --no-reuse-var-index --no-single-rand-fast`.
- `regression.detopt_regional`:
  single-turn `inc-regional` against `full-hard`.
- `regression.detopt_regional_multiturn`:
  non-degenerate multi-turn regional state machine and fallback.
- `regression.detopt_regional_degenerate`:
  degenerate multi-turn regional turns that should remain regional.
- `regression.detopt_derivation_guard`:
  recursive delete/rederive guard under `--det-opt`.
- `regression.nonrecursive_timestamp_views`:
  maintained `@post_delete_*` witness for non-recursive mixed updates.
- `regression.canonical_cli`:
  online CLI surface for mode switching, dumps, profiles, and elastic fallback.

## Scope

The suite is a correctness gate for incremental runtime behavior. It is not a
performance benchmark and it does not depend on the companion benchmark repo.

Paper-facing side-channel evaluation stays separate in
[../evaluation/README.artifact.inc.md](../evaluation/README.artifact.inc.md).
