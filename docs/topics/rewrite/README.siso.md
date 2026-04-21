# SISO Region Detection (GraphAnalyzer)

## Source references
- [src/include/souffle/problog/GraphAnalyzer.h](src/include/souffle/problog/GraphAnalyzer.h)


> The current detector uses fast-path patterns only; the dominance-based detector is retained but disabled. For rewrite behavior and performance notes, see `docs/topics/rewrite/README.rewrite.impl.md`.

This note explains what `src/include/souffle/problog/GraphAnalyzer.h` does, the major steps in its algorithm, and where the debug logs land. It is meant to help reason about why SISO regions may or may not be found on a given derivation graph.

## Status
- Fast-path detector reference; dominance-based detector is currently disabled.

## Scope
- Detection algorithm only (GraphAnalyzer); rewrite application lives in `docs/topics/rewrite/README.rewrite.impl.md`.
- Not specific to online incremental; applies to full-mode derivation graphs.

## Current behavior (fast-path only)
`detectAllSISOStrictFromExit(view)` runs only the fast-path detectors and then
deduplicates overlapping regions (smallest regions first). The active patterns are:
- **Single hyperedge**: one non-fact SI plus ≥1 fact inputs; fact inputs must be
  pure (no incoming edges, exactly one outgoing edge, no evidence/output), and
  SI must be non-fact and distinct from the exit.
- **Linear two-edge chain**: `entry -> mid -> exit` with single-input edges,
  `mid` has exactly one outgoing edge, and the `mid -> exit` edge is not negated.
- **Parallel edges**: multiple single-input edges from the same SI to the same SO,
  grouped by negation flag; regions require at least two edges in a group.
- **Fan-out converge**: a fact SI fans out to distinct `xi` nodes (each with single
  in/out edge), which converge at one multi-input edge with all-positive inputs.
- **All-facts single hyperedge**: a single edge whose inputs are all pure facts
  (no evidence, no incoming edges, only this outgoing edge); the exit has only
  that incoming edge.

Fast-path debug output is not part of the artifact runtime surface.  Use the
normal JSON stage log and rewrite metadata for artifact analysis.

## High-level goal
Given a derivation hypergraph (nodes = facts/derivations, hyperedges = rule applications), detect **SISO** (single-entry, single-exit) regions. A SISO region is defined here as a subgraph with:
- One entry node that is reachable from outside the region (at least one predecessor outside).
- One exit edge and its output (“exit succ”).
- No “escape” edges leaving the region other than the exit edge.
- Internal coverage determined by dominance-style reasoning over “dominant inputs” of hyperedges.

## Legacy pipeline (currently disabled)
The dominance-based pipeline below is retained in `GraphAnalyzer` but is not
invoked by `detectAllSISOStrictFromExit` (the entry-point returns fast-path
regions only). It is kept here for reference if the general detector is re-enabled.

For each candidate exit edge in the graph:
1) **Prefix structure build**
   - `backwardCollectFacts`: For every node, collect all facts reachable by reverse traversal to inputs; cached in `support`.
   - `computeEdgeDomInputs`: For each hyperedge, group its inputs by overlapping support sets and pick the “dominant” group (largest union). This is logged to `siso_edge_dom_inputs.log`.
   - `buildDomGraph`: Collapse each hyperedge to edges from dominant inputs to the output node; find a root (first node with no preds).
   - `computeDominators`: Classic iterative dominator sets and immediate dominators over the collapsed graph.
2) **Candidate selection for a given exit edge**
   - Identify `exitSucc` = output of the exit edge.
   - Find SO/exit node: among exit-edge inputs, pick the unique node whose outgoing edges either are the exit edge or self/output-equal edges. If not unique → reject.
   - Map exit node and its incoming edges’ dominant inputs into dominator graph indices; compute LCA of these sources as the entry node. If LCA is invalid or equals exit → reject.
3) **Region construction**
   - `buildStrictRegion`: Reverse-walk from exit to entry following only dominant inputs; fail if entry not reached.
   - `buildFullRegion`: Reverse-walk from exit to entry following all inputs; fail if entry not reached.
4) **Entry / escape checks**
   - `selectEntryEdgeAndPreds`: Find predecessors of entry that lie **outside** the full region. Must find at least one or reject.
   - `checkNoEscape`: Ensure every outgoing edge of internal nodes stays inside the region, except the designated exit edge; otherwise reject.
5) **Assembly**
   - Package `SISORegionInfo` (entry/exit, internal nodes/edges, entry preds, etc.). Regions are filtered for overlap and sorted by size in `detectAllSISOStrictFromExit`.

## Debug logging
- `siso_debug.log`, `siso_edge_dom_inputs.log`, `siso_info.csv`: used by the
  legacy dominance-based detector (currently disabled).

## Common rejection reasons observed (legacy detector)
- **Multiple SO candidates**: exit edge inputs are not uniquely “single outgoing” to the exit; fails early.
- **Invalid entryIdx**: LCA over dominant inputs is missing or equals exit.
- **No outside preds**: entry node has no predecessor outside the candidate region.
- **Escape detected**: some internal node has an outgoing edge leaving the region (other than the exit edge).

## How to read the logs (legacy detector)
1) Start with `siso_debug.log` for a run; look for the first `findCandidate` for an exit edge. If it is rejected, the reason immediately follows.
2) If `buildStrictRegion`/`buildFullRegion` log `missed entry`, dominance didn’t connect entry→exit.
3) If a candidate passes construction but fails `no outside preds` or `escape detected`, adjust the entry predicate requirement or escape criteria as needed.
4) Use `siso_edge_dom_inputs.log` to see why a specific edge chose certain dominant inputs (support-set overlap heuristic).

## Notes / limitations
- Fast-path detection ignores dominance-based regions.
- Dominance is computed on a collapsed graph using only dominant inputs; if that
  choice is too restrictive, entry LCAs can fail.
- Entry requires at least one external predecessor; pure-fact prefixes with no
  outside preds are currently rejected.
- Escape check is strict: any outgoing edge not in the region (except the single
  exit edge) rejects the region.

These constraints together can make detection conservative; relaxations would need code changes in the corresponding checks above.

## Related commits
- `812ea4081` — docs(repo): refine README narratives
- `4c4bd26b2` — docs(readme): restructure online incremental docs
- `d8d10654d` — Add pure two-node SISO tagging and report random var stats in rewrite
