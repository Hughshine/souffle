# Codex Context (Historical)

## Source references
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)


> This file is a historical snapshot. For current guidance, use `AGENTS.md` and
> `docs/INDEX.md`.

## Status
- Historical snapshot; do not use as the source of truth.

## Entry Points
- `AGENTS.md` for agent constraints and verification expectations.
- `README.md` for user-facing overview and quickstart.
- `docs/INDEX.md` for the documentation map.

## Key Docs
- `docs/ARCHITECTURE.md`
- `docs/USAGE.md`
- `docs/TESTING.md`
- `docs/RUNBOOK.md`
- `README.eval.md` (full evaluation)
- `README.eval.inc.md` (incremental evaluation)
- `README.rewrite.impl.md` (rewrite pipeline)

## Code Hotspots
- `src/include/souffle/problog/` (derivation graph, pipeline, rewrite)
- `src/problog/Pipeline.cpp` (pipeline orchestration)
- `src/include/souffle/cli/Cli.h` (CLI + batch runs)
- `src/ast2ram/online/` (online translation)

## Experiments
- Full/rewrite benchmarks: see `README.eval.md` and `README.rewrite.impl.md`.
- Incremental benchmarks: see `README.eval.inc.md` and `README.inc.region.md`.

## Related commits
- `812ea4081` — docs(repo): refine README narratives
- `3e9b024ca` — docs(readme): refresh eval and pipeline notes
- `86b6c2379` — docs(readme): update precompile and quickstart notes
