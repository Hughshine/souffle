# Conjunctive Rewrite Notes (Historical Design)

## Source references
- [src/include/souffle/problog/GraphRewriter.h](src/include/souffle/problog/GraphRewriter.h)


## Status
- Design/plan document; not the current implementation.
- Current fast paths are summarized in `docs/topics/rewrite/README.rewrite.impl.md`.
- Current SISO detection is fast-path only; dominance-based detection is disabled.

## Scope
- Design/plan document; not the current implementation.
- Full-mode rewrite only; incremental modes do not perform rewrite.

## Current status (2026-01-05)
- The actual fast paths live in component-wise forward compilation (not rewrite).
- Implemented: single-randvar and conj-only components in `ForwardCompilation.h`.
- Constraints for the conj-only fast path: no evidence, no negation, no cycles; otherwise fall back to DD.
- For current behavior and timings, see `docs/topics/rewrite/README.rewrite.impl.md`.

Technical note: DD decomposition for SISO + Fact-Prefix + pure-conjunctive regions
0. Background and goals

The core goal of the existing pipeline is: when constructing DDs (BDD/SDD), structurally decompose the derivation graph to reduce the number of random variables that must be considered at once, making forward compilation scalable.

There are currently two "structured decomposition" paths:

SISO region (entry->exit)

GraphAnalyzer identifies SISO subgraphs in the derivation graph;

GraphRewriter uses forward compilation + WMC on the local subgraph to compute Pr(exit | entry), and compresses the whole region into a summary hyperedge entry -> exit.

Fact-prefix region (cone from input facts to an intermediate node b)

For a node b, build its backward reachable subgraph to the fact frontier;

Run local forward compilation + WMC on this subgraph to compute Pr(b), rewrite b into a probabilistic fact, and delete other nodes/edges in the cone.

This document adds a third fast path:

Pure-conjunctive backward region

For a node b, if its backward reachable region is "acyclic + no disjunction + formula is purely conjunctive", then

Do not use DD; compute Pr(b) via one backward DFS, collect random variables, and multiply directly;

Then reuse Fact-Prefix rewrite to turn b into a fact.

Goal: preserve semantics while avoiding repeated BDD construction on simple structures, reducing CUDD overhead.

1. Concept and semantics: what is a "pure-conjunctive backward region"?

Object: given a node b in the current view (IncSubgraphView).

Consider the backward reachable subgraph R(b) in the view:

Start at b and traverse getIncomingEdges backward,

Continue until the boundary (usually input facts: isFact=true && in-degree=0),

Collect visited nodes and edges into a subgraph.

We define R(b) as a "pure-conjunctive backward region" if it satisfies:

No disjunction (per node)
For each node v in R(b), in the current view:

|incomingEdges(v)| <= 1

That is, no "multiple rules deriving the same conclusion".

In the current graph, input facts have no incoming edges, so there is no mixed disjunction of "fact + rule defining the same node".

Acyclic in the positive dependency graph (for the region itself)

For each node v in R(b), global SCC info shows:

sccSize[v] == 1 and no self-loop;

In other words, R(b) is a DAG in the positive dependency graph and contains no non-trivial SCC nodes.

Other parts (previous strata, etc.) may have cycles as long as those nodes are not inside R(b).

Formula structure is purely conjunctive, no OR

For each rule edge e: inputs -> out, the semantics is still:
F_e = X_e ∧ (∧_{u∈inputs} F_u) (X_e is the edge coin);

For v, because |In(v)| <= 1, it either has no rule (fact), or exactly one rule:
F_v = F_e;

With R(b) acyclic, recursively expanding yields:
F_b ≡ ∧_i L_i
where each L_i is a positive literal X or negative literal ¬X of a primitive random variable (see negation constraints).

Negation constraint: only negate primitive facts

For not A in a rule body, only allow A to be a fact and to have no further rule derivations in R(b) (i.e., A is "atomic" in the region);

In that case, not A's formula is simply ¬X_A, still a literal;

If not applies to a non-fact intermediate node (with its own subformula), the region is not pure-conjunctive and should fall back to BDD.

