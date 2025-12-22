# Probabilistic eqrel-style pruning

## Status
- Design note only; not implemented in code.
- Intended for pruning/graph simplification before forward compilation.

## Motivation

- The two pruning routines in `src/include/souffle/problog/DerivationGraph.h:916` and `:1594` already shrink the derivation graph to only the nodes needed by the requested outputs.  Their final result, however, still contains internal “mirror” nodes that are linked to each other through probability-1 hyperedges, e.g. deterministic rewrites or aliasing rules (`A(x) :- B(x).`, `B(x) :- A(x).`).  When the graph is later sent to `ForwardCompilation` and converted into a decision diagram, these pairs end up producing identical formulas.
- Treating those nodes as a Soufflé-style `eqrel` lets us collapse them eagerly: if node `u` can reach node `v` through a deterministic single-input edge and `v` can reach `u` in the same way, then `u` and `v` must evaluate to the same Boolean formula, so they are redundant.  Collapsing them means (1) fewer intermediate DD variables, (2) less fixpoint work during incremental updates, and (3) more opportunities for further pruning (duplicate hyperedges, dead negations, etc.).

## Detection strategy

1. **Work on the pruned subgraph.**  Right after each prune routine produces `{reachableNodes, reachableEdges}`, run an extra pass that only sees those survivals.
2. **Build a “deterministic dependency” graph.**  For every hyperedge `e` in the subgraph, if
   - `e->isDeterministic()` (probability is exactly `1.0`);
   - all entries in `e->getBodyNegations()` are `false`; and
   - `e->getInputs().size() == 1`;
   then we add a directed edge `inputs[0] -> e->getOutput()`.
3. **Find SCCs.**  Use Tarjan/Kosaraju on that directed graph.  Each SCC with more than one node (or a single node whose deterministic edges produce a self-loop) forms an “eqrel class”.
4. **Merge each class.**  Pick the smallest `Node::getId()` as canonical `rep`.
   - For every other member `n`:
     - Rewire all incoming hyperedges so their output becomes `rep`.
     - Rewire outgoing hyperedges by replacing `n` in their `inputs` vector with `rep`.
     - Update `rep->incomingEdges` / `rep->outgoingEdges` to include the rewired edges; clear `n`’s vectors to keep accidental reuse away.
     - Remove `n` from `DerivationGraph::nodes` and from `tupleToNodeMap`.
   - Because several hyperedges may now have identical `(rule, inputs, output)` triples, re-run the existing deduplication that `createHyperedge` already performs via `edgeKeyToEdgeMap`.

The extra phase is monotone (each merge reduces the vertex count), so we can iterate until no SCC contains more than one node.  In practice a single SCC decomposition already consolidates the full equivalence class, so we just apply the merge once per prune call.

## Required plumbing

1. **Hyperedge helpers.**  We need a writable API to redirect endpoints without freeing the edge:
   ```c++
   class Hyperedge {
   public:
       void replaceOutput(const NodePtr& newOutput) {
           output = newOutput;
           cachedSortedInputs.reset();
           cachedSortedBodyNegations.reset();
           cachedEdgeKey.reset();
       }
       void replaceInput(const NodePtr& oldNode, const NodePtr& newNode) {
           for (auto& input : inputs) {
               if (input == oldNode) {
                   input = newNode;
               }
           }
           cachedSortedInputs.reset();
           cachedSortedBodyNegations.reset();
           cachedEdgeKey.reset();
       }
   };
   ```
   The helper only mutates the edge; the caller (prune) will update the `Node::incoming/outgoingEdges` vectors to keep them consistent.
2. **Union-find helper inside `DerivationGraph`.**  Add a private method:
   ```c++
   void DerivationGraph::mergeDeterministicEquivalences(
       std::unordered_set<NodePtr>& liveNodes,
       std::unordered_set<EdgePtr>& liveEdges);
   ```
   It performs the SCC detection on `liveEdges`, rewires edges through the helpers above, and erases dead nodes from `liveNodes`, `nodes`, and `tupleToNodeMap`.
3. **Invoke the helper.**  Right before returning from both `prune` overloads (classical and incremental), insert:
   ```c++
   mergeDeterministicEquivalences(newNodes, newEdges);
   ```
   so the returned subgraph view, and the underlying `DerivationGraph`, are already deduplicated.

## Patch sketch

