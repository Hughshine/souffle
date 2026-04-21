# SDD Manager (ProbLog SDD Path)

## Source references
- [src/include/souffle/problog/formula/SddManager.h](src/include/souffle/problog/formula/SddManager.h)


## Scope
- Documents the current state of the ProbLog SDD backend (`SddFormulaManager`) and
  what it still lacks compared to the CUDD BDD backend (`WeightedBDDManager`).
- Focus: incremental/online use (variable/index lifecycle), reset semantics, and
  performance/observability parity.

## Code References
- SDD manager:
  - `src/include/souffle/problog/formula/SddManager.h`
- CUDD manager:
  - `src/include/souffle/problog/formula/CuddManager.h`
- Forward compilation and incremental delete/reuse hooks:
  - `src/include/souffle/problog/ForwardCompilation.h`
- Incremental CLI lifecycle hooks (reset/GC):
  - `src/include/souffle/cli/Cli.h`
- Pipeline construction (SDD manager instantiation):
  - `src/problog/Pipeline.cpp`

## Current SDD Behavior (What Exists)

## What the SDD Library Actually Exposes (API Surface)
The public header `sdd/sdd.h` (this repo includes it via `<sdd/sdd.h>`) provides:

- **Manager lifecycle**: `sdd_manager_create(...)`, `sdd_manager_free(...)`, `sdd_manager_copy(...)`.
- **Variable management (monotonic)**:
  - Query: `sdd_manager_var_count(...)`, `sdd_manager_is_var_used(...)`, `sdd_manager_var_order(...)`.
  - Add variables: `sdd_manager_add_var_after_last(...)` and friends.
  - Note: there is **no API to remove variables** or shrink `var_count`.
- **Boolean ops / transformations**:
  - `sdd_conjoin`, `sdd_disjoin`, `sdd_negate`, `sdd_condition`, plus `sdd_exists`/`sdd_forall`.
- **GC + refcounting**:
  - `sdd_ref` / `sdd_deref` and `sdd_manager_garbage_collect(_if)`.
- **Vtree control + “reordering” analogue**:
  - Build/load/save vtrees: `sdd_vtree_new(_with_var_order|_X_constrained)`, `sdd_vtree_read/save`.
  - Local edits: `sdd_vtree_rotate_left/right`, `sdd_vtree_swap`.
  - Global search: `sdd_manager_minimize(_limited)` with time/memory/size limits knobs.
  - Auto mode: `sdd_manager_auto_gc_and_minimize_on/off`.
- **WMC**:
  - `wmc_manager_new/free`, `wmc_set_literal_weight`, `wmc_propagate`, plus derivative / literal marginal helpers.

### Initialization
- `SddFormulaManager(var_count, auto_gc)` calls `sdd_manager_create(var_count, auto_gc ? 1 : 0)`.
  - Default call sites construct with no args, so `var_count` becomes `1`.
  - `vtree_` is currently always `nullptr` (no explicit vtree construction).

### Variable and Weight Mapping
- Two-layer indexing:
  - `rawIndex` is assigned by `getVarIndex(const Node&)` / `getVarIndex(const Hyperedge&)`
    using a monotonically increasing counter (`nextRawVar++`).
  - `rawIndex -> internalIndex` is tracked in `rawToInternal`.
- Variable creation (`createVar(rawIndex, ...)`) allocates/expands the SDD manager:
  - New raw index increments `nextInternalVar` and may call `sdd_manager_add_var_after_last()`.
  - Literals are cached in `variableRegistry[internalIndex]`.
- Weights are stored in `weight_map_[internalIndex]` and materialized into a dense
  `2*varCount` array in `buildWeightArray()` for each WMC call.
- `setVariableWeight(rawIndex, ...)` only updates weights for already-mapped raw
  indices; it does **not** allocate variables or store pending weights for
  unmapped raw indices.

### Weighted Model Counting (WMC)
- `computeWeightedModelCount(node)`:
  - Creates a fresh `WmcManager` (`wmc_manager_new`) every call.
  - Builds the full weight array and re-applies all literal weights every call.
  - Does not cache WMC results across calls.

### Observability
- `dumpProfilingStatistics()` prints vtree/live/dead node info from the SDD manager.
- `getProfilingStatistics()` now returns `{live_nodes, dead_nodes, total_nodes}` but still
  lacks CUDD-style cache/memory/reordering fields.

## Gaps vs CUDD (Missing Support / Parity Checklist)

### 1) Variable Index Reuse for Incremental Deletes
**CUDD**
- Implements `releaseVarIndex(const Node&)` / `releaseVarIndex(const Hyperedge&)` and
  reuses freed indices (via `freeIndices_` plus tuple/ruleApp keyed reuse).
- Incremental deletion calls `formulaManager.releaseVarIndex(...)` when enabled.

