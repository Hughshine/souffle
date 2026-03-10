# SCBF Formula Bundle Design (Independent)

## Status
- Independent prototype.
- Not wired into default full/inc pipeline.
- Built to make SCBF formula structure explicit (instead of planner-only metadata).
- Consumed by independent evaluator path (`evaluateScbfProgramProbabilityViaFormulaBundle`).
- This bundle IR is now treated as a construction-layer fragment, not the final
  global rewrite scope. See `README.scbf.global-formula.md`.

## Goal
- Add an explicit target-local SCBF fragment data structure with:
  - target-local equations,
  - import references to earlier strata only,
  - no same-stratum cross-target sharing.

## Files
- API:
  [src/include/souffle/problog/scbf/ScbfFormula.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfFormula.h)
- Implementation + smoke:
  [src/problog/scbf/ScbfFormula.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfFormula.cpp)

## Data structures
1. `ScbfStratumFormulaBundle`
- One SCC/stratum bundle.
- Contains `targets` and policy flag `allowCrossTargetSharing=false`.

2. `ScbfTargetFormula`
- One target gets its own local equation arena.
- Contains:
  - `localNodes`,
  - `imports`,
  - `localNodeRules` (head-indexed rules),
  - `targetLocalIndex`.

3. `ScbfRuleEquation` / `ScbfLiteralRef`
- Rule stores edge weight + body literals.
- Literal source is one of:
  - local node,
  - import node,
  - constant.

## Builder
- `buildScbfStratumFormulaBundle(view, program, cycleId)`
- For each target in `buildScbfFormulaArenaPlan`:
  - backward-slice through same-cycle incoming edges,
  - collect local nodes/edges used by that target,
  - record non-local predecessors as imports,
  - encode edge bodies as literal refs.

This enforces independent per-target arenas and keeps shared values only through
earlier-stratum imports.

## Validation
- `validateScbfStratumFormulaBundle(...)` checks:
  - cycle/topo consistency,
  - target index correctness,
  - local/import index bounds,
  - rule head/output consistency,
  - probability range checks.

## Smoke
- target: `souffle-scbf-formula-smoke`
- builds a toy recursive graph, builds SCBF program + formula bundle, validates
  invariants, and checks the downstream target imports are present.

## Source references
- [src/include/souffle/problog/scbf/ScbfFormula.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfFormula.h)
- [src/problog/scbf/ScbfFormula.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfFormula.cpp)
- [src/include/souffle/problog/scbf/ScbfIr.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfIr.h)
- [src/problog/scbf/ScbfIr.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfIr.cpp)

## Related commits
- `UNCOMMITTED` — design(scbf-formula): add explicit target-local SCBF formula bundle IR and smoke
