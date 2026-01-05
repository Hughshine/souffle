# Deterministic-First Constant Strategy (Const v2)

This document rethinks "const" handling for the probabilistic pipeline. The current
const pre-analysis (Const-FC) only short-circuits formula construction after the
derivation graph exists. That is too late for workloads dominated by deterministic
relations (probability 1). The approach here is to avoid recording derivations for
deterministic relations from the start. This doc tracks both design and the
implemented pieces: `--det-opt` computes/dumps determinism and gates
`RecordDerivation` + graph construction behavior. All current evaluations should
run with `--det-opt` enabled.

## Problem statement
- Const-FC runs inside forward compilation, after graph construction and pruning.
- Deterministic relations can generate huge derivation graphs even though their
  probability is always 1 and derivation multiplicity does not matter.
- Examples: assign/equal_assign in side_channel behave like transitive/eqrel closures,
  producing many derivations that never affect probabilistic outputs.

## Core idea
Only probabilistic choices need derivation edges. Deterministic relations should be
evaluated normally, but recorded as existence-only facts (no derivation tracking).
The derivation graph and forward compilation then only cover probabilistic structure.

## Full pipeline map (current) with optimization points
0) Codegen entry and pipeline hook
   - `src/synthesiser/Synthesiser.h` / `src/synthesiser/Synthesiser.cpp`
   - `emitProblogPipeline` calls `runPipeline(...)` after RAM execution.
1) RAM execution (semi-naive evaluation)
   - `ram::RecordDerivation` nodes are emitted for each rule application and populate
     `DerivationManager::untypedTuple2RuleApplications`.
   - Optimization point: skip RecordDerivation for deterministic relations.
2) Derivation graph construction
   - `IncrementalDerivationGraph::createFrom(...)` in
     `src/include/souffle/problog/DerivationGraph.h`.
   - Optimization point: only probabilistic rule apps produce hyperedges.
3) Prune (output and evidence reachability)
   - `DerivationGraph::prune(...)`.
4) Optional rewrite (SISO)
   - `GraphAnalyzer` + `GraphRewriter` (full-only).
5) Forward compilation (BDD/SDD)
   - `buildFormulasCyclewise` in `src/include/souffle/problog/ForwardCompilation.h`.
   - Existing const-pre (`--fold-const` / `--dumpconst`) still applies.
6) WMC and output
   - `runPipeline` in `src/problog/Pipeline.cpp`.

## Determinism classification (strict)
We need a static relation-level flag `is_probabilistic` (or `needs_derivation`).
Strict definition: a relation is deterministic iff every derivation of its tuples
has probability 1 (no random choices). This can be decided conservatively at the
relation level as a least fixed point.

Algorithm (relation graph + fixpoint):
1) Build a relation dependency graph: for each rule `H :- L1,...,Ln`, add edges
   `rel(Li) -> rel(H)` for all body literals (negated or not). Rules with p=0 can
   be ignored because they never contribute.
2) Seed `Prob` with:
   - any relation that has an input fact with 0 < p < 1 (`.prob` / `fact_prob`);
     missing `.prob` implies p=1, and p=0 is deterministic false.
   - any rule head with 0 < p < 1.
3) Propagate `Prob` to a fixpoint:
   - if a rule body references any `Prob` relation, then its head is `Prob`
     (even if the rule itself has p=1).
   - collapse SCCs in the relation dependency graph; if any relation in an SCC
     is `Prob`, mark the whole SCC `Prob`.
4) All relations not in `Prob` are deterministic.

This is equivalent to "propagate determinism only through deterministic rules":
`H` is deterministic iff every rule deriving `H` has p=1 and all body relations
are deterministic; SCC handling ensures recursion is treated as a unit. Input
relations that also have rules are handled the same way: they are deterministic
only if their SCC is deterministic and all their input fact probabilities are 1.

Incremental note: if updates can introduce 0 < p < 1 facts for a relation, either
recompute this classification after updates or mark that relation probabilistic
from the start.

Evidence note (temporary): treat relations that appear in evidence as deterministic
for now (TODO: tighten; false evidence should be treated as a contradiction).

## Det analysis pass (`--det-opt`)
Goal: compute the relation-level `det` map early enough to skip `RecordDerivation`.

Inputs:
- Relation SCCs from the existing analysis (`SCCGraphAnalysis`,
  `TopologicallySortedSCCGraphAnalysis`).
- Rule probability seeds (any rule with 0<p<1 marks its head relation).
- Fact probability seeds (any input fact with 0<p<1 marks its relation).

Execution order (full mode, current implementation):
1) Prepass: read `.facts` + `.prob` once to populate `fact_prob` and per-relation
   `hasProbFact` (only for probabilities; the main IO load still happens later).
2) Run det analysis using SCC topo order.
3) Dump `det-relations.txt` / `det-scc.txt`.
4) Execute RAM evaluation with `RecordDerivation` gated by `rel_is_det`
   (only when `--det-opt` is enabled).

