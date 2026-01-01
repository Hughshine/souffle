# Constant Pre-Analysis (Const-FC)

This document describes the constant pre-analysis that runs inside forward compilation (FC).
It is conservative: it does not mutate the derivation graph or the view; it only short-circuits
formula construction when nodes/edges are provably constant.

## Goals and scope
- Move deterministic constant simplification out of prune; prune does not rewrite the graph.
- Shared across full and incremental FC; no incremental maintenance.
- Safe fallback: if no constants are found or `--fold-const` is disabled, FC behaves as before.

## Analysis (positive-only, conservative)
- Seed TRUE: fact nodes with p=1.0.
- Seed FALSE (optional): fact nodes with p=0.0 and edges with p=0.0.
- Deterministic edges: an edge is TRUE if all its inputs are TRUE.
- FALSE propagation: an edge is FALSE if any input is FALSE; a node is FALSE if all incoming eligible
  edges are FALSE and it is not a fact with p>0.
- Negation is ignored: any edge with a negated input is skipped and counted as "ignored".
  Nodes with any ineligible incoming edge are never marked FALSE.

## Output and flags
- `--fold-const`: enable FC short-circuiting using the analysis results.
- `--dumpconst`: run the analysis and dump `const-pre-<tag>.txt` to the output directory; does not
  change formulas unless `--fold-const` is also set.
- Logs: `[const-pre] tag=...` prints counts (true/false nodes/edges, eligible/ignored).
- Timing: `[const-pre] tag=... took X ms` is emitted whenever const analysis runs
  (triggered by `--fold-const` or `--dumpconst`).

Tags used today:
- `full-worklist`, `full-cyclewise`, `full-ondemand`
- `inc-worklist`, `inc-cyclewise`, `inc-regional`
Only top-level FC calls log/dump (`seedTrueNodes` is empty).

## Implementation map (per-FC pipeline changes)
- `src/include/souffle/problog/ConstAnalysis.h`
  - `analyzeConstants`: computes `ConstAnalysisResult` (true/false nodes/edges).
  - `ConstFormulaAccess`: helpers to short-circuit node/edge formulas and input literals.
- `src/include/souffle/problog/ForwardCompilation.h`
  - `buildFormulas` (full worklist):
    - run `analyzeConstants` once; use `constAccess.edgeFormula` before AND,
      `constAccess.inputLiteral` for inputs, and `constAccess.nodeFormula` before OR.
  - `buildFormulasCyclewise` (full default, also GraphRewriter):
    - same short-circuits; const analysis ignores `seedTrueNodes` (they are still forced true in init).
  - `buildFormulasCyclewiseOnDemand`:
    - same short-circuits.
  - `buildFormulasInc`:
    - same short-circuits in delete/rederive and insert worklists.
  - `buildFormulasIncCyclewise` (inc-naive):
    - same short-circuits in delete and insert phases.
  - `buildFormulasIncRegionalCyclewise` (inc-regional):
    - same short-circuits in delete phase and the regional insert loop.
- `src/include/souffle/problog/RegionalIncremental.h`
  - `RegionalDDRebuilder::rebuildInsertRegion` uses `ConstFormulaAccess`.
  - `RegionalIncrementalForwardCompilation::applyUpdate` accepts const analysis pointer and passes it to rebuild.
- `src/include/souffle/problog/GraphRewriter.h`
  - Local FC calls still go through `buildFormulasCyclewise`, so const analysis applies there too.

## Legacy prune folding
- `DerivationGraph::foldDeterministicConstants()` still exists but is not called by prune; kept as
  legacy reference.

## TODO / limitations
- Negation-aware constant propagation (three-valued logic).
- Better FALSE-node handling with negation/evidence.
- Decide whether const analysis should treat `seedTrueNodes` as additional TRUE seeds.
