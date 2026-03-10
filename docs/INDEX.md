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
- [PRIMARY] `docs/project/README.md` — project map (modules, fork delta, maintenance)
- [PRIMARY] `docs/topics/README.md` — topical docs map (current behavior)
- [PRIMARY] `docs/design/README.md` — design proposals map
- [PRIMARY] `docs/historical/README.md` — archived/historical docs map
- [SECONDARY] `docs/topics/runtime/README.flag.md` — consolidated compiler/runtime flag reference
- [SECONDARY] `docs/topics/runtime/README.dump.md` — dump/debug outputs (compiler + runtime)
- [PRIMARY] `docs/TESTING.md` — verification status + test commands
- [SECONDARY] `docs/topics/testing/README.regression.md` — maintained regression suite details and case map
- [PRIMARY] `docs/RUNBOOK.md` — build/run/rollback/troubleshooting
- [PRIMARY] `docs/ARCHITECTURE.md` — system architecture map
- [PRIMARY] `docs/process/README.git.md` — commit hygiene
- [SECONDARY] `docs/process/README.md` — process docs map
- [SECONDARY] `CONTRIBUTING.md` — contributor workflow
- [PRIMARY] `AGENTS.md` — Codex constraints (kept at repo root)

## Project-Level Docs
- [PRIMARY] `docs/project/MODULES.md` — whole-project module inventory
- [PRIMARY] `docs/project/FORK_DELTA.md` — fork-owned modules and contributions
- [PRIMARY] `docs/project/MAINTENANCE.md` — invariants, risks, and module-level checks
- [PRIMARY] `docs/project/PROBLOG_EXTENSION_STACK.md` — end-to-end ProbLog extension implementation details
- [PRIMARY] `docs/project/DOC_SYSTEM.md` — documentation architecture and placement rules

## Current Pipeline & Semantics
- [PRIMARY] `docs/topics/pipeline/README.dred.md` — online DRed semantics + performance
- [PRIMARY] `docs/topics/pipeline/README.inc.region.md` — inc-regional implementation
- [PRIMARY] `docs/topics/pipeline/README.prune.md` — prune behavior + constraints
- [PRIMARY] `docs/topics/pipeline/README.fc.delete.md` — incremental delete logic
- [PRIMARY] `docs/topics/pipeline/README.evidence.md` — evidence semantics and flow
- [PRIMARY] `docs/topics/pipeline/README.const.md` — det-opt behavior
- [SECONDARY] `docs/topics/pipeline/README.souffle.opt.md` — AST/RAM transform inventory

## Rewrite & Graph Transforms
- [PRIMARY] `docs/topics/rewrite/README.rewrite.impl.md` — current rewrite behavior
- [PRIMARY] `docs/topics/rewrite/README.derivation.analyzer.md` — standalone derivation JSON analyzer and interactive viewer
- [SECONDARY] `docs/topics/rewrite/README.split.md` — split behavior + design notes
- [SECONDARY] `docs/topics/rewrite/README.siso.md` — SISO detection details
- [SECONDARY] `docs/topics/rewrite/README.mst.md` — magic-set (MST) translator notes
- [DESIGN] `docs/design/README.rewrite.opt.md` — optimization ideas (not implemented)

## Backends
- [PRIMARY] `docs/topics/backends/README.cudd.md` — BDD backend
- [SECONDARY] `docs/topics/backends/README.cudd.reordering.md` — reordering details
- [SECONDARY] `docs/topics/backends/README.sdd.md` — SDD backend status/gaps
- [DESIGN] `docs/design/README.ordering.md` — ordering implementation notes (FORCE path exists but is unused by CUDD)

## Profiling
- [PRIMARY] `docs/topics/profiling/README.profile.md` — profiling guide (online)
- [SECONDARY] `docs/topics/profiling/README.profile.inc.md` — incremental profiling notes
- [SECONDARY] `docs/topics/profiling/README.fc.profile.md` — FC profiling fields

## Testing & Validation
- [PRIMARY] `docs/TESTING.md` — testing status, command entry points, and policy
- [SECONDARY] `docs/topics/testing/README.regression.md` — regression suite scope, cases, and limits

