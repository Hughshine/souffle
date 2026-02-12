# FC Deletion Notes (Incremental)

## Source references
- [src/include/souffle/problog/ForwardCompilation.h](src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)


## Scope
- Focus: forward compilation (formula update) during incremental deletion.
- Main entry point: `buildFormulasIncCyclewise` in `src/include/souffle/problog/ForwardCompilation.h`.
- Regional path: `buildFormulasIncRegionalCyclewise` in `src/include/souffle/problog/ForwardCompilation.h`.
- This note does not cover SEMI-NAIVE/DRed fact maintenance.

## Deletion pipeline (cyclewise/regional, current)
1) Clear deleted formulas and invalid entries.
   - Remove formulas for `deltaDeletedNodes` and `deltaDeletedEdges`.
   - Drop formulas for nodes/edges not in `view.getValidNodes()` / `view.getValidEdges()`.
2) Build deleted-fact sets and partition deterministic vs non-deterministic.
   - `view.getDeletedFacts()` currently reflects explicit deleted facts only (impacted maps are not merged).
3) Impact analysis with deleted-edge traversal.
   - BFS from deleted facts across live edges plus a temporary adjacency for `deltaDeletedEdges`.
   - Produces `detImpact{Nodes,Edges}` and `nonDetImpact{Nodes,Edges}`.
4) Non-det conditioning (pre-shrink).
   - Compute `nonDetOnly = nonDetImpact \\ detImpact`.
   - Apply `makeCondition` to `nonDetOnly` nodes/edges and enqueue affected edges.
5) Variable ordering update (delete side).
   - Run `postprocessUselessVariables` on deleted non-det variables.
6) Det over-delete + rederive fixpoint.
   - Set det-impacted nodes/edges to False and seed SCC worklists.
   - During rederive, apply `makeCondition` to any edge/node in `nonDetImpact` before storing.
7) Insert phase (unchanged).

## Why deleted-edge traversal matters
- Deleted facts may not be connected via live edges after pruning; without traversing deleted edges,
  impact BFS can miss nodes that only lose derivations.

## Deterministic vs non-deterministic behavior
### Non-deterministic facts
- Conditioning is exact when formulas include the relevant random variables.
- Requires impacted-node/edge sets to avoid recomputing the whole graph; the inputs are the deleted
  facts and their reachable region in the view.

### Deterministic facts
- With det-opt enabled, deterministic facts are not represented in formulas, so conditioning is not
  possible.
- Current solution: over-delete impacted formulas to False, then rederive via SCC worklists.
- This is correct but can be expensive if the impacted region is large.

## Possible simplifications for deterministic deletions
- Option A (simplest semantics): disable det-opt in incremental mode so deterministic facts become
  explicit variables (prob=1). Then deletion can use the same conditioning path as non-det.
  Tradeoff: larger formulas and potentially slower FC/WMC.
- Option B (counting support): track derivation counts for deterministic nodes/edges. On delete,
  decrement counts; only when the count hits zero do we propagate changes. This avoids full rederive
  but needs per-derivation bookkeeping.
- Option C (minimal support): store a single supporting derivation per det node and re-prove only
  when that support is deleted. This keeps formulas small but still requires derivation metadata.

## TODO direction
- Align the non-cyclewise incremental path (`buildFormulasInc`) with the cyclewise/regional logic.

## Related commits
- `80232d555` — docs(eval): refresh inc benchmark notes
