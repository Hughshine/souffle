# SCBF Formula Rewrite Prototype (Independent)

## Status
- Independent prototype for rewrite on SCBF formula IR.
- Not wired into old DG rewrite path and not in default runtime pipeline.

## Goal
- Provide a rewrite layer that operates on `ScbfTargetFormula` directly, so
  rewrite can happen after cycle breaking and before DD/WMC integration.
- Keep rewrite behavior aligned with default DG rewrite:
  - same fast-path SISO kinds,
  - same split modes (`none`/`naive`/`complete`),
  - same fixpoint iteration process.

## Files
- API:
  [src/include/souffle/problog/scbf/ScbfFormulaRewriter.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfFormulaRewriter.h)
- Implementation + smoke:
  [src/problog/scbf/ScbfFormulaRewriter.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfFormulaRewriter.cpp)

## Current rewrite pipeline
1. Graph-aligned rewrite (default)
- Materialize each `ScbfTargetFormula` to a temporary incremental derivation graph.
- Run existing `GraphRewriter::rewriteUntilFixpoint(...)` with aligned flags.
- Rebuild rewritten target formula from the rewritten graph.
- Imports are now materialized as opaque external inputs, not temporary facts, so
  rewrite cannot incorrectly constant-fold away cross-stratum dependencies.

This preserves default rewrite behavior, including:
- fast-path SISO detection/rewrite sequence,
- split pass scheduling and budgets,
- full fixpoint loop semantics.

2. Local post-passes
- Run lightweight local simplifications after graph-aligned rewrite:
  - constant folding,
  - single-local alias collapse,
  - unreachable local-node pruning.

## Local post-pass details
1. Constant folding
- Drops unsatisfiable rules (`edgeProb=0` or body contains constant-0 literal).
- Drops constant-true literals.
- Canonicalizes negated constants into non-negated probabilities.

2. Single-local alias collapse
- If a local node has exactly one rule and that rule is equivalent to a direct
  positive local copy, rewrite references to its representative local node.

3. Unreachable local-node pruning
- Keeps only local nodes reachable backward from target.
- Remaps local literal indices after pruning.

## Interfaces
- `rewriteScbfTargetFormula(...)`
- `rewriteScbfStratumFormulaBundle(...)`
- `summarizeScbfFormulaRewriteStats(...)`

`ScbfFormulaRewriteConfig` now exposes aligned graph-rewrite controls:
- fast-path toggles (`enableSingleHyperedge`, `enableLinearTwoEdge`, etc.),
- split controls (`splitMode`, budgets, group caps),
- `forceCompleteSisoDetect`,
- plus local post-pass toggles.

## Smoke
- target: `souffle-scbf-rewrite-smoke`
- builds a toy SCC and checks:
  - rewrite keeps probability unchanged (within tolerance),
  - node/rule/literal counts do not increase,
  - graph-aligned rewrite path actually runs.
- requires CUDD (same dependency as default BDD-based rewrite path).

## Notes
- SCBF rewrite module is independent at pipeline level, but deliberately reuses
  legacy `GraphRewriter` implementation for algorithm alignment.
- It is meant as a safe stepping stone toward SCBF-native rewrite and later
  SCBF->DD integration.
- Current open issue: graph rewrite -> formula reconstruction is not yet proven
  semantics-preserving on all patterns.
- Bundle-level rewrite is still structurally too weak on many real graphs,
  because per-target exports make every target root a boundary. The new
  stitched-global prototype addresses visibility.
- The current stitched-global rebuild now supports `node`, `constant`, and
  shared `summary` exports, with `elided` left only as a fallback escape hatch.
- On the benchmarked real cases, rewritten-away targets are now represented by
  explicit shared summaries instead of `elided`.
- The next step is no longer export semantics; it is performance: reducing the
  DG round-trip cost (`materialize + rebuild + validate`) or moving rewrite
  directly onto the stitched SCBF graph.

## Source references
- [src/include/souffle/problog/scbf/ScbfFormulaRewriter.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfFormulaRewriter.h)
- [src/problog/scbf/ScbfFormulaRewriter.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfFormulaRewriter.cpp)
- [src/include/souffle/problog/scbf/ScbfFormula.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfFormula.h)
- [src/problog/scbf/ScbfFormula.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfFormula.cpp)

## Related commits
- `UNCOMMITTED` — design(scbf-rewrite): add independent formula-level rewrite prototype and smoke
