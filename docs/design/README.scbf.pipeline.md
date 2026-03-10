# SCBF Pipeline Design (Derivation Graph -> SCBF -> Rewrite -> DD/WMC)

## Status
- Design proposal with phased implementation.
- Phase-1 runtime integration is intended to be experimental and opt-in only.
- Current implementation (experimental):
  - `--scbf` routes to `buildFormulasCyclewise` (not `OnDemand`).
  - A cycle-break mode computes output WMC per SCC, keeps only cross-SCC boundary
    formulas, and purges completed SCC-local formulas.
  - Evidence-conditioned SCBF is still fallback-to-default.

## Motivation
- In taint workloads, some stages are strongly rewriteable, but cycle-heavy stages
  (for example `cipt-cg`) often retain large recursive structure after rewrite.
- Whole-graph formula construction keeps cyclic dependence active during formula
  propagation, which can reduce rewrite payoff and increase DD sensitivity.
- We want a pipeline that introduces an explicit cycle-broken formula layer before
  DD/WMC while preserving existing semantics.

## Problem Statement
- Add an optional full-mode pipeline:
  1. build derivation graph (`DG`),
  2. project it to a shared cycle-broken formula representation (`SCBF`),
  3. apply rewrite on SCBF-compatible structure,
  4. compile to DD/WMC.

Default pipeline must remain unchanged unless an explicit runtime flag is set.

## Terminology
- `DG`: existing derivation graph with potential recursion.
- `Stratum`: SCC-level topological layer induced by dependency DAG.
- `SCBF`: shared cycle-broken formula representation.
- `Cross-stratum sharing`: reuse of already-fixed derivations from earlier strata.
- `Intra-stratum sharing`: sharing inside the same stratum; this is disallowed in
  the target design to enforce cycle breaking.

## Semantics and Invariants
1. Result equivalence:
- For fixed facts/evidence, output marginals must match current exact pipeline
  (up to floating-point tolerance).

2. Sharing rule:
- Allowed: reuse derivations from earlier strata.
- Disallowed: sharing within the same stratum.

3. Rewrite safety:
- Rewrite is still equivalence-preserving; SCBF only changes evaluation order and
  sharing boundaries.

4. Compatibility:
- Existing non-SCBF path remains default and behavior-identical.

## Existing Hooks To Reuse
- Pipeline orchestration:
  [src/problog/Pipeline.cpp](/home/hugh/research/datalog/souffle/src/problog/Pipeline.cpp)
- SCC/dependency stratification:
  [src/include/souffle/problog/DerivationGraph.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/DerivationGraph.h)
- Cyclewise formula build:
  [src/include/souffle/problog/ForwardCompilation.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/ForwardCompilation.h)
- Rewrite engine:
  [src/include/souffle/problog/GraphRewriter.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/GraphRewriter.h)

## Architecture

### 1) SCBF Planner
- Input: pruned derivation view.
- Output:
  - SCC DAG/topological stratum order,
  - boundary map for cross-stratum inputs,
  - per-stratum processing plan.

### 2) SCBF Evaluator
- Builds fragment formulas stratum-wise.
- Reuses earlier-stratum formulas only.
- Breaks same-stratum sharing boundary by construction policy.
- Produces target-local fragments first, then a stitched global formula view.

### 3) Rewrite Integration
- Rewrite should not stop at stratum boundaries.
- Bundle fragments are construction artifacts; stitched global SCBF is the right
  post-build rewrite scope.
- Current prototype keeps old pipeline unchanged and adds the stitched global
  layer independently.

### 4) DD/WMC Backend
- Uses existing BDD/SDD managers.
- Phase-1 can use on-demand cyclewise evaluation and per-cycle GC to limit
  whole-graph formula retention.

## Phased Implementation Plan

### Phase-0 (this doc + runtime gate)
- Add an opt-in runtime flag (for example `--scbf`).
- Keep old behavior as default.
- Ensure graceful fallback to old path when SCBF constraints are not met.

### Phase-1 (experimental runtime path)
- Introduce SCBF full-mode execution path in pipeline.
- Use SCC/stratum order and on-demand cyclewise formula evaluation as initial SCBF
  executor.
- Restrict to no-evidence flow first; fallback for evidence to old path.
- Record SCBF-specific logs:
  - mode enabled,
  - fallback reason,
  - build/dump timings.

### Phase-2 (full SCBF semantics)
- Enforce strict intra-stratum sharing break in formula IR.
- Add explicit SCBF IR type (instead of implicit scheduling policy only).
- Integrate evidence-conditioned SCBF path.
- Evaluate rewrite opportunities directly on SCBF structure.

### Phase-3 (rewrite + SCBF co-design)
- Add split-as-implicit rewrite behavior where applicable.
- Add per-query SCBF path with cross-stratum-only sharing.
- Compare with baseline rewrite on taint `cipt-cg` hard cases.

## Verification Plan
1. Correctness:
- Compare output probabilities between old pipeline and `--scbf` on small
  deterministic/probabilistic benchmarks.

2. Stability:
- Ensure `--scbf` does not affect runs when flag is absent.

3. Performance:
- Measure FC/WMC time and live nodes on representative taint cases.
- Track fallback rate and reasons.

4. Regression:
- Add dedicated regression once semantics and flags are finalized.

## Risks
- SCC-level scheduling alone is not equivalent to full SCBF semantics unless
  intra-stratum sharing rules are enforced.
- Evidence conditioning requires careful integration to avoid semantic drift.
- Over-aggressive GC or partial formula retention can break incremental/CLI
  expectations if reused outside full-only mode.

## Source references
- [src/problog/Pipeline.cpp](/home/hugh/research/datalog/souffle/src/problog/Pipeline.cpp)
- [src/include/souffle/problog/ForwardCompilation.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/problog/DerivationGraph.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/DerivationGraph.h)
- [src/include/souffle/problog/GraphRewriter.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/GraphRewriter.h)
- [src/include/souffle/CompiledOptions.h](/home/hugh/research/datalog/souffle/src/include/souffle/CompiledOptions.h)

## Related commits
- `UNCOMMITTED` — docs(design): add SCBF pipeline design and phased plan
