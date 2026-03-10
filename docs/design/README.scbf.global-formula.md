# SCBF Global Formula Design (Stitched Pointer Graph)

## Status
- Independent prototype.
- Not wired into the default full/inc pipeline.
- Introduces a stitched global SCBF view above the target-local bundle IR.

## Goal
- Keep per-stratum/per-target construction for cycle-breaking and local control.
- After construction, expose one global pointer-connected formula graph so
  rewrite and analysis are not blocked by `import` boundaries.

## Layering
1. `ScbfStratumFormulaBundle`
- Construction fragment IR.
- Built per cycle/target.
- Keeps `local + import + rule` indexing because it is simple to build and
  validate.

2. `ScbfGlobalFormula`
- Stitched global view.
- Resolves each `import` to the canonical upstream target root node.
- Preserves same-stratum target isolation, but cross-stratum dependencies become
  direct references instead of opaque interfaces.

## Files
- API:
  [src/include/souffle/problog/scbf/ScbfGlobalFormula.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfGlobalFormula.h)
- Implementation + smoke:
  [src/problog/scbf/ScbfGlobalFormula.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfGlobalFormula.cpp)

## Core data structures
1. `ScbfGlobalFormulaNode`
- One stitched local-node instance.
- Carries original DG node identity plus cycle/topo/component ownership.

2. `ScbfGlobalFormulaRule`
- Head pointer + body literal pointers/constants.
- No `import` indexing remains at this level.

3. `ScbfGlobalFormulaComponent`
- One target-local fragment after stitching.
- Still owns its local nodes/rules, but imports are resolved to upstream target
  roots.

4. `ScbfGlobalFormula`
- Owns all components, nodes, rules, export-root map, and topo-ordered roots.
- Export references currently admit three states:
  - `node`: exact surviving representative,
  - `constant`: rewritten/precomputed scalar summary,
  - `summary`: shared formula summary for a rewritten-away target,
  - `elided`: fallback only, kept as an escape hatch for unsupported cases.

## Why this exists
- `import` is a construction concern, not a rewrite visibility boundary.
- Bundle-local rewrite can accidentally treat imports like closed leaves.
- The stitched global formula makes cross-stratum structure visible again while
  keeping the bundle builder unchanged.

## Validation and dump support
- `validateScbfGlobalFormula(...)`
- `dumpScbfGlobalFormulaJson(...)`
- `dumpScbfGlobalFormulaDot(...)`

Smoke target:
- `souffle-scbf-global-formula-smoke`
- builds a recursive toy graph, stitches the global formula, validates it, dumps
  JSON/DOT to `/tmp/scbf-global-formula-dump`, and checks that downstream `c`
  points directly to upstream `b`.

## Current limitation
- The stitched global formula is now the preferred rewrite/analyze IR, but the
  experimental evaluator/rewrite prototypes are still only partially migrated.
- The immediate correctness fix in the current bundle-rewrite path is to treat
  imports as opaque external inputs instead of temporary facts.
- The independent global-rewrite prototype now allows rewritten exports to map
  to a surviving node or constant summary instead of pinning every original
  target root as an output anchor. This restores nearly the same rewrite space
  as naive DG rewrite on real acyclic graphs.
- Rebuild now synthesizes shared formula summaries for rewritten-away targets.
  In the benchmarked cases this removes the need for `elided` exports entirely:
  rewritten targets now map to `node`, `constant`, or `summary`.
- `elided` remains only as a safety valve for unsupported cases. It is not the
  desired steady-state representation.
- Timing on real cases now shows the remaining gap is mostly engineering
  overhead around the DG round-trip (`materialize + rebuild + validate`), not
  missed rewrite opportunities inside `GraphRewriter`.

## Source references
- [src/include/souffle/problog/scbf/ScbfGlobalFormula.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfGlobalFormula.h)
- [src/problog/scbf/ScbfGlobalFormula.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfGlobalFormula.cpp)
- [src/include/souffle/problog/scbf/ScbfFormula.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfFormula.h)
- [src/problog/scbf/ScbfFormula.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfFormula.cpp)

## Related commits
- `UNCOMMITTED` — design(scbf-global): add stitched global SCBF formula IR and smoke
- `UNCOMMITTED` — design(scbf-global): add independent global rewrite prototype and benchmark
- `UNCOMMITTED` — design(scbf-global): loosen export anchors to node/constant/elided and expose rewrite timing breakdown
- `UNCOMMITTED` — design(scbf-global): synthesize shared export summaries for rewritten-away targets
