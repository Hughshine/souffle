# Approx Pipeline Design (Derivation Graph -> Query Formula -> AMC)

## Status
- Experimental standalone implementation exists in
  [src/problog_graph_query.cpp](/home/hugh/research/datalog/souffle/src/problog_graph_query.cpp):
  offline `derivation.json` replay supports `--backend amc` with query slicing,
  query-local formula extraction, weighted-to-unweighted conversion, and an
  external ApproxMC-compatible CLI.
- Not implemented in the main runtime pipeline yet
  ([src/problog/Pipeline.cpp](/home/hugh/research/datalog/souffle/src/problog/Pipeline.cpp)).
- Side-channel full usage currently goes through the standalone replay path after
  generating `output/derivation.json`; it is not wired into the benchmark CLI as
  a first-class backend.
- Standalone AMC replay now extracts each query formula with a query-local
  backward traversal plus path-local cycle cutting, instead of using
  `buildFormulasCyclewise(...)` over a symbolic DAG manager. This avoids relying
  on semantic fixpoint equality that holds for DD managers but was too weak for
  the symbolic AMC front-end on recursive taint queries.
- Standalone AMC replay also exposes basic progress instrumentation:
  `--amc-verb <n>` passes ApproxMC verbosity through to the child process, and
  `--amc-stream-output` streams child output plus local AMC stage markers so
  long-running queries can be distinguished from front-end stalls.
- AMC per-query diagnostics now include weighted/unweighted CNF sizes,
  projection-set size, added vars/clauses from weighted conversion, tilt, and
  quantization error. These are emitted in `[amc-run]` / `[amc-query]` so
  stalled ApproxMC runs can be distinguished from front-end or conversion blowup.
- When the build can resolve `approxmc` via `find_package(approxmc)`,
  `souffle-problog-graph-query` now prefers the direct C++ library path
  (`ApproxMC::AppMC`) over the external CLI shim. Without that package it falls
  back to the existing CLI contract.
- On formal sampled taint bundles
  `problog-benchmark/runs/taint_inc_eval_final/v1_allrules0999_noderv0` and
  `problog-benchmark/runs/taint_inc_eval_final/v2_semantic_subsetprob_noderv0`,
  `no rewrite + --query-all` on `andors-trail / pt-obj-dlog` is currently
  relation-dependent:
  - `reachableCI` completes in about `2.0s` to `2.3s`, but all completed AMC
    queries still collapse to `unweighted_cnf_clauses=0`.
  - `CIC` completes for all `829` tuples in about `16.9s` to `19.0s`, again
    with `unweighted_cnf_clauses=0` for every query.
  - `pt`, `reachableCM`, and `reachableT` can stall in
    `buildFormulasCyclewise(...)` before AMC runs, so query-all suitability is
    not uniform across relations.
  - `--amc-no-preprocess` fails quickly on these bundles with
    `Zero literal weight remains after preprocessing`; deterministic facts leave
    zero-weight polarities that the current converter still requires
    preprocessing to eliminate.
  - After switching to query-local backward cycle cutting, recursive taint heads
    such as `reachableT(8)` no longer pseudo-diverge in symbolic extraction;
    they reach weighted conversion immediately. Low precision now quantizes
    near-1 weights into forced assignments instead of failing, which can collapse
    the whole query to a deterministic answer (for example `reachableT(8)` goes
    to `1` at precision `2`). Higher precision keeps more weighted structure and
    can push the same query through to ApproxMC, so the remaining issue is
    conversion fidelity and SAT cost, not symbolic fixed-point non-convergence.
  - On the same query-all sets, the current exact BDD replay path is still much
    faster than AMC, so the standalone AMC path is useful for backend probing
    but is not yet a competitive default on these natural taint outputs.

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
  [src/problog/approx/WeightedConversion.cpp](/home/hugh/research/datalog/souffle/src/problog/approx/WeightedConversion.cpp)
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
