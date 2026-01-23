# Inc-Regional Incremental Pipeline (Current Design)

## Source references
- [src/include/souffle/problog/RegionalIncremental.h](src/include/souffle/problog/RegionalIncremental.h)
- [src/include/souffle/problog/IncRegionAnalyzer.h](src/include/souffle/problog/IncRegionAnalyzer.h)
- [src/include/souffle/problog/ForwardCompilation.h](src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)


This file describes the current inc-regional incremental pipeline implementation. It follows the code and is not a planning document.

## Status
- Active design note; matches current implementation.

## Scope / Context
- Applies only to online incremental binaries (default; `--online` optional); the old `--inc` backend is removed.
- inc-regional uses a dedicated deletion/update pass inside `buildFormulasIncRegionalCyclewise`;
  derivation-graph deltas still use DRed-style `applyDelta` before pruning.
- If rewrite runs in the full pass, the incremental CLI is disabled; inc/inc-regional runs themselves do not
  execute rewrite.

## Related docs
- `README.eval.inc.md` (benchmark procedures + logs)
- `README.dred.md` (online DRed internals and deletion bottlenecks)

## Assumptions
- Commit runs DRed-style incremental evaluation (`program->runAllInc`) and `graph->applyDelta(...)` before prune;
  the inc-regional forward-compilation pass performs deletion updates first, then regional insertion.
- Incremental mode does not run rewrite.
- Baseline formulas must exist (`nodeFormulas`/`edgeFormulas` non-empty), otherwise an `assert` fails.
- Incremental mode disables bi-imp merge (after full merge you cannot switch to inc/inc-regional).

## How to Enable
- CLI flag: `--setmode inc-regional` (or interactive CLI `setmode inc-regional`).
- `inc` / `incr` / `incremental` now map to `inc-naive`.
- Output files are separated by mode:
  - `fact-iter{N}-inc-naive.prob`
  - `fact-iter{N}-inc-regional.prob`
  - `fact-iter{N}-full.prob`

## Code Map
- `src/include/souffle/problog/RegionalIncremental.h`: main implementation (Analyzer -> Plan -> Rebuild -> Calibrate).
- `src/include/souffle/problog/IncRegionAnalyzer.h`: region analysis, boundary classification, mergeable anchors cache, analysis timing.
- `src/include/souffle/problog/DerivationGraph.h`: prune-inc builds impacted maps + deltaReach cache.
- `src/include/souffle/problog/ForwardCompilation.h`: entry for `buildFormulasIncRegionalCyclewise`.
- `src/include/souffle/cli/Cli.h` / `src/MainDriver.cpp` / `src/include/souffle/CompiledOptions.h`: mode parsing and iter output names.
- `src/include/souffle/problog/formula/*`: weight read/write and reordering time stats.

## Pipeline Overview (Per Turn)
1) Run incremental RAM (`program->runAllInc`) to populate delta relations and derivation deltas.  
2) `graph->applyDelta(...)` updates the derivation graph (DRed-style).  
3) `prune-inc` builds the incremental subgraph view; inc-regional enables insert-impact tracking.  
4) Forward compilation (`buildFormulasIncRegionalCyclewise`):  
   - Deletion phase: overdelete/condition formulas + rederive along the dependency graph.  
   - Insertion phase: analyzer → plan → rebuild → calibrate (regional path).  
5) WMC + emit iter results (`fact-iter<N>-{inc-naive|inc-regional|full}.prob`).

---

## Region Analysis (IncRegionAnalyzer)
Input: `IncrementalDerivationGraphViewInterface` + delta insert facts (or delta insert nodes if no facts).  
Output: region, boundary classification, mergeable anchors cache, deltaReachable subgraph.

Key steps (correspond to `[inc-analyze] timing(ms)`):
- `buildLeastParents_()` / `computeScopes_()`: scope and dependency structure.
- `reachFromSources_()`: compute reachability filter from delta sources.
- `initialRegion_()`: initial region.
- `classifyBoundaries_()` + `expandToFixpoint_()`: expand region by boundary rules.
- `upstreamClose_()`: add ancestors upstream (limited by reach_filter), reclassify/expand if needed.
- `deltaReachable_()` + `intersectWithDeltaReachable_()`: ensure region stays inside delta-reachable.
- `computeMergeableAnchors_()`: cache mergeable anchor candidates for boundary nodes.

