# Approx Pipeline Design (Derivation Graph -> Query Formula -> AMC)

## Status
- Experimental standalone implementation exists in
  [src/problog_graph_query.cpp](/home/hugh/research/datalog/souffle/src/problog_graph_query.cpp):
  offline `derivation.json` replay supports:
  - `--backend amc` with query slicing, query-local formula extraction,
    weighted-to-unweighted conversion, and an ApproxMC backend
  - `--backend sampling` with query-local Monte Carlo over extracted random
    variables
  - `--backend horn-sampling` with direct derivation-slice-to-Horn compilation,
    world sampling over projected probabilistic supports, and Horn least-model
    closure to answer one or many queries from the same sampled world
  - `--backend pepin` with query-local Boolean extraction followed by bounded
    DNF expansion and a direct in-memory `pepin` library call when the local
    library is available; otherwise it falls back to the older CLI route
  - `--backend schlandals` with direct derivation-slice-to-Horn encoding and a
    direct in-memory `Schlandals` PWMC library call when the local Rust wrapper
    is available; otherwise it falls back to the older CNF-file + CLI path
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
- Standalone `pepin` replay is intentionally narrow:
  - query-local backward extraction with path-local cycle cutting, rather than a
    whole-slice acyclic requirement
  - bounded DNF expansion only (`--pepin-max-terms`)
  - current preferred integration is direct library use through the local
    `pepin` C++ API; the CLI path remains only as a build-time fallback
  - `--pepin-stream-output` now emits lightweight local stage markers
    (`extract_done`, `expand_done`, `counter_done`) so a slow query can be
    distinguished from DNF-front-end work versus the `pepin` counter itself
  - current local probes with those stage markers suggest that on the queries
    tested so far, extraction and bounded DNF expansion are tiny; the dominant
    backend cost is the `pepin` counter itself once the DNF has been materialized
  - when the user does not explicitly pass `--epsilon`, the standalone replay
    now tightens the `pepin` default to `0.01` instead of the global `0.1`,
    because `0.1` was too coarse for the current narrow weighted taint queries
  - current stable default is `--pepin-weight-digits=3`; much larger rational
    denominators can produce numerically implausible weighted estimates in the
    current external counter
  - recursive / cyclic queries may still fail indirectly when the cut formula
    expands to too many DNF terms; the hard limiter remains
    `--pepin-max-terms`, not an explicit acyclic-slice guard
- Standalone `schlandals` replay is also intentionally narrow:
  - acyclic query slices only
  - stratified negation is supported only when each negated body literal points
    to a strictly lower-stratum deterministic relation in the active query slice
  - recursive negation / non-stratified negation is rejected explicitly
  - positive recursive SCCs are still rejected by the current acyclic-slice guard
  - negated literals are compiled away as already-resolved lower-stratum guards;
    they are not emitted as same-stratum non-Horn literals
  - the current prototype encodes each probabilistic fact/edge as a binary
    distribution (`present` / `absent`) and each graph node as a deterministic
    Horn head variable
  - current preferred execution path is in-memory: standalone C++ flattens the
    Horn encoding and calls a small Rust `staticlib` wrapper around the local
    `schlandals` crate, avoiding temporary CNF files and process startup
  - the older `--schlandals-bin` CLI route is retained only as a build-time
    fallback when the direct wrapper is not available
  - queries are evaluated as `1 - P(not Q)` because the Horn backend is a
    natural fit for negative target evidence, not positive target assertions
  - current validation is on small side-channel queries where exact Schlandals
    matches BDD up to decimal rounding and LDS mode returns sensible bounds
  - local stratified-negation validation currently covers both a guard-blocking
    case (`blocked_pass(1) = 0`) and a lower-stratum-absence case
    (`pass(1) = 0.7`), with exact BDD and Schlandals agreeing on both
