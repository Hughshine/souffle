# Approx Pipeline Design (Derivation Graph -> Query Formula -> AMC)

## Status
- Design proposal only.
- Not implemented in runtime pipeline yet.

## Motivation
- Current full pipeline computes probabilities via DD backends (BDD/SDD) after forward compilation.
- For large/random-heavy regions, DD compilation can be expensive or memory-bound.
- We want an alternative counting path based on ApproxMC-style approximate counting, including weighted semantics via weighted-to-unweighted conversion.

## Problem Statement
- Add a new probabilistic pipeline that:
  1. starts from the derivation graph,
  2. can produce a formula per query tuple (not only whole-graph probability evaluation),
  3. computes approximate probabilities with explicit error/confidence controls.

Target flow:

```text
Derivation Graph
  -> Query Planner (resolve query tuples, slicing, grouping)
  -> Formula Compiler (query-rooted formulas)
  -> AMC/WAMC Engine (ApproxMC + optional weighted conversion)
  -> Result Assembly (estimate + bounds + diagnostics)
```

## Scope

Phase-1 scope:
- full-only mode first.
- query-level probability for explicit query tuples (and optional output relation tuples).
- evidence-conditioned probability support in full mode.
- weighted and unweighted counting path in C++.

Out of scope for phase-1:
- incremental CLI integration.
- replacing existing BDD/SDD path.
- exact arithmetic guarantees beyond current floating-point interfaces.

## Existing Hooks To Reuse
- Graph lifecycle and mode orchestration:
  [src/problog/Pipeline.cpp](/home/hugh/research/datalog/souffle/src/problog/Pipeline.cpp)
- Formula construction abstraction:
  [src/include/souffle/problog/formula/FormulaManager.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/formula/FormulaManager.h)
- Cyclewise forward compilation:
  [src/include/souffle/problog/ForwardCompilation.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/ForwardCompilation.h)
- Query slicing/evaluation prototype:
  [src/problog_graph_query.cpp](/home/hugh/research/datalog/souffle/src/problog_graph_query.cpp)
- Weighted conversion experiment:
  [experiments/approxmc_demo/weighted_conversion.cpp](/home/hugh/research/datalog/souffle/experiments/approxmc_demo/weighted_conversion.cpp)
  and
  [experiments/approxmc_demo/weighted_appmc.cpp](/home/hugh/research/datalog/souffle/experiments/approxmc_demo/weighted_appmc.cpp)

## High-Level Architecture

### 1) Query Planner
- Input: pruned derivation view, query declarations, optional evidence.
- Responsibilities:
  - normalize query tuple set,
  - map tuple -> graph node,
  - optionally build per-query backward slices (or grouped slices),
  - preserve query roots across rewrite/simplification.
- Output:
  - query groups,
  - active subgraph per group,
  - metadata for result routing.

### 2) Formula Compiler
- Add a CNF-oriented formula manager (`CnfFormulaManager`) implementing `FormulaManager<NodeRef>`.
- Reuse `buildFormulasCyclewise(...)` to construct formula DAG per node/edge.
- Encode operations with Tseitin variables:
  - `makeAnd`, `makeOr`, `makeNot`.
- Track:
  - CNF clauses,
  - formula root literal for each node,
  - variable weights for random vars (facts and non-deterministic rule edges),
  - sampling set.

### 3) Counter Engine
- Unweighted mode:
  - approximate count on CNF + root/evidence unit clauses.
- Weighted mode:
  - convert weighted CNF to unweighted in memory,
  - run ApproxMC on converted CNF,
  - reconstruct weighted estimate using conversion metadata.

### 4) Result Assembler
- Emit per-query result:
  - `estimate`
  - `belief = 1 - delta`
  - `epsilon`, `delta`
  - `valid`, `unsat`, `message`
  - counter diagnostics (`hashCount`, `cellSolCount`)
  - weighted diagnostics (`tilt`, quantization, added vars/clauses).

## Query-Formula Semantics

For a query tuple `Q`:
- Build a base formula `F` for the active subgraph.
- Identify query root literal `lQ`.

Unconditioned probability:
- weighted measure of `F ∧ lQ`.