Boundary classification:
- `out_induced` / `scope_induced` / `residual` boundary nodes.

Output safety:
- Delta‑reachable outputs may remain **outside** the region (no output‑slice expansion).
- WMC distinguishes three modes:
  1) **Region outputs**: recompute using the rebuilt BDD + original weight map.
  2) **Delta‑reachable outputs outside region**: reuse old BDD + calibrated weights.
  3) **Outside delta‑reach**: reuse cached value if evidence unchanged.

Mergeable anchor criteria (`mergeableEdgeAtHead_`):
- Not a delta insert edge.
- Non-deterministic edge (deterministic edges are excluded from direct edge anchors).
- Respects scopes.
- Non-subsumed.
- Boundary heads with any incoming delta-insert edge are treated as **non-mergeable**
  (anchor candidates skipped), forcing region expansion or fallback to avoid calibrating
  across structural insertions.

Anchor candidates:
- Anchors can be **edges** (non-det incoming edges), or **nodes** (non-det fact nodes).
- Anchors inside the region are rejected (avoid calibrating on rebuilt formulas).
- Anchor candidates exclude ProbQuery/evidence nodes (`isQueryNode`/`hasEvidence`).
- If a boundary head is fed by a deterministic edge and the path from head to anchor
  contains no query/evidence nodes, we allow anchors from that deterministic edge’s inputs:
  - non-det fact inputs become **node anchors**
  - otherwise, any incoming **non-det edge** to those inputs becomes an edge anchor
  This supports cases where all direct incoming edges are deterministic.

Note: region uses `unordered_set`, iteration order is unstable.

---

## Impacted Maps & Delta Reach Cache (DerivationGraph)
`prune-inc` currently clears impacted maps and leaves insert-impact caches empty; the analyzer
therefore falls back to a live-graph BFS from delta insert nodes/edges when caches are missing.

`deltaReachable_()` priority:
1) Use delta-insert reachable cache (if populated)  
2) Else use impacted maps (if populated)  
3) Else BFS from delta insert nodes/edges over live edges  

`reach_filter_` uses the same delta-reach fallback chain as `deltaReachable_()`.

---

## Planning (RegionalInsertPlan)
`RegionalInsertPlanBuilder` builds a plan from analyzer cache:
- `regionNodes`: nodes inside region.
- `boundaryNodes`: union of the three boundary types.
- `anchorCandidates`: cached anchors from analyzer.
- `mergeReady`: true if `boundaryNodes` is empty or each boundary has at least one anchor.
- `regionClosed`: `boundaryNodes` is empty.

---

## Rebuild (RegionalDDRebuilder)
Goal: rebuild only nodes/edges inside the region; keep external formulas unchanged.

Steps:
- **Snapshot**: save old formulas for region nodes/edges (for fallback) and
  boundary nodes (for calibration).
- **Init inserted nodes/edges**: build formulas and set weights for delta insert nodes/edges.
- **SCC handling**: build a dep‑graph from delta‑reachable edges (fallback to full
  graph if reach-edges are empty), collect cycles in the region, compute indegree.
- **Rebuild loop**:
  - Only process edges that satisfy `shouldRebuildEdge`:
    - head is inside the region, and (edge is delta-insert or has inputs inside the region).
  - Worklist is sorted by edge depth.
  - If missing inputs are from inside the region, requeue; if missing inputs are only from outside, skip.
  - After updating an edge formula, recompute the head node OR formula.
  - Requeue outgoing edges if affected.

Timing output (`[inc-regional rebuild]`):
`snapshot/initNodes/initEdges/depGraph/regionCycles/indegree/rebuildLoop/total/reorder`  
`reorder` depends on `FormulaManager::getReorderingTimeSeconds()` (supported by CUDD).

---

## Calibration (BoundaryGateCalibrator)
Goal: compute gate calibration parameter `p*` for boundary nodes (applied by `RegionalIncrementalForwardCompilation`).

