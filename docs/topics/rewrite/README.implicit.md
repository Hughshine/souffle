# Implicit Rewrite

## Source references
- [src/include/souffle/problog/ImplicitSplitRewrite.h](src/include/souffle/problog/ImplicitSplitRewrite.h)
- [src/problog/ImplicitSplitRewrite.cpp](src/problog/ImplicitSplitRewrite.cpp)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/include/souffle/problog/GraphRewriter.h](src/include/souffle/problog/GraphRewriter.h)
- [src/include/souffle/problog/GraphAnalyzer.h](src/include/souffle/problog/GraphAnalyzer.h)
- [docs/topics/rewrite/README.rewrite.impl.md](docs/topics/rewrite/README.rewrite.impl.md)
- [docs/research/README.rewrite.status.md](docs/research/README.rewrite.status.md)
- [problog-benchmark/benchmarks/side_channel/cli/side_channel_full.py](problog-benchmark/benchmarks/side_channel/cli/side_channel_full.py)
- [problog-benchmark/taint_inc.py](problog-benchmark/taint_inc.py)

This document is the implementation-facing technical description of implicit
rewrite. It is meant to be detailed enough to serve as source material for a
formal technical report, while still reflecting the current code rather than an
idealized design.

## Status
- Active implementation document for `--implicit-rewrite` and
  `--implicit-iterate-split-rewrite`.
- Describes the current shipped architecture, not a future formula-level
  redesign.

## Scope
- Full-mode probabilistic evaluation only.
- Purpose, problem framing, internal data structures, algorithms, invariants,
  runtime integration, profiling fields, and current limitations.
- Current benchmark rankings and trusted experimental conclusions remain in
  [docs/research/README.rewrite.status.md](docs/research/README.rewrite.status.md).

## Executive Summary
Implicit rewrite is a split-aware rewrite pipeline that tries to preserve the
main benefit of legacy split-enabled graph rewrite without paying the full cost
of explicit graph growth.

Legacy rewrite with split works by materially changing the derivation graph:
- facts are cloned into shadow facts
- some outgoing uses are rewired to those clones
- rewrite then operates on the expanded graph

Implicit rewrite keeps the semantic derivation graph stable and instead builds a
rewrite-local overlay IR:
- split is represented by aliasing fact uses, not by adding graph nodes
- fast-path contractions operate directly on the overlay
- outputs that become directly known can be discharged before any residual graph
  exists
- only the residual that cannot be handled in the overlay is materialized back
  into a normal graph

The net effect is a change in where rewrite spends time:
- legacy explicit rewrite pays for graph expansion and then pays again to reason
  about the larger graph
- implicit rewrite shifts cost into overlay bookkeeping and fast-path state
  maintenance

On important taint stages, especially `typefilter-dlog`, that shift already
changes the dominant runtime from residual graph rewrite to overlay-side work.

## Problem Setting

### Base objects
The probabilistic full-mode pipeline starts from a pruned derivation graph view.
Operationally, the important objects are:
- nodes: tuples or derived facts
- hyperedges: derivation steps
- probabilistic facts: base random variables
- outputs: nodes whose probabilities must be produced
- evidence-bearing nodes: nodes with conditioning obligations

The rewrite problem is to reduce the graph before forward compilation and
weighted model counting without changing final output probabilities.

### Why legacy split exists
A single probabilistic fact may feed several downstream regions. If those
regions are structurally independent enough, treating all uses of the fact as a
single graph attachment can hide rewrite opportunities. Legacy split exposes
those opportunities by duplicating the fact structurally.

This is useful, but it couples two effects that should ideally be separate:
- semantic exposure of rewrite opportunities
- physical mutation and growth of the graph

The second effect is the main cost problem.

### The failure mode of explicit split
On rewrite-heavy workloads, explicit split can become self-defeating:
- split exposes more local rewrites
- but it also increases node and edge counts
- detection and rewrite then operate on a larger graph
- later rewrite rounds pay for the graph growth that earlier split created