## Evaluation & Results (Souffle)
- [PRIMARY] `docs/topics/evaluation/README.md` — evaluation doc map (current vs historical)
- [LATEST] `docs/topics/evaluation/README.eval.final.md` — latest curated batch summary
- [LATEST] `archive/README.md` — dated experiment archive index (batch READMEs tracked, artifacts ignored)
- [PRIMARY] `docs/topics/evaluation/README.eval.inc.md` — incremental benchmark workflow (current)
- [PRIMARY] `docs/topics/evaluation/README.eval.md` — full benchmark workflow (current)
- [SECONDARY] `docs/topics/evaluation/OPT.md` — optimization-focused inc-regional run checklist
- [SECONDARY] `docs/topics/evaluation/README.artifact.inc.md` — incremental artifact reproduction
- [HISTORICAL] `docs/historical/README.eval.inc.log.md` — archived incremental run log
- [HISTORICAL] `docs/historical/README.eval.full.log.md` — archived full-mode run log
- [HISTORICAL] `docs/historical/README.eval.final.2026-01-21.md` — archived detailed incremental snapshot
- [HISTORICAL] `docs/historical/OPT.md` — archived optimization notebook
- [HISTORICAL] `docs/historical/README.table.md` — table generation notes
- [HISTORICAL] `docs/historical/README.disjunct.md` — disjunction strengthening log
- [HISTORICAL] `docs/historical/README.problog.md` — ProbLog baseline runs

## Out of Scope (Benchmarks/Experiments/Examples)
- Benchmark, experiment, and example docs remain in their original directories and are not reorganized here.
- See `problog-benchmark/README.md` and `examples/running_example/README.md` as entry points.
- Historical benchmark notes remain in `problog-benchmark/README.sc.original.md` and `experiments/smokers_11/Plan.md`.

## Design Notes (Not Implemented)
- [DESIGN] `docs/design/README.approx.pipeline.md` — approximate probabilistic pipeline (DG -> query formula -> AMC/WAMC)
- [DESIGN] `docs/design/README.scbf.pipeline.md` — SCBF pipeline (DG -> SCBF -> rewrite -> DD/WMC)
- [DESIGN] `docs/design/README.scbf.ir.md` — independent SCBF IR/module prototype and smoke workflow
- [DESIGN] `docs/design/README.scbf.evaluator.md` — independent SCBF evaluator prototype and smoke workflow
- [DESIGN] `docs/design/README.scbf.formula.md` — explicit SCBF target-local formula bundle IR
- [DESIGN] `docs/design/README.scbf.global-formula.md` — stitched global SCBF formula IR above target-local bundles
- [DESIGN] `docs/design/README.scbf.rewrite.md` — independent SCBF formula-level rewrite prototype
- [DESIGN] `docs/design/README.inc.regional.multi.md` — multi-turn inc-regional state
- [DESIGN] `docs/design/README.inc.regional.opt.md` — inc-regional optimizations
- [DESIGN] `docs/design/IncRegional_PaperFormal_TechReport.md` — formalization notes
- [DESIGN] `docs/design/README.elastic.md` — elastic switch ideas
- [DESIGN] `docs/design/README.eqrel.md` — eqrel pruning concept
- [DESIGN] `docs/design/README.lazy.md` — lazy DD plan
- [DESIGN] `docs/design/README.refactor.md` — refactor opportunities

## Archive
- [HISTORICAL] `docs/historical/PAPER.md` — paper table/figure working note (archived)
- [HISTORICAL] `docs/historical/FINAL.md` — FC-only speedup snapshot note (archived)
- [HISTORICAL] `docs/historical/TODO.md` — archived experiment follow-ups
- [HISTORICAL] `docs/historical/README.rewrite.120725.md`
- [HISTORICAL] `docs/historical/README.rewrite.120825.md`
- [HISTORICAL] `docs/historical/README.rewrite.2.md`
- [HISTORICAL] `docs/historical/README.rewrite.conj.md`
- [HISTORICAL] `docs/historical/README.rewrite.md`
- [HISTORICAL] `docs/historical/README.precompile.md`
- [HISTORICAL] `docs/historical/README.codex.md`
- [HISTORICAL] `docs/historical/Plan.md`
- [HISTORICAL] `docs/historical/README.eval.inc.log.md`
- [HISTORICAL] `docs/historical/README.eval.full.log.md`
- [HISTORICAL] `docs/historical/README.eval.final.2026-01-21.md`
- [HISTORICAL] `docs/historical/OPT.md`

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [tests/regression/CMakeLists.txt](tests/regression/CMakeLists.txt)

## Related commits
- `UNCOMMITTED` — docs(rewrite): index standalone derivation analyzer guide
- `UNCOMMITTED` — docs(testing): add topical regression workflow docs and index entries
- `UNCOMMITTED` — docs(project): add project-level module map, fork delta, and maintenance docs
- `UNCOMMITTED` — docs(project): add detailed ProbLog extension stack documentation
- `UNCOMMITTED` — docs(process): move commit hygiene guide under docs/process
