# Inc-Regional Optimization Notes (SCC Closure + DAG)

This note captures analysis and optimization ideas focused on the inc-regional
insert path, specifically the SCC-closure step and its impact on DAG-heavy
benchmarks. For the pipeline overview, see `README.inc.region.md`.

## Scope
- Code paths: `src/include/souffle/problog/RegionalIncremental.h`,
  `src/include/souffle/problog/IncRegionAnalyzer.h`,
  `src/include/souffle/problog/DerivationGraph.h`.
- Benchmark context: P1-P14 side-channel runs (seed0, strengthen, delta ratios
  0.003 / 0.006 / 0.01). Run log is in `README.eval.final.md`.

## DepGraph caching (confirmed)
`DerivationGraphViewInterface::getCycleDependencyGraph()` caches a
`CycleDependencyGraph` in `cachedCycleDependencyGraph_` and returns it on
subsequent calls. The cache is cleared via `clearCycleDependencyGraphCache()`
from structural-reset sites including `invalidateCaches()` and `applyDelta()`.
Net effect: the SCC/dependency graph is cached within a single update, but it is
recomputed per turn because `applyDelta()` clears the cache.

Relevant code:
- `src/include/souffle/problog/DerivationGraph.h`:
  - `getCycleDependencyGraph()` / `cachedCycleDependencyGraph_`
  - `clearCycleDependencyGraphCache()`
  - `invalidateCaches()` and `applyDelta()` call sites

## What SCC-closure does (and what least-parents do not)
- SCC-closure is implemented in `RegionSccClosure::closeToScc()`
  (`RegionalIncremental.h`). It expands the region so that if any node in the
  region belongs to an SCC, all nodes/edges in that SCC (within the
  delta-reachable filter) are also included.
- Least-parents / scope computation in `IncRegionAnalyzer.h` is dominance-based
  and is used to find merge points and boundaries. It does not guarantee that
  the region is SCC-closed. A region can contain a node in an SCC without
  containing every peer in the SCC.

Why SCC-closure is not redundant:
- Example: if A and B are in a cycle (A -> B -> A) and the region only contains
  A, a partial update can leave formulas for B stale even if boundaries appear
  valid. SCC-closure prevents partial-cycle updates.
- Therefore SCC-closure is required for correctness on cyclic graphs even if
  least-parent scopes already cover merge points.
- On a DAG, SCC-closure is a no-op (every SCC is size 1), so the work is pure
  overhead.

## Observations from the P1-P14 benchmark
Data source: `problog-benchmark/side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0/
inc_regional_fc_subcalc.tsv` and per-run JSON logs in `P*/output/`.
Snapshot timestamp: 2026-01-20 12:14 local (see log filenames
`log_inc_regional_profile_*_2026120_1214xx.json`).

- `inc_regional_scc_expanded=0` for all 27 runs where inc-regional insertion
  executed (12 runs had empty insert delta, so no entry).
- Median breakdown of inc-regional insert time (27 runs):
  - analyze: 0.487 ms (35.72%)
  - scc_close: 0.651 ms (47.81%)
  - plan: 0.058 ms (4.67%)
  - rebuild: 0.125 ms (10.23%)
  - calibrate: 0.003 ms (0.21%)
- `inc_regional_total_ms / fc_insert_ms` median: 0.86 (i.e., most FC insert
  time is inc-regional overhead, dominated by analyze + scc_close).

Takeaway: the SCC-closure step is the single largest share of the
inc-regional insert path for this benchmark, yet it never expanded the region.
This is consistent with a mostly-DAG derivation graph or with regions already
SCC-closed.

## DAG-focused optimization ideas
Implementation status (2026-01-20):
- Added a fast-path in `RegionalIncremental.h` that runs a DFS-based cycle check
  from region nodes and skips `getCycleDependencyGraph()` when no region node is
  part of a cycle. This avoids full SCC/dependency/depth computation on DAG-heavy
  workloads while preserving correctness on cyclic graphs (it still falls back
  to the full SCC closure when a cycle is detected).

1) **Skip SCC-closure on acyclic programs**
   - If the Datalog program is non-recursive (no rule SCCs), the derivation
     graph is acyclic for all inputs. In that case SCC-closure can be skipped
     entirely, and the depGraph never needs to be built.
   - Implementation idea: attach an "is_acyclic" flag from compile time to the
     runtime view. When true, bypass `closeToScc()` and any depGraph build.
   - Risk: requires a reliable program-level check.

2) **Run SCC only on the delta-reachable subgraph**
   - `RegionSccClosure::closeToScc()` already limits expansion to the
     delta-reachable filter. Instead of building the full `CycleDependencyGraph`,
     compute SCCs on the induced subgraph of delta-reachable nodes/edges
     (which `IncRegionAnalyzer` already computes).
   - Expected benefit: in DAG-heavy or small-delta workloads, this avoids full
     graph SCC computation and should cut most of the scc_close cost.

3) **Skip SCC-closure when the region cannot intersect a non-trivial SCC**
   - Maintain a cached set/bitset of "cyclic nodes" (nodes in SCCs of size > 1)
     for the current graph. If `region.nodes` has no intersection, skip
     `closeToScc()`.
   - This still needs SCC data once per turn, but if we can cheaply update the
     cyclic-node set across turns (or reuse it across multiple consumers), the
     per-turn cost falls.

4) **Exploit existing cache within a turn**
   - If another stage already computed `getCycleDependencyGraph()` (e.g., debug
     stats or a prior call in the same turn), ensure the inc-regional path reuses
     that cached instance to avoid redundant SCC builds.

## Clarifying the least-parent vs SCC intuition
- Least-parent scopes reason about dominators and merge points (branch joins),
  not about strongly connected cycles.
- An SCC can contain no least-parent boundary nodes and still require full
  inclusion for correct fixpoint evaluation. Therefore SCC-closure is not
  implied by least-parent scopes.
- On DAGs, least-parent scopes and SCC-closure both become simpler; SCC-closure
  becomes a no-op and is a good optimization target.

## Notes / Next Checks
- If we pursue option (1), confirm whether program-level recursion detection is
  already exposed in runtime options; otherwise add a minimal flag.
- If we pursue option (2), reuse `reach_filter_` and `succs_` / `preds_` from
  `IncRegionAnalyzer` to avoid recomputing adjacency for SCC detection.
- If we pursue option (3), store cyclic-node membership in
  `IncrementalDerivationGraphViewInterface` and invalidate it alongside the
  depGraph cache.