Under these conditions, F_b is truly a "conjunction of literals":

F_b = ∧_{v∈P} X_v ∧ ∧_{u∈N} ¬X_u

Where:

P: set of primitive random variables appearing positively (fact coin + edge coin);

N: set of primitive random variables appearing negatively.

If a variable appears in both P and N, then X ∧ ¬X ≡ False, and Pr(F_b)=0.

Under the independent-variable assumption (the current model assumption), we can directly compute:

Pr(F_b) = ∏_{v∈P} p_v × ∏_{u∈N} (1 - p_u)

Here p_v is the probability of the corresponding fact or edge coin.

2. Global prerequisites: SCC info and "trivially acyclic" predicate

To detect "acyclic in the region", reuse existing SCC infrastructure:

When building the derivation graph / CycleDependencyGraph, run a global SCC analysis once (as already done before forward compilation).

Record for each node:

sccId[node]: SCC id;

sccSize[sccId]: number of nodes in the SCC;

hasSelfLoop[node]: whether it has a self-loop edge.

Expose a lightweight interface concept to GraphRewriter (names can vary):

bool isTriviallyAcyclic(NodePtr v)

Semantics: sccSize[v] == 1 && !hasSelfLoop[v];

In pure-conj detection, every node in the region must satisfy isTriviallyAcyclic(v).

Key points:
"Acyclic" constrains the region itself, not the whole graph or current stratum;
Other strata or other parts of the graph may have cycles as long as they are not included in the pure-conj region.

3. Detect pure-conj backward region: construct R(b)

In GraphRewriter, when trying the pure-conj fast path for node b, first construct its backward region R(b) and check structure. Suggested flow:

3.1 Backward BFS/DFS construction

Start from b, do a BFS/DFS on the current view (IncSubgraphView):

Data structures:

worklist: node queue or stack;

regionNodes: set (NodePtr set);

regionEdges: set (EdgePtr set).

Pseudo flow (concept):

Init:

Put b into worklist and regionNodes;

If !isTriviallyAcyclic(b), abort pure-conj fast path.

Loop:
Pop v from worklist:

Check isTriviallyAcyclic(v):

If false: v's SCC has a cycle; this region is not suitable for pure-conj; abort construction (return "not pure-conj");

Query incoming edges in the current view:

If |incomingEdges(v)| > 1: disjunction exists (multiple rules derive the same conclusion), abort;

If |incomingEdges(v)| == 0:

v is boundary:

In the current system this means an input fact (isFact=true, in-degree=0);

In the pure-conj scenario this is fine: stop expanding.

If |incomingEdges(v)| == 1: let e be the unique incoming edge:

Add e to regionEdges;

For each source node u in e->getInputs():

Add u to regionNodes;

If u is not a fact, push to worklist and continue backward;

If u is a fact (isFact=true and no incoming edges), treat as boundary and do not continue.

Constraints:
During construction, add size thresholds to prevent the region from becoming too large, e.g.:

regionNodes.size() <= SOUFFLE_PURE_CONJ_MAX_NODES;

regionEdges.size() <= SOUFFLE_PURE_CONJ_MAX_EDGES;
If exceeded, abort and fall back to BDD.

Finally, if no early abort, we obtain:

regionNodes: all nodes in R(b);

regionEdges: all edges in R(b);
and we have ensured:

region is acyclic (all v trivially acyclic);

every v in region has in-degree <= 1 (no disj in view);

boundary nodes are facts (do not expand further);

the region topology is a DAG flowing from b to facts.

3.2 Additional negation constraints (handled in the next probability stage)

When constructing R(b), ignore negation for now; only ensure the structure is acyclic, disj-free, and boundary nodes are facts.

In the later "probability computation" stage, if you find:

a body's negation applies to a non-fact node;

or a negated fact still has further derivations in the region (not primitive),

then the region is no longer a "pure literal conjunction" at the formula level, and should fall back to BDD.

4. Compute Pr(b) on a pure-conj region: literal collection and product

On R(b), assuming the above structural conditions hold, compute Pr(b) without DD, using a closed form.

4.1 Primitive random variables and VarId

