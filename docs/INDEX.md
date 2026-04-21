# Documentation Index

This index is the source of truth for AE-facing documentation in this branch.
The branch is scoped to the full-mode probabilistic artifact.

Legend:
- [PRIMARY] must-read for current artifact behavior
- [SECONDARY] useful implementation context
- [HISTORICAL] retained only for provenance, not artifact commands

## Entry Points
- [PRIMARY] `README.md` — artifact overview and quickstart
- [PRIMARY] `docs/USAGE.md` — generated-program usage and runtime surface
- [PRIMARY] `docs/TESTING.md` — build and regression commands
- [PRIMARY] `docs/RUNBOOK.md` — concise build/run/troubleshooting guide
- [PRIMARY] `docs/ARCHITECTURE.md` — full-mode architecture map
- [PRIMARY] `docs/topics/rewrite/README.rewrite.impl.md` — rewrite dispatcher behavior
- [SECONDARY] `docs/topics/runtime/README.flag.md` — runtime flag reference
- [SECONDARY] `docs/topics/testing/README.regression.md` — regression suite details
- [SECONDARY] `docs/project/PROBLOG_EXTENSION_STACK.md` — implementation stack
- [SECONDARY] `docs/process/README.git.md` — commit hygiene
- [PRIMARY] `AGENTS.md` — Codex constraints

## Current Full-Mode Topics
- [PRIMARY] `docs/topics/pipeline/README.prune.md` — prune behavior and constraints
- [PRIMARY] `docs/topics/pipeline/README.const.md` — `--det-opt` behavior
- [PRIMARY] `docs/topics/pipeline/README.evidence.md` — evidence semantics
- [PRIMARY] `docs/topics/rewrite/README.rewrite.impl.md` — rewrite dispatcher and graph rewrite
- [SECONDARY] `docs/topics/rewrite/README.split.md` — internal split behavior
- [SECONDARY] `docs/topics/rewrite/README.siso.md` — SISO detection details
- [PRIMARY] `docs/topics/backends/README.cudd.md` — BDD backend
- [SECONDARY] `docs/topics/backends/README.cudd.reordering.md` — reordering details

## Evaluation
- [PRIMARY] `docs/topics/evaluation/README.eval.md` — companion `CAV-FULL`
  benchmark provenance and command shape

## Source references
- [src/MainDriver.cpp](../src/MainDriver.cpp)
- [src/include/souffle/CompiledOptions.h](../src/include/souffle/CompiledOptions.h)
- [src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp)
- [tests/regression/CMakeLists.txt](../tests/regression/CMakeLists.txt)

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