- Standalone `horn-sampling` replay keeps the same Horn/distribution frontend
  but swaps the backend evaluator:
  - probabilistic choices are sampled only over projected support variables
    (facts / probabilistic rule applications)
  - deterministic Horn/internal variables are not sampled; they are derived by
    a worklist least-model closure
  - unlike the current exact `schlandals` path, positive recursive Horn slices
    are allowed; the closure computes the least fixpoint directly
  - the same sampled world is reused for every query in the active batch, so
    `--query-all` becomes one shared-world multi-query estimator instead of
    independent per-query samplers
  - when sample count is derived rather than fixed, `--delta` is interpreted as
    a per-query confidence target and the backend derives samples directly from
    `(epsilon, delta)` without an extra union-bound correction across the batch
  - current guarantee is therefore per-query additive error under Hoeffding; the
    backend does not currently claim a simultaneous batch confidence guarantee
    for all queries in one run
  - the current execution strategy is no longer plain “sample every support,
    then run full closure”:
    - supports are ordered by reverse relevance to the active query batch
      (how many queries can reach them through Horn producer chains)
    - each sampled world is evaluated incrementally in that support order
    - propagation runs eagerly after each active support draw
    - the world evaluation stops as soon as every query in the batch is already
      true, because additional positive supports cannot invalidate a Horn
      least-model derivation
  - this keeps the same sampled world distribution and therefore the same
    additive Hoeffding guarantee; it only changes the internal schedule of how
    a world is evaluated
  - current local validation covers:
    - small acyclic taint queries (`reachableCI(27,1916)`) close to exact BDD
    - relation-level shared-world sampling on `query-all reachableCI`
    - positive recursive taint query `reachableT(8)`, which the exact Horn
      backend still rejects but Horn sampling evaluates directly
    - stratified-negation guard fixtures (`pass(1)`, `blocked_pass(1)`)
    - thicker side-channel query `P19: KEY_IND(217457)`, where Horn sampling
      is much cheaper than exact Horn search and stays close to exact BDD
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

## Horn-Aware Sampling Design

### Goal

Keep the current `horn-sampling` semantics and confidence story, but exploit
Horn structure during each sampled world evaluation instead of treating world
evaluation as a full replay from scratch.

The design target is deliberately narrow:

- keep projected-support world sampling
- keep least-model Horn closure
- keep per-query additive Hoeffding guarantees
- do not introduce query-specific importance weights yet

### Baseline

The original implementation did:

1. sample every projected support variable
2. materialize the full sampled world
3. run Horn closure to a fixpoint
4. read query truth values

This is correct, but it wastes work in the common high-probability case where
queries become true long before every support choice has been drawn.

### First Integration Step

The current first-step integration is a scheduling optimization inspired by the
Horn/PWMC view of “probabilistic supports + deterministic closure”.

For a fixed query batch:

1. Compile the query-local active slice once into:
   - projected probabilistic supports
   - deterministic Horn rules
   - reverse producer links from heads to the rules that can derive them
2. Score supports by reverse relevance:
   - start from every query head
   - walk backward through Horn producer rules
   - count how many queries can reach each support
3. During each sampled world:
   - draw supports in descending relevance order
   - after each active support, propagate Horn consequences immediately
   - stop the world once all query heads are already true

This is still unbiased Monte Carlo over the same world distribution. The
optimization is purely operational: it reduces support draws and propagation
work for worlds that become decisive early.

### Why This Is Semantically Safe

The current Horn backend only keeps positive Horn rules plus already-resolved
lower-stratum deterministic guards. Under that semantics:

- deriving more supports can only add facts to the least model
- once a query head is true in the current partial world, later support draws
  cannot make it false
- if every query in the active batch is already true, the remaining unsampled
  supports are irrelevant to the batch result

So early stopping on “all queries true” preserves the exact Bernoulli outcome of
every sampled world for the current query batch.

### What This Does Not Do Yet

This first integration step does not yet:

- prove that a still-false query has become impossible under the remaining
  unsampled supports
- change the sampling distribution
- provide multiplicative guarantees
- reuse partial residual bounds from the Horn approximate-counting work

Those are still future work. The current implementation is intentionally the
lowest-risk bridge between the Horn-approx view and the existing Monte Carlo
backend.

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

7. DNF-friendly backends
- When the extracted query formula is naturally a small proof DNF, prefer a
  direct DNF backend over CNF/XOR counting.
- Keep the DNF term cap explicit so backend selection fails loudly instead of
  silently expanding exponentially.

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

5. DNF expansion blow-up
- A generic Boolean DAG is not a proof DNF.
- `pepin`-style counting needs explicit cubes, so the backend must reject or
  cap formulas whose DNF expansion would blow up.

6. Weighted-rational sensitivity
- The current external `pepin` path is sensitive to very large literal-weight
  denominators.
- In local validation, stable results required coarse decimal rationalization
  (`weight_digits=3`), so this backend is currently best viewed as an
  experimental DNF-side probe rather than a claim-grade weighted counter.

7. Backend-specific accuracy knobs matter
- For `pepin`, a loose `epsilon` can look like a semantic bug on tiny weighted
  queries when it is really just approximation error.
- Local taint validation showed that `epsilon=0.1` could turn an exact
  `0.998001` query into roughly `0.983040`, while `epsilon=0.01` moved the same
  query to roughly `0.996896` at higher runtime.

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

## Related commits
- `UNCOMMITTED` — fix(approx): interpret horn-sampling delta as per-query confidence
- `UNCOMMITTED` — feat(approx): add experimental horn-sampling backend with shared-world Horn closure
- `UNCOMMITTED` — feat(approx): add experimental pepin DNF backend to standalone graph-query tool
- `UNCOMMITTED` — docs(approx): record pepin backend scope and current weighted-rational limitation
