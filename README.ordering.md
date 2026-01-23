# Correlation-Driven Variable Ordering (Implementation Notes)

## Status
- `BDDForceHeuristics` is implemented but not wired into `CuddManager`.
- `CuddManager` currently enables adaptive dynamic reordering and explicitly skips static ordering.
- `BDDForceHeuristics::compute()` builds a static order via block heuristics; the FORCE/event path exists
  but is not invoked by `compute()`.

## Source references
- [src/include/souffle/problog/formula/GraphHeuristics.h](src/include/souffle/problog/formula/GraphHeuristics.h)
- [src/include/souffle/problog/formula/CuddManager.h](src/include/souffle/problog/formula/CuddManager.h)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)

## Current ordering pipeline (when `BDDForceHeuristics::compute()` is called)

### 1) Variable universe
- Variables are only probabilistic facts (`p < 1.0`) and probabilistic rule edges (`p < 1.0`).
- Deterministic nodes/edges never become variables; they only propagate dependency sets.
- Variables are assigned stable IDs based on tuple/edge-key ordering.

### 2) Support propagation
- A fixpoint computes support sets for every node and edge.
- Negated inputs are ignored when propagating supports.
- Supports are not merged across direct recursion within the same SCC (to avoid self-dependence loops).
- A hard cap of 2000 iterations guards the fixpoint loop.

### 3) Block-based ordering
- If `force_person_blocks` is true, variables are grouped by person using `stress(...)` and
  `influences(..., ...)` tuple patterns.
- Otherwise, a "smokes"-specific adjacency graph is built:
  - For each `smokes(...)` node, gather its immediate probabilistic inputs (facts and parent edges).
  - Link variables that co-occur in these inputs, then BFS over that adjacency (stable fallback order).
- If the adjacency graph is empty, the order falls back to the stable baseline.

## FORCE/event path (present but unused by `compute()`)
- `buildEvents()` emits attractive OR/AND events, correlation anchors/pulls/groups, and repulsive events
  between per-input support blocks (negated inputs are skipped).
- `forceFree()` iterates a FORCE relaxation where repulsive events subtract span and push variables away
  using `repel_push`.
- These routines are currently not invoked by `compute()`, so the related weights in `Params` are unused
  unless the FORCE path is explicitly integrated.

## Params notes
- `w_and`, `w_or`, `w_corr_*`, `w_anti`, `repel_push`, and `force_iters` are consumed by the FORCE/event
  implementation.
- `block_support_min_size`, `block_support_max_size`, and `block_max_emit_size` are defined but are not
  referenced elsewhere in the current implementation.

## Integration status (CUDD)
- `CuddManager::preConfig()` logs "skip static ordering" and enables adaptive dynamic reordering.
- If static ordering is re-enabled, it should apply a `BDDForceHeuristics` order (likely via
  `Cudd_ShuffleHeap`) and document the configuration toggle.

## Related commits
- `4c4bd26b2` — docs(readme): restructure online incremental docs
- `337b2b398` — trivial
