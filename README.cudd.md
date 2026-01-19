# CUDD (BDD Backend) Notes

## Scope
- This document describes how the CUDD-backed BDD knowledge backend is used in this
  fork for ProbLog-style inference (forward compilation + weighted model counting).
- It is not a general CUDD tutorial.

## When You Are Using CUDD
- Runtime flag: `-k bdd` / `--knowledge=bdd` (default).
- `-k sdd` switches to the SDD backend (optional dependency).
- CLI overview: `docs/USAGE.md`.
- Dependency note and troubleshooting: `docs/RUNBOOK.md`.

## Key Code Locations
- BDD manager wrapper: `src/include/souffle/problog/formula/CuddManager.h`
  - `WeightedBDDManager` owns the `DdManager` and implements BDD ops + WMC.
  - `WeightedBDDManager::initManager()` calls `Cudd_Init(...)`.
- Pipeline wiring + init config: `src/problog/Pipeline.cpp`
  - `estimateBddVarCount(...)` and `makeCuddInitConfig(...)` choose init parameters.
- Forward compilation call sites: `src/include/souffle/problog/ForwardCompilation.h`
  - `formulaManager.preConfig(view)` is invoked in both full and incremental paths.

## Manager Initialization (`Cudd_Init`)
- The manager is created via:
  `Cudd_Init(numVars, numVarsZ, numSlots, cacheSize, maxMemory)`.
- Current init strategy is code-driven (Pipeline computes a config from an estimated
  BDD variable count, then passes it into `WeightedBDDManager`).
- Detailed notes (thresholds, hooks, and reordering behavior):
  - `README.cudd.reordering.md`

## Reordering and Post-Delete Shuffle
- Dynamic reordering is configured by `adaptiveReorder(...)` and CUDD autodyn.
- Explicit (non-dynamic) reordering is done via `Cudd_ShuffleHeap` in
  `postprocessUselessVariables()` when `--post-del` is enabled.
- Full details:
  - `README.cudd.reordering.md`

## Profiling and Observability
- Enable: `--fc-profile` (runtime).
- CUDD-focused lines include:
  - `[fc-profile] stage=CUDD_PRECONFIG ...`
  - `[fc-profile] stage=CUDD_CREATEVAR ...`
  - `[fc-profile] stage=CUDD_CREATEVAR_STATS ...`
- Field-level reference:
  - `README.fc.profile.md`

## Practical Tuning Workflow
1. Reproduce with `--fc-profile` enabled and capture stdout.
2. Check whether manager growth dominates:
   - `CUDD_PRECONFIG` shows `old_var_size`/`new_var_size`.
   - Large `new_var_size >> old_var_size` typically means repeated manager growth
     (variable indices not stable across turns, or `numVars` too small).
3. Check for GC/reorder during variable creation:
   - `CUDD_CREATEVAR_STATS` reports deltas around `Cudd_bddIthVar(...)`.
4. If you tune initialization, change only the config source:
   - `src/problog/Pipeline.cpp` (`makeCuddInitConfig`).
   - Keep changes documented in `README.cudd.reordering.md` if thresholds change.

## Common Failure Modes
- `Failed to initialize CUDD manager`: missing/incorrect CUDD install or link.
- Performance cliffs:
  - Manager grows repeatedly (insufficient `numVars` headroom or unstable indices).
  - Cache is too small for the workload (high recomputation).
  - Reordering is too frequent or disabled too early (workload-dependent).