This is particularly problematic when the eventual rewrite is dominated by very
simple contractions such as fact folding. In those cases, explicit split pays a
large structural cost to expose reductions that are semantically simple.

## Design Objectives
- Preserve end-to-end output probabilities.
- Preserve useful split-enabled rewrite opportunities.
- Avoid explicit graph expansion whenever possible.
- Let all-facts-heavy workloads terminate inside the overlay without residual
  graph rewrite.
- Stay compatible with the existing runtime pipeline when residual work still
  needs legacy infrastructure.
- Support both one-shot and iterative split/rewrite schedules.

## Non-Goals
- Replace the entire FC/WMC stack.
- Provide a general theorem prover for arbitrary graph equivalence.
- Eliminate residual materialization in every case.
- Guarantee that iterative implicit rewrite strictly dominates single-pass
  implicit rewrite in the current implementation.

## Legacy Rewrite vs Implicit Rewrite

### Legacy rewrite with explicit split
The legacy full rewrite path is conceptually:
1. start from a pruned `IncSubgraphView`
2. optionally perform explicit split on the graph
3. run detection and local probabilistic summarization on the rewritten graph
4. continue downstream on the rewritten residual graph

The implementation is direct because the graph itself is the only IR.

The disadvantage is equally direct: structural refinement is paid for as real
node and edge growth.

### Implicit rewrite
Implicit rewrite adds a second IR between the pruned graph and the downstream
pipeline.

Conceptually:
1. keep the base graph unchanged
2. build an overlay that mirrors the current live structure
3. encode split as aliasing of fact uses
4. execute selected rewrites directly in the overlay
5. materialize only the residual if the overlay cannot finish the whole job

This separates semantic exposure from physical graph mutation.

## Formal Objects and Terminology

### Base graph
The base graph is the pruned derivation graph view entering rewrite. It remains
the semantic source of truth for node identity.

### Overlay
The overlay is a rewrite-local working IR. It owns:
- mutable edge liveness
- mutable input references
- mutable node fact state
- alias bookkeeping for split
- fast-path bookkeeping indices

It does not replace the base graph semantically. It is a rewrite-specific view
that can later be projected back to a normal graph if needed.

### Base node
A base node is the original node identity from the derivation graph.

### Alias
An alias is a structural use partition of a base fact. It is not a new random
variable. This distinction is fundamental.

### Output discharge
An output is discharged when its probability is known directly from overlay
state, so no residual graph work is needed for that output.

### Residual graph
The residual graph is the portion of the workload that the overlay could not
fully eliminate. Only this residual is materialized back to the ordinary graph
world.

## Core Data Structures

### `SplitNodeRef`
Defined in
[ImplicitSplitRewrite.h](src/include/souffle/problog/ImplicitSplitRewrite.h).

`SplitNodeRef` is the basic reference type of the overlay. It consists of:
- `base: NodePtr`
- `alias: size_t`

Interpretation:
- `alias = 0` means the unsplit base structural occurrence
- `alias > 0` means a split structural occurrence of the same base fact

The crucial semantic invariant is:
- `base` determines probabilistic identity
- `alias` only determines structural use partitioning

This is why implicit split can expose more local rewrite structure without
introducing extra random variables.

### Overlay edge record
Each overlay edge stores a mutable copy of the information rewrite needs:
- `baseEdge`
- `inputs: vector<SplitNodeRef>`
- `negations`
- `output`
- `probability`
- `deterministic`
- `active`

This duplication exists because rewrite needs local mutability for:
- edge liveness
- rewritten edge probabilities
- alias reassignment of inputs

without mutating the base graph.

### Base node state
For each base node, the overlay tracks a node-state record containing at least:
- whether the node was originally a fact
- whether the node is currently treated as a fact
- the current fact probability
- whether the node is needed as output
- whether the node carries evidence obligations

This lets the overlay turn derived nodes into fact-like nodes without having to
construct replacement graph structure immediately.

### Active indices
The overlay maintains derived indices such as:
- incoming active edges by base node
- outgoing active edges by `SplitNodeRef`
- the current active edge list
- active edge count

