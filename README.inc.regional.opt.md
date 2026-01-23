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
  Additional runs (2026-01-21): `side_channel_inc_strengthen_fresh` P16–P20
  insert-turn analysis (see `README.eval.final.md`).

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

## P16–P20 insert-turn observations (2026-01-21)
Data source: `problog-benchmark/side_channel_inc_strengthen_fresh/P*/output/delta-*.json`.

Key takeaways:
- **FC insert is usually faster** than inc‑naive (notably P16/P20), but
  **WMC is consistently slower** for inc‑regional, often dominating total time.
- Within inc‑regional **analyze**, the top costs are `reexpand`, then `prepare`
  and `expand`. The pattern worsens with larger deltas (P19 inc5).

Top analyze components (ms; most expensive per case):
- P16: reexpand 36–72, prepare 40–55, expand 24–48
- P17: reexpand 85–183, prepare ~47, expand 6–42
- P18: reexpand 245–346, prepare 67–87, expand 0–64
- P19: reexpand 473–752, prepare 85–136, expand 10–272
- P20: prepare 114–128 dominates inc1; reexpand 168–179 dominates inc3/inc5

## Boundary / Anchor observations (stdout trace)
Data source: `problog-benchmark/side_channel_inc_strengthen_fresh/P15/
stdout_inc_regional_region_trace12.txt` (mtime 2026-01-20 01:41:26 local).
The trace is produced by `--profile-inc-regional`, which logs boundary-head
anchor checks plus per-head anchor lists. Use `--profile-inc-regional-heavy`
to add backward-closure trace details for `--inc-regional-trace-tuples`.
This is a *single-case* snapshot to understand what boundary heads and anchors
look like in practice; it is not a full benchmark summary.

Summary (unique heads):
- Boundary heads (unique): 769
  - With anchors: 272 (all `RAND(*)` heads)
  - Missing anchors: 497 (`RAND(*)` 257 + `KEY_IND(*)` 240)
- Missing-anchor reasons:
  - `all_incoming_edges_deterministic`: 257 (all `RAND(*)`)
  - `head_is_output_or_evidence`: 240 (all `KEY_IND(*)`)
- Anchors (total 1572): node anchors 767 + edge anchors 805
  - Node anchors are all `RAND(*)` facts
  - Edge anchors always output `RAND(*)`, and the edge patterns are
    `xor_assign_left/xor_assign_right/BV_DIFF_REC/RAND`

Anchor-check failures (top reasons for rejected candidates):
- `edge_deterministic`: 1230
- `input_fact_prob1`: 1230
- `input_edge_deterministic`: 663
- `head_output_or_evidence`: 240

Interpretation:
- Boundary heads in this trace are almost entirely `RAND(*)` and `KEY_IND(*)`.
- `KEY_IND(*)` heads are filtered out as anchors because they are treated as
  output/evidence and thus not anchor-safe.
- `RAND(*)` heads fail mostly because their incoming candidates are deterministic
  or probability-1 facts, matching the anchor filters.

## Implementation status (2026-01-21)
- **Local dep-graph**: build the dependency/SCC graph from the delta‑reachable
  subgraph, falling back to the full graph only when reach‑edges are empty.
- **DAG fast‑path**: skip SCC‑closure if the region does not touch a cycle
  (DFS-based check) to avoid full SCC work on DAG‑heavy workloads.
- **Anchor hygiene**: anchors inside the region are rejected to avoid calibrating
  on rebuilt formulas.
- **Output handling**: removed delta→output slice expansion; WMC now chooses
  between (a) rebuilt BDD + original weights for region outputs, (b) old BDD +
  calibrated weights for delta‑reachable outputs outside the region, (c) cached
  value if outside delta‑reach with unchanged evidence.
- **Fallback safety**: snapshot/restore region formulas before falling back to
  classic cyclewise rebuild so partial regional changes do not leak.

## Boundary overlap closure (WIP, 2026-01-22)
Motivation: inc‑regional mismatches appear when a delta‑reachable node outside
the region can reach **multiple boundary heads** that are *not independent*.
In that case, per‑boundary calibration treats correlated updates as independent.

Current conservative overlap rule (no owners):
- **Any** overlap between boundary backward‑reach sets is treated as dependent.
- This is conservative but avoids missing dependencies when the owners heuristic
  is incomplete (e.g., merge‑only regions without a branching source).

Implementation notes (current):
- Run a **multi‑source backward BFS** on DR (allow traversal through region
  nodes) starting from boundary heads.
- For each node, record the first boundary that reaches it; if another boundary
  reaches the same node, mark it as **multi‑boundary** (overlap).
- Add overlap nodes to the region, then **forward‑expand** from all multi‑boundary
  nodes within DR (pulls in downstream effects so region formulas are consistent).
- After expansion, rerun input‑closure + SCC‑closure, recompute boundaries/anchors,
  and retry plan building.

Forward‑expand definition:
- For each multi‑boundary node, follow outgoing edges within DR and include all
  reachable nodes/edges in the region. This is intentionally conservative and can
  push region close to full DR.