Need a unified identifier for primitive random events:

Each probabilistic fact (0<p<1) corresponds to a VarId;

Each probabilistic edge (0<p<1) corresponds to a VarId;

Use an integer id or std::pair<kind,id>, depending on the current implementation.

Provide:

double getProbability(VarId v): return p for that random event;

VarId varOfFact(NodePtr fact);

VarId varOfEdge(EdgePtr edge).

4.2 Literal sets: positive/negative sets + contradiction checks

Construct the formula on R(b):

F_b = ∧_i L_i
L_i ∈ {X, ¬X}

Maintain two sets:

posVars: VarId set appearing as X;

negVars: VarId set appearing as ¬X.

Rules:

Traverse all edges in the region:

For each edge e:

If 0 < prob(e) < 1:

Let ve = varOfEdge(e);

Insert into posVars;

If ve is already in negVars, then X ∧ ¬X ≡ False, immediately set Pr(b)=0 and end the fast path.

Handle body negations:

For each not A:

Require A is a fact and has no further incoming edges in the region (construction already ensured A is a boundary fact);

Let va = varOfFact(A), insert into negVars;

If va is already in posVars, again X ∧ ¬X, Pr(b)=0.

Traverse all fact nodes in the region:

For each fact f:

If 0 < prob(f) < 1:

Let vf = varOfFact(f), insert into posVars;

If vf is already in negVars, Pr(b)=0.

If no positive/negative contradictions arise, we have a self-consistent set of positive/negative literals.

4.3 Probability product

Under the variable independence assumption:

Pr(F_b) = ∏_{v∈posVars} p_v × ∏_{u∈negVars} (1 - p_u)

Implementation:

Iterate posVars, multiply p_v;

Iterate negVars, multiply (1 - p_v);

If any p_v is 0 or 1, normal multiplication handles it (some literals are effectively constants).

If X ∧ ¬X was already detected, return 0.

4.4 Fast path vs fallback

If during literal collection you encounter any of:

negation on a non-fact (or a fact that still has derivations in the region);

cannot assign a clear VarId to some node/edge;

or other implementation-complex cases (e.g., future extensions);

then abandon the pure-conj fast path and fall back to the original BDD-based computeRegionMarginalProbability to preserve correctness.

5. Integration suggestions with GraphRewriter / Pipeline
5.1 Integration point: fact-prefix pass

Currently GraphRewriter roughly has two rewrite types:

Fact-prefix region rewrite:

Construct a region by walking backward from exit=b to the fact frontier;

Use BDD on the region to compute Pr(b);

Set b as fact(prob=Pr(b)) and delete other nodes/edges in the cone.

Entry->exit SISO rewrite:

For a SISO region found by GraphAnalyzer, compute Pr(exit|entry) and insert a summary edge.

The new pure-conj fast path naturally fits inside fact-prefix rewrite as a "try cheap path first, fall back to BDD" branch:

For each candidate exit node b:

First construct R(b) using the pure-conj detection from the previous section (regionNodes/regionEdges):

If construction fails (disj, cycle, region too large, etc.), skip pure-conj and go directly to BDD;

On R(b), attempt pure-conj probability computation:

If successful (including Pr(b)=0 cases):

Use existing fact-prefix rewrite logic to turn b into a fact and delete the other nodes/edges in the cone;

Do not run BDD for this b;

If pure-conj detection/computation fails (negation on non-fact, etc.), fall back to the existing BDD path.

5.2 Order relative to SISO rewrite

Recommended order:

An outer iteration loop (until no rewrite happens in a round);

In each round:

First run fact-prefix + pure-conj fast path;

Then run fact-prefix + BDD-based rewrite (for nodes not handled by pure-conj);

Finally run entry->exit SISO rewrite;

If in a round none of the three rewrites does anything, consider the fixpoint reached.

This way:

The pure-conj fast path is the cheapest and removes 100% conjunctive regions first;

BDD-based fact-prefix handles the remaining "facts-to-b" cones;

SISO handles regions with more complex internal structure but decomposable entry->exit.

5.3 Relation between cycles and strata