These are optimization structures. They exist to make degree queries and
fast-path detection cheap enough. They are not semantic objects.

### Alias bookkeeping
The overlay also tracks:
- next alias IDs per fact
- currently known aliases per fact
- cached split work keyed by the current outgoing-use set

This is the bridge between split semantics and iterative reuse.

## Overlay Lifecycle

### 1. Construction
The overlay is built from the pruned view. This copies the local edge and node
information needed for rewrite, while preserving base node identity.

### 2. Split refinement
Facts eligible for split are examined. If split is useful, some edge-input
occurrences are rebound from `f@0` to `f@k` for fresh aliases `k`.

This is the core design choice:
- the graph topology is not duplicated
- the reference structure is refined

### 3. Overlay fast paths
The overlay then executes supported local contractions such as `all-facts` and
`single-hyperedge` until an internal fixpoint is reached.

### 4. Output discharge
Any output whose probability is now directly known is recorded as a precomputed
result.

### 5. Residual handling
If active residual structure still remains, the overlay materializes only that
residual back into a normal graph representation.

## Split Semantics

### Why split operates on uses, not nodes
The motivating case is one fact feeding several downstream regions. The semantic
fact is shared, but its outgoing uses may belong to different structural
contexts.

Explicit split expresses this by cloning a node. Implicit split expresses it by
changing input references.

Suppose a fact `f` feeds two branches `A` and `B`.
- explicit split creates `f_shadow` and rewires one branch to it
- implicit split leaves the semantic fact alone and changes one branch input
  from `f@0` to `f@1`

The result is structurally similar for rewrite, but only the explicit version
pays permanent graph growth.

### Split eligibility
Current implementation is conservative. A base fact is only considered if it is:
- originally a fact
- currently a fact
- not an output node
- not evidence-bearing
- not a materialized shadow artifact from a residual step

This keeps split away from semantically sensitive nodes.

### `naive-split`
`naive-split` is the default trusted mode.

For a base fact `f`:
1. collect active outgoing uses from `f@0`
2. for each use, start from the corresponding edge output
3. compute a downstream reachability region
4. determine which outgoing uses overlap downstream
5. keep one group on `alias = 0`
6. assign fresh aliases to additional disjoint groups

The key observation is that split is driven by downstream overlap, not by local
syntactic pattern names.

### Immediate-sink shortcut
If all successors are immediate sinks, the implementation bypasses general BFS
and groups by sink directly. This is important for all-facts-heavy workloads,
where a general reachability computation would be wasted work.

### `complete-split`
The code also supports `complete-split`, which is more aggressive. The currently
trusted runtime path is still dominated by `naive-split`, so that is the mode
this document emphasizes.

## Split Cache and Incremental Reuse
The current split cache is pragmatic rather than fully formal.

Cached unit:
- base fact

Approximate cache key:
- current active outgoing base-edge set of that fact

Operational meaning:
- if the fact has the same current active outgoing-use set as before, reuse the
  previous split decision and skip repartitioning

This is enough to remove large amounts of repeated split work in iterative mode.
It is not yet a full proof-driven incremental overlap framework.

## Overlay Fast-Path Rewrite Families
The overlay directly implements all fast-path rewrite families that are
currently active in the legacy detector:
- `all-facts`
- `single-hyperedge`
- `linear-two-edge`
- `parallel-edge`
- `fan-out-converge`

Anything outside those families remains a residual-graph concern.

### `all-facts`
This is the dominant case for taint `typefilter-dlog`.

Condition, informally:
- an active edge has only fact-like inputs
- the edge can therefore be summarized locally

Operational effect:
- compute the output probability from input facts and edge semantics
- mark the output as currently fact-like
- deactivate the edge

Why it matters:
- this turns large regions of rewrite into direct probability propagation
- it is the main reason implicit rewrite can finish some stages without any
  residual graph rewrite

### `single-hyperedge`
This handles a local one-edge structure that can be safely summarized in the
overlay.