```diff
diff --git a/src/include/souffle/problog/DerivationGraph.h b/src/include/souffle/problog/DerivationGraph.h
@@ class Hyperedge {
 public:
     const std::vector<NodePtr>& getInputs() const { return inputs; }
-    NodePtr getOutput() const { return output; }
+    NodePtr getOutput() const { return output; }
+    void replaceOutput(const NodePtr& newOutput) {
+        output = newOutput;
+        cachedSortedInputs.reset();
+        cachedSortedBodyNegations.reset();
+        cachedEdgeKey.reset();
+    }
+    void replaceInput(const NodePtr& oldNode, const NodePtr& newNode) {
+        for (auto& input : inputs) {
+            if (input == oldNode) {
+                input = newNode;
+            }
+        }
+        cachedSortedInputs.reset();
+        cachedSortedBodyNegations.reset();
+        cachedEdgeKey.reset();
+    }
@@ class DerivationGraph {
 protected:
     std::unordered_set<NodePtr> nodes;
     std::unordered_set<EdgePtr> edges;
+    void mergeDeterministicEquivalences(std::unordered_set<NodePtr>&, std::unordered_set<EdgePtr>&);
@@
 SubgraphView DerivationGraph::prune(...) {
     ...
-    return SubgraphView(std::move(newNodes), std::move(newEdges));
+    mergeDeterministicEquivalences(newNodes, newEdges);
+    return SubgraphView(std::move(newNodes), std::move(newEdges));
 }
@@
 IncSubgraphView IncrementalDerivationGraph::prune(...) {
     ...
-    return IncSubgraphView(std::move(newNodes), std::move(newEdges),
-            std::move(newDeltaDeletedNodes), std::move(newDeltaDeletedEdges));
+    mergeDeterministicEquivalences(newNodes, newEdges);
+    return IncSubgraphView(...);
 }
```

The implementation of `mergeDeterministicEquivalences` will:

1. Collect deterministic single-input edges, create adjacency lists, and compute SCCs.
2. For every SCC with more than one node:
   - Choose a representative.
   - For every non-representative node, walk its `incomingEdges` and `outgoingEdges` to rewire through `replaceOutput` / `replaceInput`.
   - Drop the node from `nodes`, `tupleToNodeMap`, and `liveNodes`, and mark it as `pruned`.
3. After rewiring, revisit every edge in `liveEdges` and remove duplicates by comparing `edge->getEdgeKey()`.  Any duplicates are deleted from the graph, mimicking what `createHyperedge` already guarantees.

## Expected impact / risks

- The optimization is worthwhile whenever the Datalog encoding produces deterministic equivalence cycles (alias relations, views, transitive “copy” rules).  That pattern appears often in probabilistic encodings after unfoldings or rule expansions, so we expect smaller DDs and faster forward compilation.
- The helper only touches nodes within the pruned subgraph, so it is safe for both full and incremental pipelines.  We must, however, be careful with shared ownership (`shared_ptr`): all mutations happen before we hand out the `SubgraphView`, and we explicitly clear cached vectors to avoid iterator invalidation.
- The deterministic check ignores negated inputs and multi-input edges, so we will not accidentally merge nodes that rely on conjunction semantics.  The restriction can be relaxed later (e.g. detect deterministic bijective mappings with two inputs), but the simple rule keeps correctness obvious now.

With this scaffold in place, we can experiment with aggressive eqrel-style collapsing without touching the forward compilation stage itself.

## Queries and evidence inside an eqrel

- **Representative keeps the flags.**  When an SCC contains a query node or a node with evidence, the representative must inherit those flags (`Node::isQuery`, `needOutput`, `hasEvidence`, `evidenceValue`).  If multiple nodes in the class disagree (e.g. conflicting evidence assignments), we should bail out of the merge or trigger a consistency error, because the probability mass becomes undefined.
- **Evidence-driven reachability.**  The existing prune logic already forces every evidence node to stay in the subgraph even if it is not on a query path.  After merging, the representative should remain marked as “reachable” so that downstream components still regard the evidence as bound.  The SCC pass therefore needs to seed `reachableNodes` with any representative that now has `hasEvidence==true`.
- **Query outputs.**  Similar care is needed for query heads: when an eqrel class contains a query fact, the merged node must stay in `outputNodes` (used for reference counting) and keep `needOutput=true`, otherwise the final `SubgraphView` would omit the queried tuple.  If multiple queries fall into one class, we can record the union of their metadata labels.
- **Incremental bookkeeping.**  Incremental pruning relies on `deltaInsertNodes`/`deltaDeleteNodes`.  When a node with evidence/query metadata is subsumed, we should mark the representative as reinserted (so that change propagates) and the obsolete node as deleted.  This keeps the incremental fixpoint consistent even though the actual formula remains unchanged.

These extra guards ensure that merging deterministic equivalence classes does not accidentally drop or duplicate user-facing evidence/query facts.

## Mapping original nodes to merged reps

- **Why:** Once nodes are merged, “one node = one formula” no longer holds; callers asking for the formula/probability of an original fact must transparently land on its representative.
- **State to keep:** Maintain a `std::unordered_map<size_t, NodePtr> nodeIdToRep` (or Union-Find parent map keyed by `Node::getId()`).  During the SCC merge, initialize each member’s entry to the chosen representative and keep the representative’s entry pointing to itself.
- **Query path:** Whenever a formula/cache lookup happens (e.g. in forward compilation or probability query), resolve `n` via `findRep(n->getId())` before indexing formula maps.  This keeps existing maps keyed by node ID usable, as long as `findRep` is applied consistently on reads.
- **Incremental updates:** If new nodes are added later, seed their parent to themselves; if an SCC merge occurs, union the sets and update `nodeIdToRep` accordingly.  Delta bookkeeping will still work if every API that consumes a node first canonicalizes it.
- **Serialization / debug output:** When dumping DOT/JSON, it’s safer to render only representatives (or annotate each node with its rep id) so downstream tools aren’t confused by hidden aliases.