Flow (per boundary node v):
1) `target = Pr_new(v)` (computed from rebuilt formulas).
2) For each anchorCandidates[v]:
   - Compute `oldVal` using snapshot old formulas.
   - Set anchor variable weight to 0 to get `alpha`, set to 1 to get `beta`.
   - `p* = (target - alpha) / (beta - alpha)`.
3) Record `CalibrationRecord` and `weightOverrides`.

Current behavior:
- `calibrate()` computes and returns overrides.
- `RegionalIncrementalForwardCompilation` applies the overrides via
  `FormulaManager::setVariableWeight`, stores `lastOverrides_`, and records
  region/delta‑reach sets for WMC routing.
- Degenerate anchors (`alpha ~= beta`) are filtered using boundary snapshots; if a boundary head
  has no non-degenerate anchors it will trigger fallback (unless disabled).

---

## Fallback & Guards
`RegionalIncrementalForwardCompilation::applyUpdate`:
- `mergeReady == false` -> fall back to classic `buildFormulasIncCyclewise`.
- Any boundary calibration failure -> fall back (can be disabled via `Options`).
- `nodeFormulas`/`edgeFormulas` empty -> `assert` failure.

---

## Profiling / Logs
Common output:
- `[inc-analyze] timing(ms): ...`
- `[inc-regional rebuild] timing(ms): ...`
- `[inc-regional] timing(ms): analyze sccClose plan rebuild calibrate total`
- `[prune-inc impact] ...` (prune-inc rebuilds impacted maps)
- `[prune-inc] delta-delete counts (start/post-mark-pruned/filtered/canonicalised/view): nodes=... edges=...`
- `[inc-naive] delta counts: insNodes=... insEdges=... delNodes=... delEdges=...`
- `[inc-regional] delta counts: insNodes=... insEdges=... delNodes=... delEdges=...`
- `Deletion deletedVarsIndex size: N` (used for variable cleanup during deletion)
Debug output is off by default; enable as needed:
- `--dumpjson` / CLI `set dumpjson`: output JSON (after prune).
- `--profile-inc-regional`: emits inc-regional analysis/boundary/anchor diagnostics and timing.
- `--profile-inc-regional-heavy`: enables heavy inc-regional diagnostics (backward-closure trace for `--inc-regional-trace-tuples`).

---

## Known Issue / TODO (Anchors)
- Some KEY_IND-heavy cases produce **all-degenerate anchors** for boundary heads
  (alpha ~= beta), leading to `calibration_failed` fallback even though anchors exist.
- The analyzer now logs the full candidate set per boundary head and prints
  `degenerate_anchor` when all candidates collapse.
- Next steps: improve candidate selection for KEY_IND rules (e.g., prefer anchors
  whose formulas contain non-trivial random variables), or add an alternative
  calibration path when only degenerate anchors exist.
- `--dumpdot` / CLI `set dumpdot`: output derivation graph DOT.
- `--dumpstat` / CLI `set dumpstat`: output `dumpStatisticsInc` / `dumpStatistics`.
Output location notes:
- All `.dot` / `.json` / `dumpStatistics` outputs go into the `-D` output directory (including `derivation-inc-*.dot/json`, `scc.dot`, etc.).
- Debugger JSON reports also go into the output directory; the filename uses the **basename** of `--logfile` plus a timestamp; stdout only prints the filename.

---

## Known Limitations / TODO
- Regional only covers insertion; deletion still uses old logic.
- Current side-channel inc1 benchmark has little disjunction, so inc-regional often
  matches inc-naive behavior (delta-reachable is narrow and calibration rarely changes outputs).
- Anchor selection currently takes the first feasible candidate with no scoring/optimization.
- Region iteration order is unstable (`unordered_set`); logs and output ordering are not guaranteed stable.

---

## Quick Run (Example)
```
./compute -F input -D output_run_inc_regional --setmode inc-regional < delta/inc10_1.txt
./compute -F input -D output_run_full --setmode full < delta/inc10_1.txt
```
Confirm consistency:
```
diff output_run_inc_regional/fact-iter1-inc-regional.prob output_run_full/fact-iter1-full.prob
```

## Related commits
- `739ee83cb` — refactor(inc-region): align regional insert with naive propagation
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
- `986d6bf48` — fix(inc-region): apply gate overrides and tighten anchors