**SDD (Supported, with constraints)**
- Overrides `releaseVarIndex(...)` to:
  - drop pointer-keyed maps (`nodeToRawIndex_` / `edgeToRawIndex_`),
  - drop raw→internal mapping and cached literal/weight state when safe,
  - reuse freed **internal** variable IDs (optionally keyed by tuple/ruleApp) when `reuseVarIndexEnabled`.
- Safety: an internal var is only recycled when `sdd_manager_is_var_used(var)` reports it is unused.
- `rawIndex` allocation still stays monotonic; the SDD manager `var_count` cannot shrink, but internal reuse
  prevents further growth in steady-state incremental workloads.

Why it matters:
- In incremental runs, deleted facts/edges leave behind stale pointer-keyed map
  entries (potential correctness hazard if addresses are reused).
- `sdd_manager_var_count()` monotonically increases, which increases the cost of:
  - building the dense weight array,
  - re-applying all literal weights during WMC.

### 2) Reset Semantics (Soft/Hard)
**CUDD**
- Implements:
  - `reset()` (logical reset: clear maps/caches, keep manager instance),
  - `resetHard()` (recreate manager).
- The incremental CLI calls `reset()`/`resetHard()` in full-mode turns.

**SDD (Supported)**
- Overrides `reset()` and `resetHard()` so the incremental CLI can use SDD safely in `FULL_SOFT`/`FULL_HARD`.
- `reset()` clears wrapper state (maps, caches, free-lists) and triggers conditional GC.
- `resetHard()` recreates the underlying `SddManager` (drops accumulated var_count growth).

### 3) Manager Lifetime / RAII
**CUDD**
- Uses RAII (`std::shared_ptr<DdManager>`) so the underlying manager is released
  automatically.

**SDD (Supported)**
- `SddNodeRef` now holds a `std::shared_ptr<SddManager>` so node refs extend manager lifetime.
- `SddFormulaManager` owns the `SddManager` via `std::shared_ptr` with a deleter that calls `sdd_manager_free()`.

### 4) Initialization Sizing / Preallocation
**CUDD**
- Computes an estimated variable count and passes it into `Cudd_Init(...)` config
  (prealloc vars, cache sizing, memory cap scaling).

**SDD (Missing)**
- Default construction starts at `var_count=1` and grows one variable at a time via
  `sdd_manager_add_var_after_last()`.
- There is no `preConfig(view)` override to pre-size `var_count` based on the
  pruned graph or incremental deltas.

### 5) Garbage Collection / Minimization / Optimization Controls
**CUDD**
- Implements:
  - `tryGarbageCollection()` (heap reduction),
  - dynamic reordering enable/disable,
  - `stopDynamicOptimization()`.

**SDD (Partial)**
- Wires the constructor `auto_gc_and_minimize` flag through to `sdd_manager_create`.
- Implements `tryGarbageCollection()` via `sdd_manager_garbage_collect_if(...)`.
- Still missing: explicit vtree minimization/search policy wiring and any analogue of “stop dynamic optimization”.

### 6) Profiling / Stats Parity
**CUDD**
- Implements `getLiveNodeCount()` and structured `getProfilingStatistics()` for FC/CLI.

**SDD (Partial)**
- Implements `getLiveNodeCount()` and `getProfilingStatistics()` with live/dead/total node counts.
- Missing CUDD-style stats: cache hit rate, memory usage, reordering runtime.

### 7) WMC Performance Parity
**CUDD**
- Uses an internal cache (`wmcCache_`) per manager to memoize recursive WMC.

**SDD (Missing)**
- Rebuilds and reapplies weights per WMC call; no reuse/caching layer.
- Practical implication: even if the SDD structure is unchanged, repeated per-node
  WMC in FC/CLI can be dominated by weight array setup cost, which also scales with
  `sdd_manager_var_count()`.

### 8) Deterministic Facts/Edges Handling
**CUDD**
- Setting weights does not force creation/expansion of variables; deterministic facts
  can map to `True` without growing the manager.

**SDD (Current Behavior)**
- `setVariableWeight(rawIndex, ...)` only applies if `rawIndex` already maps to an
  internal variable; it will **not** allocate or grow `var_count` on its own.
- Deterministic facts typically stay as constants (no variable creation), which
  avoids manager growth but also means weights for unmapped raw indices are ignored
  until a formula forces `createVar`.

## Status / Known Issues
- SDD remains non-default for AE. Use BDD unless a diagnostic run explicitly targets SDD.

## Suggested Next Steps (Implementation Checklist)
- Add `preConfig(view)` to pre-size var_count (full + delta-insert paths) and to clear
  per-turn caches if any are added.
- Expand stats parity with CUDD (cache hit rate, memory usage, reorder runtime).
- Avoid allocating variables for deterministic facts/edges (and/or treat global weights
  without expanding var_count).

## Related commits
- `78890247d` — fix(inc): align delta handling and node metrics