Operationally:
- identify a valid single-entry local shape
- compute the replacement probability locally
- update node and edge state in the overlay

Compared with `all-facts`, this is a more selective fast path and is less often
the sole dominant effect on taint-like workloads.

### `linear-two-edge`
This contracts a two-edge chain:
- `entry -> mid`
- `mid -> exit`

under the same structural conditions used by the legacy fast path:
- both edges are single-input
- `mid` has exactly one incoming and one outgoing active edge
- `mid` is not query/evidence-bearing
- the second edge is not negated on `mid`

The overlay rewrite replaces the chain by one synthetic edge
`entry -> exit` with probability `p1 * p2`.

### `parallel-edge`
This contracts multiple single-input edges with:
- the same input reference
- the same output node
- the same body polarity

The overlay rewrite replaces them by one synthetic edge with effective
probability:
- `1 - Π(1 - p_i)`

### `fan-out-converge`
This contracts a fact-driven shape:
- one fact-like entry fans out through several single-input edges
- each branch output is used only once
- the branch outputs converge through one positive multi-input edge

The overlay rewrite removes the fan edges and convergence edge, then replaces
them with one synthetic edge from the original fact reference to the exit. The
new edge keeps the shared fact as an input and absorbs only the branch-edge and
convergence-edge probabilities, so alias structure and base-fact semantics stay
intact.

### Fixpoint discipline
Fast paths run to fixpoint inside the overlay.

Single-pass implicit rewrite:
1. build overlay
2. split once
3. run fast paths to internal fixpoint
4. discharge outputs
5. materialize only if needed

Iterative implicit rewrite:
1. build one persistent overlay
2. repeat selected split plus fast-path rounds on that same overlay
3. stop when another round is not justified
4. materialize at most once at the end

## Output Discharge
Output discharge is one of the most important benefits of the overlay design.

An output can be discharged when:
- it has become directly fact-like with a known probability, or
- the overlay has recorded a direct tuple-level probability for it

If all outputs are discharged:
- no residual materialization is needed
- no residual graph rewrite is needed
- no residual FC/WMC work is needed

This is the cleanest execution mode and already occurs on large taint stages.

## Materialization and Residual Handoff
Materialization exists for compatibility, not because it is the main objective
of the design.

When the overlay cannot finish the whole job, it produces a
`MaterializedImplicitSplitGraph` that contains:
- a residual `IncrementalDerivationGraph`
- the live residual nodes and edges
- output nodes
- diagnostic counts such as alias-node materialization information

### Why materialization still exists
The broader runtime still relies on legacy infrastructure for:
- residual graph rewrite beyond the overlay fast paths
- FC/WMC on the residual graph path

So the current architecture is intentionally hybrid:
- implicit overlay first
- residual legacy pipeline second

### Critical correctness rule
A rewritten residual edge must be synthetic if its probability changed inside
the overlay.

Concretely:
- the materialized residual edge must not retain the original `Rule*`

Reason:
- keeping the original rule lets later code restore the original rule
  probability
- that would silently erase the overlay rewrite result

This was the main correctness bug fixed during runtime stabilization.

## Runtime Integration
The runtime entry point is in [Pipeline.cpp](src/problog/Pipeline.cpp).

### Runtime flags
From [CompiledOptions.h](src/include/souffle/CompiledOptions.h):
- `--implicit-rewrite`
- `--implicit-iterate-split-rewrite`

The iterative flag implies the implicit path and enables the persistent-overlay
outer loop.

### High-level runtime flow
Under `opt.isImplicitRewriteEnabled()` the current runtime does:
1. start from the pruned full-mode view
2. build `ImplicitSplitPipelineOptions`
3. call `runImplicitSplitRewritePipeline(view, options)`
4. receive:
   - residual materialization, if any
   - live residual nodes and edges
   - directly discharged tuple probabilities
   - profiling counters and timings
5. determine whether all outputs are already covered
6. if needed, recover isolated fact outputs from the live rewritten view
7. continue the rest of the pipeline on the residual view plus precomputed
   outputs

