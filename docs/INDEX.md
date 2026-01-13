# Documentation Index

## Core Entry Points
- `README.md`: user-facing overview and quickstart.
- `CONTRIBUTING.md`: contributor workflow and review checklist.
- `AGENTS.md`: Codex constraints, commands, and pitfalls.

## System Docs
- `docs/ARCHITECTURE.md`: high-level design and key modules.
- `docs/TESTING.md`: verification strategy and CI commands.
- `docs/RUNBOOK.md`: run/rollback/troubleshooting guidance.
- `docs/SECURITY.md`: data handling and dependency hygiene.
- `docs/USAGE.md`: program syntax, CLI, and runtime options.

## Repo Hygiene
- `README.git.md`: commit scope rules and commit message format.

## Research Notes and Design Logs
- `README.rewrite.md`: current rewrite pipeline behavior and timing table.
- `README.rewrite.opt.md`: rewrite optimization notes.
- `README.split.md`: split design, current behavior, and historical observations.
- `README.siso.md`: SISO detection algorithm details.
- `README.evidence.md`: evidence flow and conditioning semantics.
- `README.const.md`: deterministic-first (det-opt) design and FC integration.
- `README.dred.md`: online DRed internals and deletion bottlenecks.
- `README.inc.region.md`: inc-regional pipeline design/profiling notes.
- `README.ordering.md`, `README.eqrel.md`, `README.lazy.md`, `README.refactor.md`: design roadmaps and refactor notes.
- `README.precompile.md`: precompile refactor log (historical, build-related).
- `README.rewrite.120725.md`, `README.rewrite.120825.md`, `README.rewrite.2.md`,
  `README.rewrite.conj.md`, `README.rewrite.impl.md`: historical rewrite notes and alternative plans.
- `README.mst.md` : MST pipeline notes (translator changes, emitRules effects, derivation graph edges

## Evaluation and Profiling
- `README.eval.md`: full-mode evaluation workflows.
- `README.eval.inc.md`: incremental evaluation workflows.
- `README.profile.md`: profiling guidance.
- `README.profile.inc.md`: incremental profiling notes and experiment plan.
- `README.fc.profile.md`: forward-compilation profiling notes and field definitions.
- `README.prune.md`: prune-stage behavior and performance notes.
- `README.fc.delete.md`: incremental forward-compilation deletion logic (det vs non-det).
- `README.problog.md`: ProbLog baseline timing runs (historical).

## Experiments
- `experiments/side_channel_inc_mini/README.md`: mini incremental benchmark.
- `experiments/ground/README.txt`: ground-program frontend tests.
- `experiments/**/run.sh`: per-experiment scripts.

## Agent Context (Historical)
- `README.codex.md`: Codex context dump and code hotspots (historical).
- `Plan.md`: local TODO list and planning notes.
