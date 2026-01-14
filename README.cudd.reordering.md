# CUDD Reordering (ProbLog BDD Path)

## Scope
- Covers how dynamic and explicit variable reordering is configured and used in the
  ProbLog BDD path.
- Code references:
  - `src/include/souffle/problog/formula/CuddManager.h`
  - `src/include/souffle/problog/ForwardCompilation.h`
  - `src/problog/Pipeline.cpp`

## Manager Initialization (Cudd_Init)
- The BDD manager is created in `WeightedBDDManager::initManager()` using:
  `Cudd_Init(numVars, numVarsZ, numSlots, cacheSize, maxMemory)`.
- The config is built in `makeCuddInitConfig()` (Pipeline) using the estimated
  BDD variable count:
  - `numVars`: `min(2 * varCount, UINT_MAX)` to preallocate BDD vars.
  - `numVarsZ`: default 0.
  - `numSlots`: fixed to 512 (unique table buckets).
  - `cacheSize`, `maxMemory`: scaled by `varCount` (small graphs get smaller
    cache/memory; large graphs use defaults).
- GC and reorder hooks are installed via `Cudd_AddHook(..., CUDD_*_HOOK)`.

## Dynamic Reordering Enablement
- Dynamic reordering is enabled via `adaptiveReorder(manager)` which calls
  `Cudd_AutodynEnable(manager, next)` or disables it for large graphs.
- The heuristic is chosen by `adaptiveReorder()` based on current BDD node count:
  - `< 10k`: `CUDD_REORDER_SIFT_CONVERGE`
  - `< 50k`: `CUDD_REORDER_SIFT`
  - `< 100k`: `CUDD_REORDER_WINDOW4_CONV`
  - `< 300k`: `CUDD_REORDER_WINDOW4`
  - `< 3M`: `CUDD_REORDER_WINDOW2`
  - `>= 3M`: `CUDD_REORDER_NONE` (dynamic reordering disabled)
- `adaptiveReorder2()` exists but is not used.

## When Reordering Is Activated
- `formulaManager.preConfig(view)` is called in both full and inc pipelines:
  - Full: `buildFormulasCyclewise` and `buildFormulasCyclewiseOnDemand`
  - Inc: `buildFormulasIncCyclewise` (insert preconfig)
- `preConfig(...)`:
  - Clears caches, scans facts/edges, and calls `createVar(...)`.
  - Calls `adaptiveReorder(...)` to enable dynamic reordering based on size.
- Dynamic reordering itself is triggered by CUDD during BDD operations
  (e.g., `Cudd_bddAnd`, `Cudd_bddOr`, `Cudd_bddIthVar`). It is not forced by
  `preConfig(...)` beyond enabling a heuristic.

## Reordering Hooks and Counters
- `myVRFunc` is registered for `CUDD_PRE_REORDERING_HOOK` and
  `CUDD_POST_REORDERING_HOOK`.
  - It records start/end timestamps for each reorder and increments a counter.
  - After each reorder, it calls `adaptiveReorder(dd)` to pick the next heuristic.
- `Cudd_ReadReorderingTime()` provides cumulative time spent in CUDD reordering.
  We emit a per-stage delta as `reordering_runtime` in FC profiling.
- `Cudd_ReadReorderings()` provides the total number of reorder operations; this
  is captured in `CUDD_CREATEVAR_STATS` when `--fc-profile` is enabled.

## Explicit Reordering (Non-Dynamic)
- `postprocessUselessVariables()` uses `Cudd_ShuffleHeap` to explicitly reorder
  variables after deletion when `--post-del` is enabled.
- This is separate from CUDD dynamic reordering and does not use the VR hook.

## Profiling and Observability
- `--fc-profile` adds:
  - `CUDD_PRECONFIG` lines: total preConfig time and sub-breakdown.
  - `CUDD_CREATEVAR` and `CUDD_CREATEVAR_STATS`: per-var timing and deltas for
    GC/reorder/slots/keys.
  - `reordering_runtime` in FC stage info (delta of `Cudd_ReadReorderingTime`).
- The `Variable ordering takes ...` / `Variable reordering takes ...` messages
  measure `preConfig(...)` wall time, not dynamic reordering time alone.

## Summary
- Full and incremental pipelines share the same CUDD dynamic reordering path.
- Differences in reordering cost come from workload size and when CUDD decides to
  trigger reorders, not from different configuration paths.
- Explicit reordering (`Cudd_ShuffleHeap`) is only used by post-delete cleanup
  and is controlled by `--post-del`.