Dump (always when `--det-opt` is enabled):
- `det-relations.txt` (per relation: scc id, rule seed, fact seed, final det/prob).
- `det-scc.txt` (SCC summary).

Pseudocode (relation-level):
```text
// Inputs (compile-time metadata + runtime seeds)
rels, rel_to_scc, scc_topo_order, scc_succ
rule_seed[rel]   // any rule with 0<p<1 derives rel
fact_seed[rel]   // any input fact with 0<p<1 in rel
evidence_rel[rel] // TODO: currently force det, so do not seed Prob

prob_scc[scc] = false
for rel in rels:
  if rule_seed[rel] or fact_seed[rel]:
    prob_scc[rel_to_scc[rel]] = true

for scc in scc_topo_order:
  if prob_scc[scc]:
    for succ in scc_succ[scc]:
      prob_scc[succ] = true

rel_is_det[rel] = !prob_scc[rel_to_scc[rel]]
```

## One-pass `.prob` prepass (facts read twice)
Simplest implementation: keep Souffle's normal IO load, but add a prepass that
reads `.facts` + `.prob` once to fill `fact_prob` and `hasProbFact`.
- `.prob` is read exactly once.
- `.facts` is read twice (prepass + normal IO load), which is acceptable for now.
- In `--det-opt` mode, this prepass runs before `runAll()` and the post-`runAll()`
  `.prob` scan is skipped. In normal mode the existing post-`runAll()` scan remains.

Pseudocode (prepass):
```text
open facts file for relation R
open prob file for relation R if it exists
for each facts line:
  tuple = parse(fields)
  prob = prob_file ? parse(prob_line) : 1.0
  fact_prob[tuple] = prob
  if 0 < prob < 1: hasProbFact[R] = true
```

## Implementation status (det-opt gating)
- `src/include/souffle/CompiledOptions.h` adds `--det-opt` flag, help text, and getter.
- `src/synthesiser/Synthesiser.cpp` emits SCC metadata + seeds, runs det analysis
  + dumps before `runAll()` (with timing logs), and adds the `.facts`+`.prob` prepass.
- `src/synthesiser/Synthesiser.cpp` gates `RecordDerivation` when `--det-opt`
  and the head relation is deterministic; normal runs are unchanged.
- `src/include/souffle/Derivation.h` / `src/Derivation.cpp` host `fact_prob` and the
  new `relationHasProbFact` map used by det analysis.
- `src/include/souffle/problog/DerivationGraph.h` marks nodes from deterministic
  relations as `isFact` during graph construction when `--det-opt` is enabled.

## Stage-by-stage optimization detail
### 1) RAM translation and synthesis (semi-naive)
- `ram::RecordDerivation` is the only place that creates derivation metadata.
- If a rule head relation is deterministic, do not emit RecordDerivation for it.
- This avoids filling `DerivationManager` with redundant rule applications.

### 2) DerivationManager and graph build
- `DerivationManager::untypedTuple2RuleApplications` will contain only probabilistic
  rule applications.
- `createFrom(...)` builds edges only from those rule apps, so deterministic-only
  subgraphs disappear.
- Deterministic tuples still appear as fact nodes when they are used by a
  probabilistic rule or are outputs, but they have no incoming edges.
- In `--det-opt` runs, any node whose relation is deterministic is marked
  `isFact=true` during graph construction, so empty rule applications are treated
  as existence-only facts.

### 3) Prune and rewrite
- Same algorithms, smaller graphs.
- Eqrel-style merging (`README.eqrel.md`) becomes a secondary cleanup, not a primary fix.

### 4) Forward compilation
- Deterministic fact nodes map to `True` formulas (no BDD/SDD variables).
- Existing const-pre still helps for late constants, evidence conditioning, or p=0 edges.

### 5) Incremental updates
- Deterministic relations still participate in delta evaluation (tuple existence).
- Skip delta-derivation bookkeeping for deterministic relations.
- Probabilistic rules depending on deterministic tuples are still recorded, so the
  derivation graph remains correct.

## Correctness constraints
- If a relation can depend on any probabilistic fact or rule (including negation),
  it must be marked probabilistic.
- Deterministic relations are existence-only; multiple derivations do not change
  truth or probability.
- Evidence handling is a TODO; current plan treats evidence relations as det.

## Expected impact
- Smaller derivation graphs and faster graph construction.
- Fewer rule application records in DerivationManager.
- Faster forward compilation due to fewer edges and variables.
- Large wins for deterministic transitive or eqrel-style relations such as
  assign/equal_assign in side_channel.

## Relation to current Const-FC
- The existing const pre-analysis in `ForwardCompilation.h` is a late-stage
  micro-optimization that does not address graph size.
- Deterministic-first recording is the primary strategy; Const-FC remains as a
  safe fallback for any remaining constants.
