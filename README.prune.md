# Prune Stage Notes (Full + Incremental)

## Status
- Active note. Summarizes prune behavior and known performance drivers.

## Scope
- Applies to `DerivationGraph::prune` (full) and `IncrementalDerivationGraph::prune` (inc).
- Focus is on correctness constraints and why PRUNING_INC is often the bottleneck.

## Related docs
- `README.profile.inc.md` (timings and profiling data).
- `README.inc.region.md` (impacted-map rebuild and delta-reach cache usage).
- `README.eval.inc.md` (benchmark tables and stage breakdowns).
- `README.dred.md` (SEM stage context).

## Code map
- `src/include/souffle/problog/DerivationGraph.h`: full/inc `prune()` implementations.
- `src/include/souffle/problog/DerivationGraph.h`: impacted-map rebuild and delta-reach cache.
- `src/include/souffle/problog/RegionalIncremental.h`: inc-regional consumes impacted maps.

## What prune computes
- A subgraph reachable from output relations and evidence nodes (backward reachability).
- Node/edge `pruned` flags and a live subset (`liveNodes`, `liveEdges`).
- Outputless-component pruning (removes disconnected components after reachability).
- Delta-delete filtering: retain only deltas still pruned after reachability.
- For inc: build `IncSubgraphView` (delta sets + impacted maps + delta-reach cache).
- For full: optional bi-imp merge (when enabled) and cleanup.

## Why prune is expensive today
- It is full-graph work per turn: reachability, mark/prune, and outputless pruning touch
  most of the graph even when deltas are tiny.
- Impacted-map rebuild is heavy: current logic does BFS per delta node/fact, so worst-case
  cost is O(|delta_nodes| * (|V|+|E|)). This dominates insert turns on large cases.
- Building the view materializes large sets/maps (live/delta/impacted), which is costly
  when the graph is large or delta sets are large.
- Incremental mode disables bi-imp merge (see below), so graphs are typically larger
  than the full-mode view, amplifying prune and downstream costs.
- Optional `dumpStatisticsInc` and other debug output adds overhead if enabled.

## Impacted-map inefficiency (current root cause)
- The impacted-map rebuild walks the graph from each delta node to identify affected
  nodes/edges. This repeats work across overlapping BFS frontiers.
- The maps are built unconditionally in inc mode, even if inc-regional is not used.
- See `README.inc.region.md` for the exact cache usage and data flow.

## Recent changes (2026-01-11)
- Delete impact BFS now only sources from explicit deleted facts (not all delta-deleted nodes).
- Insert impact maps (and delta-reach cache) are skipped in inc-naive; they are only
  built when inc-regional is enabled.

## Update log (2026-01-12, trimmed ruleset, det-opt, P17-P20, inc1/inc3/inc5)
- Correctness: all deltas OK (max|Δ|=0).
- PRUNING_INC insert turns dropped sharply after skipping insert impact maps:
  avg PRN across P17-P20 moved from ~6.4/9.5/9.7s to ~1.06/1.07/1.25s for inc1/inc3/inc5
  (≈6.0x/8.9x/7.8x faster).
- PRUNING_INC delete turns are roughly flat to slightly slower:
  avg PRN across P17-P20 moved from ~0.58/0.50/0.54s to ~0.63/0.67/0.68s.
- Remaining bottleneck: insert FC dominates (e.g., P20 inc3/inc5 insert FC still ~65–68s).

## Correctness constraints: bi-imp merge
- In multi-turn incremental mode (non full-only), bi-imp merge must remain disabled.
  Merging changes node identity and invalidates derivation tracking across turns.
- It is not sufficient to check only the current turn; if the pipeline has multiple
  turns, enabling merge can break delta correctness.
- Any attempt to re-enable merge in inc must prove that delta bookkeeping remains
  valid across turns (not currently established).

## Potential fixes (open)
- Multi-source BFS for impacted maps (replace per-delta BFS).
- Lazy impacted-map build: only compute when inc-regional is enabled.
- Incremental reachability (ref-counts or dynamic reachability) to avoid full rescans.
- Cache live/pruned state and avoid full relabeling on tiny deltas.
- Faster edge deletion structures to reduce applyDeltaDeletes overhead.
- Elastic switch: if delta ratio is large, fall back to full recompute.
- Gate `dumpStatisticsInc` behind a debug option to avoid cost in perf runs.

## Data references
- Timing breakdowns and examples are in `README.profile.inc.md`.