Loop structure (single monotone fixpoint):
1) (If region changed) input‑closure + SCC‑closure; recompute boundaries/anchors.
2) Dependent overlap closure (as above); if changes, loop.
3) Build plan; if missing anchors, expand from failed boundaries and loop.

Fallback refinement:
when expand attempts exceed the cap, **only add the DR connected component(s)**
containing the failed boundaries (undirected connectivity over DR edges),
instead of expanding to the full delta‑reachable region.

## DAG-focused optimization ideas

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

## Idea: reuse dep-graph across insert (no implementation yet)
Insert FC is monotonic with respect to the graph after the delete stage in the
same turn: it only **adds** nodes/edges. This raises a potential optimization:
if a dep-graph already exists for the *post-delete* graph, could we apply the
insert delta directly to that dep-graph rather than invalidating and rebuilding?

Feasibility notes:
- The existing `CycleDependencyGraph` is a full SCC + dependency + component +
  depth structure. Edge inserts can *merge* SCCs, update dependencies, and
  change depths; there is no incremental update path today.
- Reusing the **previous turn** dep-graph is not safe without handling deletes,
  because deletions can **split** SCCs. Dynamic SCC maintenance with deletions
  is much more complex than insert-only updates.
- Reusing the **post-delete** dep-graph *within the same turn* is more plausible
  (insert-only), but would still require implementing dynamic SCC/condensation
  updates plus component/depth recomputation. This is a non-trivial algorithmic
  change.
- A lighter-weight alternative is to keep the current "local dep-graph from
  deltaReachable" approach, which already shrinks the graph size. Incremental
  updates might not pay off unless dep-graph dominates overall time.

Bottom line: the idea makes sense conceptually for insert-only phases, but
implementing a correct incremental update of SCC/dependency/depth is not a
small change. It is likely only worth doing if dep-graph construction remains
the dominant cost after local-subgraph optimization.

### Clarification: reuse across turns when a turn has no deletes
If a turn performs **only inserts**, then the dep-graph after the previous
turn’s fixpoint is still a valid starting point. In principle, we could carry
that dep-graph forward and apply the insert delta incrementally. This avoids a
full rebuild in insert-only turns.

Key requirements / risks:
- **Detect delete vs insert:** `applyDelta()` currently invalidates caches for
  any delta. To reuse across turns we need a distinction between “insert-only”
  and “delete/other,” and only keep the dep-graph in the insert-only case.
- **Incremental SCC maintenance:** inserting edges can **merge SCCs**. We need
  dynamic SCC update on a directed graph. This is not a small change and is
  more complex than a simple union-find.
- **Condensation / dependencies / depths:** `CycleDependencyGraph` also stores
  dependencies and depth metadata. Merging SCCs invalidates these, so we need
  a correct incremental recomputation strategy for these structures too.
- **Cross-user correctness:** dep-graph is used by multiple paths (forward
  compilation, rewriter, inc-regional, etc.). Any incremental update must
  preserve all invariants, not just SCC closure for inc-regional.

Feasibility outlook:
- **Possible, but substantial:** implementing an incremental SCC + dependency
  update for insert-only turns is possible in theory but likely larger than the
  current optimization scope. If we pursue this, it should be justified by
  profiling showing dep-graph rebuild is still dominant after local-subgraph
  optimization.

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

## 2026-01-22 benchmark findings (det-opt, side_channel_inc_strengthen_fresh)
Summary from `README.eval.final.md` (no-profile timing + profile breakdown):
- **Insert FC slowdown vs inc-naive.** Average insert-turn speedup
  (inc-naive / inc-regional) ≈ **0.89×** → inc-regional is typically slower.
- **Region growth is large.** Average `R/DR` after analyze ≈ **0.588**, final
  `R/DR` ≈ **0.771** (≈ **1.38×** growth). Large cases (P15–P20) expand to
  **~0.99–1.00** of delta-reachable.
- **Expansion drivers.** Overlap-closure + forward-expand + close-inputs fired
  in nearly all runs; missing-anchor expansion appeared in larger cases
  (P15–P20) and contributes major node growth.
- **Insert timing breakdown (profile).** Average inc-regional insert time is
  dominated by **analyze (~75%)**, then **rebuild (~18%)**, with plan/calibrate
  low single digits. Inside rebuild, **rebuildLoop ~82%** of rebuild time;
  depGraph cost is ~0 (delta-reach subgraph).

Implications for optimization focus:
- **Reduce region growth**: overlap closure / forward-expand + close-inputs are
  the largest drivers of “near-full” regions → dial back or make dependency
  checks more precise so regions don’t collapse into DR.
- **Anchor scarcity drives expansion** in big cases → expand anchor candidate
  set or relax constraints so we avoid `expand_missing_anchor` fallback.
- **Analyze cost dominates**: cache/reuse boundary + anchor computations across
  expansion attempts, and avoid repeated full recompute when only a few nodes
  were added.
