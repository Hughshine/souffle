# Prune Stage Notes (Full + Incremental)

## Source references
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)
- [src/include/souffle/problog/RegionalIncremental.h](src/include/souffle/problog/RegionalIncremental.h)


## Status
- Active note. Summarizes prune behavior and known performance drivers.

## Scope
- Applies to `DerivationGraph::prune` (full) and `IncrementalDerivationGraph::prune` (inc).
- Focus is on correctness constraints and why PRUNING_INC is often the bottleneck.

## Related docs
- `docs/topics/profiling/README.profile.inc.md` (timings and profiling data).
- `docs/topics/pipeline/README.inc.region.md` (delta-reach fallback chain and inc-regional expectations).
- `docs/topics/evaluation/README.eval.inc.md` (current incremental workflow).
- `docs/topics/pipeline/README.dred.md` (SEM stage context).

## Code map
- `src/include/souffle/problog/DerivationGraph.h`: full/inc `prune()` implementations.
- `src/include/souffle/problog/DerivationGraph.h`: impacted-map and delta-reach plumbing (currently empty in prune-inc).
- `src/include/souffle/problog/RegionalIncremental.h`: inc-regional consumes impacted maps.

## What prune computes
- A subgraph reachable from output relations and evidence nodes (backward reachability).
- Node/edge `pruned` flags and a live subset (`liveNodes`, `liveEdges`).
- Outputless-component pruning (removes disconnected components after reachability);
  gated by `--prune-extra` (default off, see `docs/USAGE.md`).
- Delta-delete filtering: retain only deltas still pruned after reachability.
- For inc: build `IncSubgraphView` (delta sets + impacted maps + delta-reach cache).
- For full: optional bi-imp merge (when enabled) and cleanup.

## Why prune is expensive today
- It is full-graph work per turn: reachability, mark/prune, and outputless pruning touch
  most of the graph even when deltas are tiny.
- Building the view materializes large sets/maps (live/delta/impacted), which is costly
  when the graph is large or delta sets are large.
- Incremental mode disables bi-imp merge (see below), so graphs are typically larger
  than the full-mode view, amplifying prune and downstream costs.
- Optional `dumpStatisticsInc` and other debug output adds overhead if enabled.

## Impacted-map status (current code)
- `prune-inc` currently initializes impacted maps and delta-reach caches as empty;
  the analyzer therefore falls back to BFS over the live subgraph (see `docs/topics/pipeline/README.inc.region.md`).
- If impacted-map rebuild is reintroduced, prefer multi-source BFS or a union cache to
  avoid O(|delta_nodes| * (|V|+|E|)) behavior.

## Historical notes (2026-01-11)
- These notes refer to an earlier branch where prune-inc rebuilt impacted maps.
  The current code leaves impacted maps empty and uses analyzer BFS fallback.

## Historical update log (2026-01-12, trimmed ruleset, det-opt, P17-P20, inc1/inc3/inc5)
- Correctness: all deltas OK (max|Δ|=0).
- PRUNING_INC insert turns dropped sharply after skipping insert impact maps:
  avg PRN across P17-P20 moved from ~6.4/9.5/9.7s to ~1.06/1.07/1.25s for inc1/inc3/inc5
  (≈6.0x/8.9x/7.8x faster).
- PRUNING_INC delete turns are roughly flat to slightly slower:
  avg PRN across P17-P20 moved from ~0.58/0.50/0.54s to ~0.63/0.67/0.68s.
- Remaining bottleneck: insert FC dominates (e.g., P20 inc3/inc5 insert FC still ~65–68s).

## Historical update log (2026-01-12, delete impact union BFS, trimmed ruleset, det-opt, P17-P20, inc1/inc3/inc5)
- Change: delete impact maps are now computed as **union BFS** for deterministic and non-deterministic deleted facts
  (per-fact BFS removed); non-deterministic conditioning is applied once per impacted node/edge using the full
  deleted-var list.
- Correctness: all deltas OK (max|Δ|=0).
- PRUNING_INC delete turns improved: avg PRN across P17-P20 moved from ~0.62/0.67/0.68s to ~0.46/0.45/0.45s
  for inc1/inc3/inc5 (≈1.3–1.5x faster).
- PRUNING_INC insert turns also improved modestly (avg ~1.04/1.07/1.25s → ~0.77/0.85/1.03s).

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
- Timing breakdowns and examples are in `docs/topics/profiling/README.profile.inc.md`.

## Related commits
- `80232d555` — docs(eval): refresh inc benchmark notes
- `dc4024f87` — perf(prune): skip insert impacts in inc-naive