### Handoff semantics
The handoff must preserve the live rewritten view, not the whole base graph.

This sounds obvious, but it is a real correctness requirement. If the runtime
reconstructs the downstream view from the base graph instead of the rewritten
live set, rewrite-eliminated structure reappears and both performance and
probability semantics can be corrupted.

## Single-Pass Algorithm Sketch
A compact algorithmic sketch of the current non-iterative path is:

1. Build overlay from the pruned view.
2. Identify split-eligible facts.
3. Apply split by rebinding selected input occurrences to fresh aliases.
4. Rebuild active indices needed for fast paths.
5. Run overlay fast paths to fixpoint.
6. Collect directly discharged outputs.
7. If no residual active structure remains, return only precomputed outputs.
8. Otherwise materialize the residual graph.
9. Optionally run residual legacy graph rewrite.
10. Hand residual graph plus precomputed outputs to the downstream probabilistic
    pipeline.

## Iterative Algorithm Sketch
The iterative path keeps the same overlay alive across outer rounds.

1. Build one overlay.
2. Run an outer round consisting of:
   - split work that is still justified
   - overlay fast paths to fixpoint
3. Decide whether another outer round can expose new profitable structure.
4. If no, stop.
5. Materialize at most once, after the final overlay round.
6. Run residual legacy handling at most once.

This persistent-overlay structure replaces the older, more expensive pattern of
repeatedly bouncing between overlay and ordinary graph worlds.

## Correctness Invariants
The implementation depends on the following invariants.

### 1. Aliases are structural only
All aliases of one base fact denote one semantic probabilistic variable.

### 2. Fact lookup is alias-stable
Alias references inherit the base fact probability. Alias changes must not
create separate probabilistic identities.

### 3. Output and evidence obligations are preserved
Split and local rewrite must not casually erase output identity or evidence
semantics.

### 4. Rewritten residual edges remain rewritten after materialization
Any rewritten probability that leaves the overlay must survive the transition to
a materialized graph.

### 5. Handoff preserves the rewritten live view
The downstream pipeline must continue from the rewritten residual, not from the
whole original graph.

### 6. Matched-input provenance is respected in experiments
Stage-local taint comparisons are only valid when all compared runs consume the
same matched stage input.

## Correctness Argument Sketch
The current implementation is not presented as a machine-checked proof, but the
intended semantic argument has a clear decomposition.

### Split soundness
Implicit split is sound because it does not duplicate probabilistic identity. A
split alias changes only which structural use-site a fact occurrence belongs to.
All aliases still refer to the same base fact and therefore to the same
underlying random variable.

### Fast-path soundness
Overlay fast paths are intended to be local probability-preserving
transformations. Each successful fast path replaces a small local derivation
shape by an equivalent summary in overlay state:
- `all-facts` replaces an edge whose inputs are already known facts by a direct
  fact state on the output
- `single-hyperedge` replaces a locally isolated one-edge shape by an equivalent
  summarized effect

The invariant that matters is not graph isomorphism. It is preservation of the
probability of every still-live output under the same base random variables.

### Output discharge soundness
Directly discharged outputs are sound only because they are recorded as final
probability results, not as disposable intermediate observations. Once an output
is discharged, the downstream pipeline must not attempt to recompute it from a
stale residual graph.

### Residual handoff soundness
Residual materialization is sound only if the residual graph faithfully
represents the current live overlay state. That requires two things:
- live-set preservation: only still-live rewritten structure is materialized
- rewritten-probability preservation: any rewritten edge that leaves the
  overlay remains rewritten after materialization

### End-to-end soundness obligation
The full runtime is correct when the union of:
- directly discharged output probabilities
- probabilities computed from the residual graph

coincides with the output probabilities of the original pruned graph. In the
current implementation, this is validated operationally by regression tests and
matched benchmark comparisons rather than by a separate formal proof artifact.

## Profiling Fields and Their Meaning
The runtime logs several implicit-specific fields.

### `implicit rewrite total_ms`
Overall wall time spent inside the implicit rewrite pipeline.