Conditioned on evidence `E`:
- numerator: weighted measure of `F ∧ lQ ∧ E`
- denominator: weighted measure of `F ∧ E`
- result: `P(Q|E) = Num / Den`.

For approximate counting, denominator and numerator are both approximate. Confidence composition should be explicit (for example, split `delta` across the two calls).

## Weighted Handling

ApproxMC itself is unweighted. Weighted support should use:
- weighted literal map on CNF vars,
- in-memory weighted-to-unweighted conversion,
- ApproxMC on converted CNF,
- reconstruction via:

```text
weightedEstimate = unweightedEstimate * multiplier / 2^divideExp
```

Important:
- ApproxMC `(epsilon, delta)` guarantee applies to converted unweighted count.
- conversion quantization introduces additional error terms; report separately.

## Integration in Runtime Pipeline

Suggested runtime branching inside `runPipeline(...)`:
- Keep existing `bdd|sdd` path unchanged.
- Add a new optional approximate path gated by a new runtime mode (for example `--prob-engine approx`).
- In phase-1, require `--full-only` for evidence + approx path.

## Build/Link Strategy

Recommended strategy:
- Do not hard-wire ApproxMC as mandatory dependency for all builds.
- Keep core Souffle build working without ApproxMC.
- Enable approx path when ApproxMC headers/libs are available.
- For generated C++ workflows, pass link flags at final compile step (same pattern as existing `-I/-L/-l` usage).

## Optimization Opportunities

1. Query slicing
- Compile only backward slice of queried tuples, not entire graph.

2. Multi-query reuse
- Share base CNF across queries in same slice/component.
- Add only query-specific unit literals per counting call.

3. Evidence reuse
- Cache denominator count for repeated `P(Q|E)` under same evidence.

4. Sampling-set minimization
- Keep projection on semantic random vars; avoid unnecessary auxiliaries.

5. Hybrid fast paths
- Reuse existing single-randvar and conjunction fast paths to skip approximate counting when exact closed form is cheap.

6. Adaptive parameters
- Auto-tune precision/epsilon/delta based on random variable count, tilt diagnostics, and query budget.

## Risks and Constraints

1. Query marking consistency
- Query-node semantics differ between some full/incremental graph creation paths.
- Approx query extraction should rely on explicit resolved tuple set, not implicit flags only.

2. Rewrite interaction
- Rewrite may remove/merge nodes unless query roots are pinned.

3. Cost scaling
- Per-query approximate calls can be expensive for large query sets.
- Need grouping and reuse strategy early.

4. Error interpretation
- Users must see separate approximation error (AMC) vs conversion error (quantization).

## Incremental Extension (Phase-2+)

After full-only stabilizes:
- incremental approximation per commit for impacted query set,
- reuse unchanged component CNFs,
- regional update integration can reuse impacted maps and delta-reach logic from current inc-regional pipeline.

## Proposed Rollout

1. Add standalone internal API
- `QueryFormulaIndex` + `CnfFormulaManager` + `ApproxCounter`.

2. Add full-only runtime option
- run approx on explicit query tuples and write `.prob` output with diagnostics.

3. Add evidence-conditioned path
- numerator/denominator calls and result confidence reporting.

4. Add incremental mode support
- only after correctness/performance baseline is established.

## Verification Plan

- Small deterministic sanity cases: compare approx path to exact BDD/SDD.
- Weighted toy cases: compare against known values and converter demo.
- Query-slice equivalence: full-graph vs slice result consistency for same query.
- Evidence cases: validate numerator/denominator wiring and zero-denominator handling.
- Regression hook: add dedicated regression cases once runtime flags are finalized.

## References
- ApproxMC experiment notes: [amc.md](/home/hugh/research/datalog/souffle/amc.md)
- ApproxMC demo: [experiments/approxmc_demo/README.md](/home/hugh/research/datalog/souffle/experiments/approxmc_demo/README.md)
- Weighted conversion internals:
  [experiments/approxmc_demo/WEIGHTED_TO_UNWEIGHTED.md](/home/hugh/research/datalog/souffle/experiments/approxmc_demo/WEIGHTED_TO_UNWEIGHTED.md)
