# Rewrite Optimization Notes

This file summarizes analysis of GraphRewriter / GraphAnalyzer rewrite costs,
adjacency queries, cache invalidation, and optimization ideas. It is a design
note only; no code changes are included.

## Current behavior (rewrite loop)

- The rewrite loop detects SISO regions, then rewrites them one by one.
- Each region rewrite mutates the working view immediately:
  - new edges/nodes are inserted
  - old edges/nodes are removed
  - view.invalidateCaches() is called in multiple places
- Per region cost is local (region size), but per iteration cost includes
  full-graph passes (SISO detection, compaction, cleanup, precompute, counting).

Key reference: src/include/souffle/problog/GraphRewriter.h

## getIncomingEdges / getOutgoingEdges

Implementation in DerivationGraphViewInterface:
- Uses node adjacency lists and filters by whether the edge is in the view.
- It does not scan the entire graph; it scans the node degree list.
- However, view-level edge removals do not remove edges from node adjacency
  lists, so the "degree scan" can still be as large as the base graph.

Key reference: src/include/souffle/problog/DerivationGraph.h

## Cache usage and invalidation

Cached entrypoints:
- getIncomingEdgesStable() caches sorted incoming edges by node id
- getCycleDependencyGraph() caches SCC/dep graph
- getValidNodes()/getValidEdges() caches in IncrementalDerivationGraphViewInterface

invalidateCaches() clears those caches. Merging invalidations is only safe if
the rewrite batch never calls any cached entrypoint while the graph is mutated.
Otherwise, stale cache results can be read.

## Optimization ideas: priority and ease

P0 / easy:
- Only run compaction/cleanup/precompute when this iteration actually rewrote
  something (or when split changes the graph).
- Avoid full-graph work when nothing changed.

P0 / medium:
- Batch region rewrites, then invalidate caches once at the end of the batch.
- Requires "no cached entrypoint usage during batch" rule.

P1 / medium:
- Add a cached getOutgoingEdgesStable() (symmetric to incoming).
- Must ensure all structural mutations trigger cache invalidation.

P1 / hard:
- Incremental SISO detection using a dirty set (recompute only affected parts).
- Correctness is non-trivial because rewrites can create new SISO regions.

P2 / hard:
- Maintain view-level adjacency lists and update them incrementally on rewrite.
- This reduces degree scans after large view deletions, but correctness is hard
  because all mutation paths must be covered.

## Correctness notes by optimization

- Conditional compaction/cleanup/precompute:
  - Correctness is relatively easy if these are purely optimizing passes.
  - Need to verify no later stage relies on precompute side effects.

- Batched rewrite + single invalidate:
  - Correctness is medium; safe only if no cached entrypoint is called during
    the batch (getIncomingEdgesStable, getCycleDependencyGraph, getValidNodes).
  - If any cached entrypoint is used mid-batch, stale data can be read.

- Cached outgoing edges:
  - Correctness is medium; must invalidate on every mutation path (rewrite,
    compaction, split, deletion).

- Dirty-set SISO detection:
  - Correctness is hard. Rewrites can create new SISO regions outside the local
    region, so a naive dirty set can miss new candidates.
  - Conservative expansion or fallback to full detection is required.

- View-level adjacency maintenance:
  - Correctness is hard. All paths that add/remove edges or nodes must update
    the view adjacency; missing a path yields incorrect queries.

## Conclusion

The main performance risk today is not a bug in getIncomingEdges/getOutgoingEdges
logic, but that repeated view mutations combined with full-graph passes and
degree scans on base-graph adjacency can dominate runtime. The lowest-risk
optimizations are conditional full-graph passes and cautious batching with
strict avoidance of cached entrypoints during mutation.