Implementation does not need to explicitly reason about "current stratum" vs "previous stratum" boundaries. Just:

Call isTriviallyAcyclic(v) for each node v in the region (based on global SCC);

If any v participates in a non-trivial SCC (regardless of stratum), the region cannot use the pure-conj fast path.

Your current graph setup (input facts have no incoming edges, pruned as boundaries) implies:

Backward cones typically contain only nodes and facts in the current stratum;

Cyclic structures in previous strata will not appear in pure-conj regions (otherwise rules would extend into this region).

6. Runtime flags and tuning suggestions

For easier experimentation/debugging, add a configurable switch and thresholds for the pure-conj fast path:

Environment variables or options:

SOUFFLE_PURE_CONJ_REWRITE (true/false, default on);

SOUFFLE_PURE_CONJ_MAX_NODES (default equal to or smaller than fact-prefix MAX_NODES);

SOUFFLE_PURE_CONJ_MAX_EDGES;

SOUFFLE_PURE_CONJ_MAX_RANDOM_VARS (optional: cap the number of random variables in the region; if too large, still use BDD).

Reading these configs can follow existing SOUFFLE_SISO_MAX_EDGES and SOUFFLE_FACT_PREFIX_MAX_* style.

Add statistics in rewrite.log or console output, e.g.:

pureConjRegions: number of regions that used the pure-conj fast path;

pureConjZeroProbRegions: number of regions where fast path computed Pr(b)=0;

average/max pureConjRegionNodes/Edges, etc.

7. Testing and validation suggestions
7.1 Unit tests

Construct small derivation-graph examples covering:

Pure-conj DAG (no disj, no cycles):

Example: a,b are facts, c :- a (coin), d :- c,b (coin), compute Pr(d).

Compare: BDD pipeline vs pure-conj fast path, ensure results match.

With disjunction:

d :- a, d :- b;

buildPureConjRegionFrom should return "not pure-conj", use BDD;

Fast path should be disabled.

With cycles:

a :- b, b :- a;

Any region containing a or b should be blocked by isTriviallyAcyclic, pure-conj fast path not used.

Negation on fact:

p is a fact, q :- not p;

Backward region is {q, p}, structure has no disj and no cycles;

Pure-conj should identify literals {¬X_p}, Pr(q) = 1 - p(p).

Positive and negative both appear:

Construct a region containing p and not p on the same path (e.g., different rule chains converging to one node),

Literal collection should detect X and ¬X appearing together and return Pr=0.

Negation on non-fact:

q :- p, r :- not q;

When constructing R(r), encounter not q and q is not a fact;

tryComputePureConjProbability should return "cannot handle" (fallback BDD), check results match pure BDD pipeline.

7.2 Integration tests

On existing benchmarks (P9/P10 etc.) run:

baseline: --rewrite off;

rewrite without pure-conj: only existing fact-prefix + SISO;

rewrite with pure-conj: new fast path enabled.

Compare:

Final query probabilities match;

Total runtime, BDD node count, memory;

pureConjRegions count and average region size.

8. Summary

For Codex, this change can be split into three main steps:

Wiring:

Expose an isTriviallyAcyclic(node) interface from existing CycleDependencyGraph/SCC results;

Add a pure-conj fast path invocation in GraphRewriter (before BDD fact-prefix rewrite).

Structure detection:

In the view, construct region R(b) via backward BFS/DFS from the exit node;

During construction check: acyclic (trivial SCC), no disj (in-degree <= 1), boundary is fact, size within thresholds.

Probability computation:

Traverse region edges + facts, collect positive/negative literals of primitive random variables;

If the same VarId appears in both positive and negative, set Pr=0;

Otherwise compute Pr(b) via ∏ p × ∏ (1-p), reuse existing fact-prefix rewrite to turn b into a fact.

Everywhere, implement robust fallback: if any detection step fails, immediately return to the existing BDD-based logic, ensuring correctness first and optimization second.

## Related commits
- `812ea4081` — docs(repo): refine README narratives
- `3e9b024ca` — docs(readme): refresh eval and pipeline notes
- `4dd403de4` — Translate Chinese comments and docs to English