### `overlay_prep_ms`
Time spent building the overlay and preparing its derived bookkeeping. This is
currently the dominant rewrite-side cost on many successful implicit runs.

### `overlay_split_ms`
Time spent deciding and applying split aliases.

### `overlay_fastpath_ms`
Time spent inside overlay-side fast-path rewrite.

### `materialize_ms`
Time spent projecting residual overlay state back to a normal graph. On the best
implicit cases, this drops to zero.

### `graph_rewrite_ms`
Time spent in residual legacy `GraphRewriter` after overlay work. On strong
implicit cases, this is also often negligible.

### Counters
The implementation also reports counters such as:
- alias counts
- fast-path fire counts
- residual node and edge counts

These are useful for distinguishing:
- semantic success with low residual work
- semantic success with heavy overlay overhead
- failure to reduce the residual at all

## Cost Model and Complexity Intuition
The dominant costs are not all of the same kind.

### Overlay construction and prep
This includes:
- copying rewrite-relevant edge state
- building node-state records
- initializing active indices
- establishing alias bookkeeping

### Split work
This includes:
- identifying eligible facts
- computing naive overlap structure
- rebinding input occurrences to aliases

### Fast-path bookkeeping
This includes:
- candidate scans
- degree queries
- active-index rebuilds
- repeated fixpoint iterations

### Residual compatibility cost
If the overlay cannot finish the whole job, additional cost appears in:
- materialization
- residual graph rewrite
- residual downstream FC/WMC

The current performance trend on strong implicit runs is encouraging because it
shows that the old residual compatibility cost is no longer dominant. The main
optimization frontier has shifted to overlay maintenance itself.

## Worked Example
Consider a fact `f` with two outgoing branches, `A` and `B`.

### Without split
Both branches read from one structural source `f@0`. If the rewrite logic uses
local degree and boundary structure, the shared source can hide the fact that
these branches are independent enough to summarize separately.

### Legacy explicit split
Explicit split creates a second node, such as `f_shadow`, and rewires one branch
to it. Rewrite now sees two separate structural sources, but the graph has grown.

### Implicit split
Implicit split keeps the base fact untouched and changes only the edge-input
references:
- branch `A` still consumes `f@0`
- branch `B` consumes `f@1`

From the perspective of rewrite, the two uses are now distinct. From the
perspective of probability semantics, they still refer to the same underlying
fact variable.

This example captures the entire point of the overlay design:
- expose structure
- do not pay permanent graph growth

## Current Performance Reading
The compact trusted status is maintained in
[docs/research/README.rewrite.status.md](docs/research/README.rewrite.status.md).
At the implementation level, the current picture is:
- successful implicit runs often make `materialize_ms` and `graph_rewrite_ms`
  almost disappear
- rewrite-side time is then dominated by `overlay_prep_ms` and
  `overlay_fastpath_ms`
- full taint chain totals are still dominated by a small number of heavy stages,
  especially `typefilter-dlog` and `pt-obj-dlog`
- iterative implicit rewrite is correct and sometimes useful, but still has not
  produced a qualitative regime change over single-pass implicit rewrite

## Limitations
- Only the currently active fast-path families are handled fully inside the
  overlay.
- Anything beyond those fast paths still relies on residual graph fallback.
- More general residual structures still need legacy handling.
- The split cache is practical rather than fully formal.
- Iterative mode is not yet consistently better than single-pass implicit.
- The current design is still graph-oriented; it is not yet a formula-level
  rewrite framework.

## Open Directions
- reduce active-index rebuild cost further
- make iterative round selection more selective
- extend overlay-side handling so fewer residuals need legacy machinery
- clarify whether a future formula-level IR should replace residual graph
  materialization entirely

## Related commits
- `UNCOMMITTED` — docs(rewrite): expand implicit rewrite into a report-grade technical note
- `b0b178aab` — perf(problog): refine implicit rewrite and research docs
- `736f94f77` — refactor(problog): prototype SCBF and implicit rewrite paths
