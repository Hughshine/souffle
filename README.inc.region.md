# Inc-Regional Incremental Pipeline (Current Design)

This file describes the current inc-regional incremental pipeline implementation. It follows the code and is not a planning document.

## Status
- Active design note; matches current implementation.

## Scope / Context
- Applies only to online incremental binaries (default; `--online` optional); the old `--inc` backend is removed.
- inc-regional only replaces insertion forward compilation; deletion still reuses the inc-naive DRed-like logic.
- rewrite is not executed in inc/inc-regional mode.

## Related docs
- `README.eval.inc.md` (benchmark procedures + logs)
- `README.dred.md` (online DRed internals and deletion bottlenecks)

## Assumptions
- Deletion runs the classic incremental delete logic first, then enters regional insertion.
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
1) `applyDeltaDeletes` (old logic)  
2) `applyDeltaInserts` (old logic)  
3) `prune-inc` (build subgraph + impacted maps + deltaReach cache)  
4) Forward compilation: run deletion (inc-naive logic) first, then inc-regional insertion  
5) WMC + emit iter results

inc-regional only replaces insertion forward compilation; deletion reuses inc-naive deletion.

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
- `computeMergeableAnchors_()`: cache mergeable old incoming-edge candidates for boundary nodes.

Boundary classification:
- `out_induced` / `scope_induced` / `residual` boundary nodes.

Mergeable anchor criteria (`mergeableEdgeAtHead_`):
- Not a delta insert edge.
- Non-deterministic edge (deterministic edges are excluded).
- Respects scopes.
- Non-subsumed.

Note: region uses `unordered_set`, iteration order is unstable.

---

## Impacted Maps & Delta Reach Cache (DerivationGraph)
`prune-inc` rebuilds impacted maps (insert/delete) on the subgraph:
- Run BFS from each delta insert fact (along outgoing edges) to collect impacted nodes/edges.
- Store in `insertedFactImpactedNodes/Edges` (`unordered_set`).
- Also build **delta-insert reachable union cache**:
  - `deltaInsertReachableNodes`
  - `deltaInsertReachableEdges`
  This cache is exposed to the analyzer via `getDeltaInsertReachableNodes/Edges()`.

`deltaReachable_()` priority:
1) Use union cache (fastest)  
2) If empty, fall back to impacted maps  

`reach_filter_` directly uses delta-reachable cache (no extra patching).

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
- **Snapshot**: save old formulas for boundary nodes (for later calibration).
- **Init inserted nodes/edges**: build formulas and set weights for delta insert nodes/edges.
- **SCC handling**: build `CycleDependencyGraph`, collect cycles in the region, compute indegree.
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
Goal: compute gate calibration parameter `p*` for boundary nodes (not auto-applied yet).

Flow (per boundary node v):
1) `target = Pr_new(v)` (computed from rebuilt formulas).
2) For each anchorCandidates[v]:
   - Compute `oldVal` using snapshot old formulas.
   - Set anchor variable weight to 0 to get `alpha`, set to 1 to get `beta`.
   - `p* = (target - alpha) / (beta - alpha)`.
3) Record `CalibrationRecord` and `weightOverrides`.

Current behavior:
- `calibrate()` **only computes and returns overrides**, does not write back weights.
- `RegionalIncrementalForwardCompilation` stores `lastOverrides_` for external use.

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
- `--dumpdot` / CLI `set dumpdot`: output derivation graph DOT.
- `--dumpstat` / CLI `set dumpstat`: output `dumpStatisticsInc` / `dumpStatistics`.
Output location notes:
- All `.dot` / `.json` / `dumpStatistics` outputs go into the `-D` output directory (including `derivation-inc-*.dot/json`, `scc.dot`, etc.).
- Debugger JSON reports also go into the output directory; the filename uses the **basename** of `--logfile` plus a timestamp; stdout only prints the filename.

---

## Known Limitations / TODO
- Calibration results are not auto-applied yet (computed and cached only).
- Regional only covers insertion; deletion still uses old logic.
- Current side-channel inc1 benchmark has no disjunction, so inc-regional effectively degenerates to inc-naive (delta-reachable is mostly deleted parts).
- Anchor selection currently takes the first feasible candidate with no scoring/optimization.
- Region iteration order is unstable (`unordered_set`); logs and output ordering are not guaranteed stable.

---

## Quick Run (Example)
```
./compute -F input -D output_run_inc_regional --setmode inc-regional < delta/inc10_1.txt
```
Confirm consistency:
```
diff output_run_inc_regional/facts.prob output_run_full/facts.prob
```
