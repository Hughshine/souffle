# Documentation Index

This index is the source of truth for doc location, recency, and importance. Core Souffle docs live at the repo root and under `docs/`. Benchmark/experiment/example docs remain in their original directories.

Legend:
- [LATEST] most recent canonical snapshot
- [PRIMARY] must-read for current behavior
- [SECONDARY] useful context but not required
- [DESIGN] proposal/roadmap (not implemented)
- [HISTORICAL] archived or superseded

## Entry Points
- [PRIMARY] `README.md` — project overview + quickstart
- [PRIMARY] `docs/USAGE.md` — CLI/runtime options (authoritative)
- [SECONDARY] `README.flag.md` — consolidated compiler/runtime flag reference
- [SECONDARY] `README.dump.md` — dump/debug outputs (compiler + runtime)
- [PRIMARY] `docs/TESTING.md` — verification status + test commands
- [PRIMARY] `docs/RUNBOOK.md` — build/run/rollback/troubleshooting
- [PRIMARY] `docs/ARCHITECTURE.md` — system architecture map
- [PRIMARY] `README.git.md` — commit hygiene
- [SECONDARY] `CONTRIBUTING.md` — contributor workflow
- [PRIMARY] `AGENTS.md` — Codex constraints (kept at repo root)

## Current Pipeline & Semantics
- [PRIMARY] `README.dred.md` — online DRed semantics + performance
- [PRIMARY] `README.inc.region.md` — inc-regional implementation
- [PRIMARY] `README.prune.md` — prune behavior + constraints
- [PRIMARY] `README.fc.delete.md` — incremental delete logic
- [PRIMARY] `README.evidence.md` — evidence semantics and flow
- [PRIMARY] `README.const.md` — det-opt behavior
- [SECONDARY] `README.souffle.opt.md` — AST/RAM transform inventory

## Rewrite & Graph Transforms
- [PRIMARY] `README.rewrite.impl.md` — current rewrite behavior
- [SECONDARY] `README.split.md` — split behavior + design notes
- [SECONDARY] `README.siso.md` — SISO detection details
- [SECONDARY] `README.mst.md` — magic-set (MST) translator notes
- [DESIGN] `README.rewrite.opt.md` — optimization ideas (not implemented)

## Backends
- [PRIMARY] `README.cudd.md` — BDD backend
- [SECONDARY] `README.cudd.reordering.md` — reordering details
- [SECONDARY] `README.sdd.md` — SDD backend status/gaps
- [DESIGN] `README.ordering.md` — ordering implementation notes (FORCE path exists but is unused by CUDD)

## Profiling
- [PRIMARY] `README.profile.md` — profiling guide (online)
- [SECONDARY] `README.profile.inc.md` — incremental profiling notes
- [SECONDARY] `README.fc.profile.md` — FC profiling fields

## Evaluation & Results (Souffle)
- [LATEST] `README.eval.final.md` — latest incremental evaluation log (2026-01-21)
- [SECONDARY] `README.eval.inc.md` — incremental benchmark workflow + historical runs
- [SECONDARY] `README.eval.md` — full benchmark workflow + notes
- [SECONDARY] `OPT.md` — inc-regional run procedure + ratio collection notes
- [SECONDARY] `README.artifact.inc.md` — incremental artifact reproduction
- [HISTORICAL] `README.table.md` — table generation notes
- [HISTORICAL] `README.disjunct.md` — disjunction strengthening log
- [HISTORICAL] `README.problog.md` — ProbLog baseline runs

## Out of Scope (Benchmarks/Experiments/Examples)
- Benchmark, experiment, and example docs remain in their original directories and are not reorganized here.
- See `problog-benchmark/README.md` and `examples/running_example/README.md` as entry points.
- Historical benchmark notes remain in `problog-benchmark/README.sc.original.md` and `experiments/smokers_11/Plan.md`.

## Design Notes (Not Implemented)
- [DESIGN] `README.inc.regional.multi.md` — multi-turn inc-regional state
- [DESIGN] `README.inc.regional.opt.md` — inc-regional optimizations
- [DESIGN] `IncRegional_PaperFormal_TechReport.md` — formalization notes
- [DESIGN] `README.elastic.md` — elastic switch ideas
- [DESIGN] `README.eqrel.md` — eqrel pruning concept
- [DESIGN] `README.lazy.md` — lazy DD plan
- [DESIGN] `README.refactor.md` — refactor opportunities

## Archive
- [HISTORICAL] `README.rewrite.120725.md`
- [HISTORICAL] `README.rewrite.120825.md`
- [HISTORICAL] `README.rewrite.2.md`
- [HISTORICAL] `README.rewrite.conj.md`
- [HISTORICAL] `README.rewrite.md`
- [HISTORICAL] `README.precompile.md`
- [HISTORICAL] `README.codex.md`
- [HISTORICAL] `Plan.md`

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)

## Related commits
- `e214cd028` — docs(repo): refresh incremental docs and index
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
- `ce3dd2f5b` — docs(inc-region): add SCC-closure optimization notes
