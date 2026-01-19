# Inc-Regional (Insertion) – Paper-Formalization Implementation Notes (CAV'26 §6.1)

This report documents a **drop-in replacement** for the existing `inc regional` insertion-update region analyzer (the one based on **least parents + scopes + outward growth**) and explains how the new implementation follows the **CAV 2026 Incremental Compilation** paper’s region-identification formalization (Section 6.1, incremental BDD construction).

The code artifact accompanying this report is:

- `IncRegionAnalyzer_PaperFormal.h`

It preserves the external API (`Region`, `Boundaries`, `IncRegionAnalysis`, `RegionAnalyzer::analyze(...)`) so that downstream components (e.g., `RegionalIncrementalForwardCompilation`, `RegionSccClosure`, `BoundaryGateCalibrator`) can remain largely unchanged.

---

## 1. What changes (high level)

### Old implementation (current repo)

The existing `inc regional` insertion path does roughly:

1. Compute delta-reachable subgraph.
2. Compute **least parents** for each delta source (expensive).
3. Compute **scopes** for those least parents.
4. Start from a delta seed region and **grow outward** until every boundary node has a “mergeable anchor” (then calibrate).

The biggest performance bottleneck is the **least parents** analysis, which can be much larger than the delta region and has high redundancy.

### New implementation (this patch)

The new analyzer is based directly on the paper’s viewpoint:

- Instead of growing a region from delta, we decide **which nodes can be reused** (paper: **reusability**) and define the region as **exactly the nodes that are not reusable** (paper: Theorem 2).

- Boundaries and anchors emerge naturally:
  - A boundary node is a node inside the region with at least one outgoing dependency edge to a node outside the region.
  - Anchor candidates are old, non-deterministic, non-subsumed incoming edges into a boundary node.

This yields a region whose size is driven by:

- `|∆fact|` (affected atoms from dependency closure)
- the “true correlation surface” induced by delta

…and does **not** require least parents computation.

---

## 2. Mapping paper concepts to code

The paper section relevant here is the incremental BDD generation step:

- **Step 1: DepAnalysis** – compute affected atoms `∆fact`
- **Step 2: SelectRegion** – compute recompilation region `R` via reusability

The implementation directly mirrors this.

### 2.1 Sources `S` (paper Definition 9)

Paper: source atoms are the atoms that appear in the delta derivations.

Code:

- `sources_` is built as the union of:
  - delta-insert nodes (`view.getDeltaInsertNodes()`)
  - heads and bodies of delta-insert edges (`view.getDeltaInsertEdges()`)
  - plus any `delta_input_facts` passed into `analyze(...)` (kept for backward compatibility)

This addresses the earlier mismatch you called out: **the previous interface only passed heads/nodes**, but the paper’s formal `S` includes **bodies as well**.

### 2.2 Affected atoms `∆fact`

Paper: dependency closure in the flattened graph (Definition 8).

Code:

- `reach_filter_.nodes` and `.edges` store `∆fact`.
- The analyzer uses:
  - `view.getDeltaInsertReachableNodes/Edges()` if available, otherwise
  - recomputes a DepAnalysis-style closure using a BFS over the **flattened graph**.

Flattened-graph edges are implicit: for each hyperedge `(B → H)`, we treat each `b ∈ B` as an arc `b → H`.

### 2.3 Reusability and region `R` (paper Definition 11 + Theorem 2)

Paper:

- A node is reusable (w.r.t each source) if:
  - (R1) it has no new incoming derivation edges, and
  - (R2) for each incoming edge and each body atom, either that body atom is a **summary node** for the head or it is reusable.

Region:

- `R = { v ∈ ∆fact | v is not reusable }`

Code:

- We compute *non-reusable* nodes by a **worklist BFS** per **source SCC** (sources grouped by SCC because they have identical reachability):
  1. Seeds: nodes that must be rebuilt immediately
     - all delta-insert nodes
     - all heads of delta-insert edges (R1 violation)
  2. Propagation: if `b` is non-reusable and `b → t` is a flattened arc, then `t` becomes non-reusable *unless `b` is the summary predecessor for `t` wrt the current source*.

Finally, we union the non-reusable sets over all source SCCs to obtain the region nodes.

---

## 3. Summary nodes: implementable criterion

The biggest conceptual shift you pointed out is correct:

- **Old code**: grow from delta outward
- **Paper**: define whether a node **needs update** based on **reusability**, and only rebuild those.

The reusability recursion depends on **summary nodes** (paper’s Section 6.1).

### 3.1 Paper condition

A predecessor `t′` is a summary node of `t` w.r.t. a source `s` if:

- (C1) `t′ → t` is a flattened edge (direct predecessor)
- (C2) single-entry requirement:
  - every path `s ⇝ t` ends with `t′ → t`
  - and any atom that can influence `t′` cannot reach `t` except through `t′ → t`
- (C3) there exists an old incoming edge `e*` into `t′` whose label variable occurs in the old BDD of `t` (and is non-subsumed)

### 3.2 Code criterion

In the code, for a fixed **source SCC** `sid` and target node `t`, we compute **at most one** summary predecessor:

1. Compute `Pred(t)` = all flattened predecessors (all body atoms of all incoming hyperedges).
2. Compute which predecessors are reachable from `sid`.
   - If there is **not exactly one** reachable predecessor, there is **no summary node**.
   - If there is exactly one reachable predecessor, that node is the only candidate `cand`.
