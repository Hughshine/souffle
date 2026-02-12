# Derivation Graph Split (Design and Current Behavior)

## Source references
- [src/include/souffle/problog/GraphRewriter.h](src/include/souffle/problog/GraphRewriter.h)
- [src/include/souffle/problog/GraphAnalyzer.h](src/include/souffle/problog/GraphAnalyzer.h)


This document describes the split transform used to expose more SISO structure
before rewrite and forward compilation. It combines current implementation
status, design notes, and historical evaluation observations.

## Status
- Active design note. Current behavior matches the implementation as of 2026-01-04.
- Historical results are labeled and should be re-run after semantic changes.

## Scope
- Split discovery and transform (no incremental maintenance yet).
- Applies to full-mode rewrite only; incremental modes skip rewrite.
- Split is an optional pre-pass inside the rewrite loop.

## Current behavior (2026-01-04)
- CLI: `--split-mode={no-split|naive-split|complete-split}` (short `-P`),
  default `naive-split`.
- Split currently only duplicates input fact nodes; incoming edges of internal
  nodes are not cloned yet. Facts with evidence or `needOutput` are skipped, and
  facts in evidence-affected components are excluded.
- `complete-split` uses a multi-source union-find on downstream reachability;
  any downstream join merges branches (conservative, semantics-safe).
- `hasRVReach` is recomputed globally each split pass (no incremental update yet).
- `naive-split` uses bounded reachability (max 50 nodes per branch) and only
  splits branches whose reachable node sets are pairwise disjoint.
- Budget controls are active:
  `splitMaxNewNodesPerPass`, `splitMaxNewEdgesPerPass`,
  `splitMaxGroupsPerNode`, `splitMinGroupEdges`.
- Scheduling: rewrite runs to fixpoint, then split runs once; if split changes
  the graph, rewrite repeats. Split is no longer interleaved with every rewrite
  iteration.
- Logs: `[GraphRewriter] split(mode): nodes=.. edges=.. time=.. ms`.
- Debug DOT: `siso_regions_iter*.dot` in output directory when `--dumpdot`.

## Executable workflow
- Use the full evaluation scripts in `docs/topics/evaluation/README.eval.md` with `--rewrite` and
  `--split-mode=...`.
- Keep no-rewrite and rewrite outputs separate to compare `facts.prob`.

## Historical observations (pre-2026-01-04)
- Output consistency: `facts.prob` matched across split modes for P4-P19.
- `naive-split` triggered rarely on large cases and behaved close to `no-split`.
- `complete-split` triggered more frequently and reduced RV ratios on P17-P19,
  but increased rewrite iterations and region counts.
- The semantics changed on 2026-01-04 to "any join merges branches"; the older
  timing tables should be treated as historical and re-run under the new rule.

## Design goal
Split tries to separate downstream reasoning that is independent at the random
variable level, so rewrite can detect more SISO regions and forward compilation
can exploit smaller components.

## Definitions
- Derivation graph: directed hypergraph with edges `inputs -> output`.
- RV (random variable) may be attached to an edge (or referenced by an edge).
- Downstream RV support:
  - `DownRV(n)`: RVs reachable from node `n` along derivation direction.
  - `DownRV(e: u->v)`: `rv(e) U DownRV(v)` (optionally omit `rv(e)` if desired).
- Split criterion (conceptual): outgoing edges of a node can be partitioned
  into groups whose downstream RV supports are disjoint.

## Core algorithm: partition outgoing edges by downstream joins
Instead of O(k^2) pairwise RV-set intersections, use a multi-source traversal
that detects downstream joins and unions branches that meet.

### Intuition
If two outgoing branches ever meet downstream, they are correlated and must
stay in the same group. If they never meet, they can be separated.

### Pseudocode (union-find grouping)
```text
partitionOutgoing(u):
  out = outEdges(u)
  k = size(out)
  if k <= 1: return [out]
  if hasRVReach[u] == false: return [out]

  // owner[x]: NONE, SINGLE(i), or MULTI (with rep)
  // Use a token array to avoid clearing per call.

  init queue with each dst(out[i]) labeled owner=SINGLE(i)

  while queue not empty:
    x = pop
    for each edge x -> y:
      if owner[y] == NONE:
        owner[y] = owner[x]
        rep[y] = rep[x]
        push y
      else if owner[y] != owner[x]:
        // downstream join detected
        union(rep[y], rep[x])
        owner[y] = MULTI

  // group by union-find representative
  groups = map rep -> list of edges
  for i in 0..k-1:
    groups[find(i)].append(out[i])
  return groups
```

### Properties
- Any downstream join merges branches (conservative correctness).
- Complexity is roughly O(|reachable cone|), independent of k^2.

## Candidate filtering: hasRVReach
`hasRVReach[n]` is true if any RV is reachable downstream of `n`.
- Use it to skip split checks on nodes that cannot reach RVs.
- Current implementation recomputes it globally each split pass.
- Future improvement: maintain `rvReachCount[n]` incrementally as edges change.

## Split decision (cost vs benefit)
Split can blow up graph size if applied indiscriminately. Use constraints:
- Minimum groups: require `groups.size >= 2`.
- Budget limits per pass: max new nodes/edges and max groups per node.
- Optional cost model:
  - `cost ~= (groups.size - 1) * indeg(u)`
  - `benefit ~= total downstream RV count across groups`
  - Apply if `benefit >= threshold` and budget allows.

## Split transform
Two possible modes for cloning incoming edges of non-fact nodes:
- `ALIAS_RV` (recommended): clone edges but reuse RV identifiers.
- `FRESH_RV` (risky): clone edges and assign new RV identifiers.

Current implementation only splits input fact nodes, so incoming-edge cloning
is not yet needed. When enabled, ensure adjacency and degree caches stay
consistent, and update `hasRVReach`/`rvReachCount` accordingly.

## Scheduling in rewrite
Event-driven candidate queue is preferable to scanning all nodes:
- Enqueue a node when outgoing edges change or `hasRVReach` flips.
- In linear graphs, few nodes qualify, so split checks stay cheap.

## Correctness checks
- Graph invariants: `inEdges`/`outEdges` consistency, degree caches correct.
- Probability preservation: compare `facts.prob` on small graphs before/after
  split under both ALIAS_RV and FRESH_RV (if enabled).
- Structural sanity: SISO counts should not decrease after split.

## Default parameters (suggested)
- `MAX_CLONES_PER_NODE = 2`
- `MAX_NEW_IN_EDGES_PER_PASS = 50000`
- `worthSplitting`: `benefit >= 1 AND cost <= 1000`

## Related docs
- `docs/topics/rewrite/README.rewrite.impl.md` for rewrite pipeline and evaluation tables.
- `docs/topics/rewrite/README.siso.md` for SISO detection details.
- `docs/topics/evaluation/README.eval.md` for executable benchmark workflows.

## Related commits
- `812ea4081` — docs(repo): refine README narratives
- `2fe1123db` — perf(problog): limit prob results to outputs
- `e1e9f6c84` — perf(problog): add hybrid stage metrics for component FC
