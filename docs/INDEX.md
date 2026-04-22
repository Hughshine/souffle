# Documentation Index

This index is the source of truth for AE-facing documentation in this branch.
The branch is scoped to the full-mode probabilistic artifact.

Legend:
- [PRIMARY] must-read for current artifact behavior
- [SECONDARY] useful implementation context

## Entry Points
- [PRIMARY] `README.md` — artifact overview and quickstart
- [PRIMARY] `docs/USAGE.md` — generated-program usage and runtime surface
- [PRIMARY] `docs/TESTING.md` — build and regression commands
- [PRIMARY] `docs/RUNBOOK.md` — concise build/run/troubleshooting guide
- [PRIMARY] `docs/ARCHITECTURE.md` — full-mode architecture map
- [PRIMARY] `docs/topics/rewrite/README.rewrite.impl.md` — rewrite dispatcher behavior
- [SECONDARY] `docs/topics/runtime/README.flag.md` — runtime flag reference
- [SECONDARY] `docs/topics/testing/README.regression.md` — regression suite details
- [PRIMARY] `AGENTS.md` — Codex constraints

## Current Full-Mode Topics
- [PRIMARY] `docs/topics/pipeline/README.prune.md` — prune behavior and constraints
- [PRIMARY] `docs/topics/pipeline/README.const.md` — `--det-opt` behavior
- [PRIMARY] `docs/topics/pipeline/README.evidence.md` — evidence semantics
- [PRIMARY] `docs/topics/rewrite/README.rewrite.impl.md` — rewrite dispatcher and graph rewrite
- [PRIMARY] `docs/topics/backends/README.cudd.md` — BDD backend

## Evaluation
- [PRIMARY] `docs/topics/evaluation/README.eval.md` — companion `CAV-FULL`
  benchmark provenance and command shape

## Source references
- [src/MainDriver.cpp](../src/MainDriver.cpp)
- [src/include/souffle/CompiledOptions.h](../src/include/souffle/CompiledOptions.h)
- [src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp)
- [tests/regression/CMakeLists.txt](../tests/regression/CMakeLists.txt)