3. Enforce the “no correlation through shared dependencies” portion of (C2):
   - For every other predecessor `p ≠ cand`, require `Dep(cand) ∩ Dep(p) = ∅`.
4. Enforce (C3) (engineering approximation described below):
   - require that `cand` has at least one anchor candidate edge.

If all checks pass, `cand` is the summary node.

### 3.3 Efficient `Dep(x) ∩ Dep(y)` check (why SCC matters)

The naive way to test `Dep(cand) ∩ Dep(p) = ∅` is to compute full reverse-reachability sets over the node graph.

Instead, we do this on the **SCC condensation DAG** (`CycleDependencyGraph`):

- Two nodes have intersecting dependency sets iff their SCC ancestor sets intersect.

So `Dep(cand) ∩ Dep(p) = ∅` becomes:

- `AncScc(scc(cand)) ∩ AncScc(scc(p)) = ∅`

This is computed with **memoized closure** on the SCC DAG, and is typically far cheaper than node-level reverse BFS.

---

## 4. Anchors and boundary calibration compatibility

Downstream, your pipeline calibrates boundary gates by selecting an **incoming anchor edge** for each boundary node.

### 4.1 Anchor candidates in code

For a node `u`, the analyzer produces:

- `mergeableAnchorsByHead[u] = { e | e is an incoming edge into u }` filtered by:
  - `e` is **old** (not delta-insert)
  - `e` is **non-deterministic** (`prob < 1.0`) so it has an edge variable
  - `e` is **non-subsumed** among the old incoming edges of `u` (subset test)

This is aligned with the paper’s need for a witness edge `e*` in (C3), and with the existing calibrator which solves for a new weight for the edge variable of `e*`.

### 4.2 Boundary nodes

Boundary nodes are still computed in the same shape your code expects:

- a node in the region that has at least one outgoing dependency to a node outside the region.

The `Boundaries` struct still contains 3 sets; in this implementation they are used only as labels:

- `out_induced`: boundary nodes that are heads of delta-insert edges
- `scope_induced`: boundary nodes that are delta-insert nodes
- `residual`: others

The pipeline only uses the union of these sets.

---

## 5. Why this removes least-parents overhead

- We do **not** compute least parents.
- We do **not** compute scopes.
- We do **not** iteratively “grow outward until merge-ready”.

Instead:

- The region is computed by a reusability fixed point, which is essentially:

  - `nonReusable := seeds ∪ propagate(nonReusable)`

  with propagation blocked by summary nodes.

The runtime is dominated by:

- delta closure size (`|∆fact|`)
- number of outgoing arcs from non-reusable nodes
- summary checks on targets encountered

This correlates naturally with delta magnitude.

---

## 6. Engineering notes and caveats

### 6.1 (C3) “variable occurs in Πbdd(t)” is approximated

The paper’s (C3) is a BDD-level condition:

- the chosen anchor variable must appear in the **old BDD of the reused target**.

The analyzer currently does not have access to the compiled BDDs or a direct “vars-in-BDD” query.

So we approximate (C3) as:

- the candidate summary predecessor `cand` has at least one old, non-det, non-subsumed incoming edge.

This is usually sufficient in your current compilation strategy because the BDD of a node typically contains variables for its incoming non-det edges, and downstream BDDs are built compositionally.

If you want to make this exact, the best upgrade is:

- pass a `FormulaManager/BDDManager` handle into the analyzer, and
- implement a `bool bddContainsVar(Πbdd(t), var(e*))` check.

Then (C3) can be enforced precisely.

### 6.2 Grouping sources by SCC is intentional

Reusability is defined per source atom `s`. If two sources are in the same SCC, they have identical reachability, so the summary predicate and reusability outcome are identical.

We therefore group by SCC to reduce redundant computation.

### 6.3 Inserted base facts

If a delta-insert node is a base fact with no incoming edges, it cannot supply an anchor variable. This is consistent with the current calibrator (which calibrates edge variables only), and causes the region to expand to include its descendants (since summary cannot be established).

If you later represent base facts via stable variables with reweighting, you can add a “fact-anchor” path.

---

## 7. Integration instructions

### Option A (minimal disruption)

1. Drop `IncRegionAnalyzer_PaperFormal.h` into the same include directory.
2. In `RegionalIncremental.h`, replace:

```cpp
#include "souffle/problog/IncRegionAnalyzer.h"
```

with:

```cpp
#include "souffle/problog/IncRegionAnalyzer_PaperFormal.h"
```

3. Ensure that the rest of the pipeline uses the same types (`incra::RegionAnalyzer`, etc.).

### Option B (true drop-in replacement)

Overwrite your existing `souffle/problog/IncRegionAnalyzer.h` with the contents of `IncRegionAnalyzer_PaperFormal.h`.

This avoids touching includes.

---

## 8. Suggested next improvements (optional)

1. **Exact (C3)** using BDD variable occurrence.
2. Replace `unordered_set<size_t>` SCC-closure sets with a packed bitset for speed and memory.
3. Cache `Pred(t)` as SCC IDs rather than NodePtrs if you only need SCC-level checks.
4. Extend the same framework to deletion (requires careful handling of cycle repair and negative facts).

