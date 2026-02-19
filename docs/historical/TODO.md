# TODO

- Investigate `P12` compare-all timeout for `mix-d25-i75` sample-4 (inc-naive + inc-regional exit=124).
  - Delta: `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/P12/delta/mix-d25-i75_4.txt`
  - Output: `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/P12/output/delta-mix-d25-i75/sample-4/`
  - Meta JSON: `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/P12/output/delta-mix-d25-i75/sample-4/delta-mix-d25-i75-4.json`

## Rewrite/View Data-Structure Mismatch (follow-up)

Problem statement:
- Current rewrite mutates `IncSubgraphView` membership (`view.mutableEdges()/mutableNodes()`) heavily.
- However, node-level adjacency vectors in `DerivationGraph` (`incomingEdges/outgoingEdges`) are append-heavy and not compacted for those view-only removals.
- Fast-path SISO detection queries adjacency through `getIncomingEdges/getOutgoingEdges`, which filters by current view membership but still scans the full historical vectors first.

Observed symptom:
- Later rewrite iterations can have fewer live nodes/edges yet significantly higher detect time.
- The dominant cost becomes repeated full-graph scans and per-node adjacency filtering over historical edge lists.

Why this matters:
- The view API is read-oriented and semantic; rewrite uses it as a mutable working graph.
- This is functionally correct, but the data layout does not match the mutation pattern of rewrite/split/compaction.

Planned work:
1. Introduce dirty-frontier SISO detection (first global pass, then local passes around changed nodes/edges, with conservative fallback to global).
2. Revisit graph/view mutation boundaries:
   - Decide whether rewrite should use a dedicated mutable graph view with compact adjacency.
   - Or add adjacency compaction / tombstone cleanup hooks after heavy rewrite waves.
3. Add profiling counters for adjacency-scan effort (not only live edge/node counts), so regressions are measurable.

## Rewrite follow-ups (new)

1. Explore split beyond `fact` nodes.
   - Current split passes (`naive`/`complete`) only consider nodes with `is_fact=true`.
   - There are derived-node fan-out cases that still look structurally splittable (e.g., disjoint downstream cones), so we should evaluate a safe derived-node split variant.
   - Need explicit semantic constraints before enabling by default (shared evidence/output nodes, probability ownership, and interaction with later compaction).

2. Add a deterministic bypass rewrite for single-input chains.
   - Candidate rule: for a deterministic single-input edge `u -> v` (`p=1`, non-negated), if `v` has no other incoming edge, bypass `v` by reconnecting `v`'s outgoing edges to `u`, then remove the original edge and isolated `v`.
   - Expected benefit: reduce intermediate deterministic nodes/edges before split and SISO detection.
   - Must preserve semantics for negation/body ordering/probability fields and avoid rewriting when `v` carries output/evidence constraints.

3. Improve `naive` split with connected-group partitioning.
   - Current `naive` split only rewires branches that are fully independent from all other branches.
   - In practice, branches often form multiple overlap-components; splitting by connected overlap groups (similar to `complete` grouping, but lighter) could still separate unrelated branch sets.
   - Follow-up: design a low-overhead grouping pass for `naive` mode that avoids full `complete` split cost while capturing obvious partition opportunities.

4. Revisit `naive` split reachability cap (`max_reachable=50`).
   - Current guard aborts split for a fact if any branch BFS visits more than 50 nodes.
   - In `taint/angulo`, this skipped clearly disjoint branches for `MV(2901,375)` / `Alloc(375,66)` (one branch reachability was 74), so split opportunity was lost.
   - Follow-up: make the cap configurable (CLI/env) and evaluate modestly larger defaults (e.g., 100/200) with profiling.
   - If cost grows too much, use a hybrid budget: per-fact node/edge visit budget + early stop only for the offending branch, instead of dropping the whole fact.

Another tricky part is DD dynamic reordering: BDD libraries use periodic
reordering to reduce formula blow-up, but reordering itself is expensive.
Both `inc-naive` and `inc-regional` reuse old BDDs and variable ordering.
`inc-naive` may still trigger reordering more often because it performs more
formula operations. We need a clear policy for when incremental turns should
disable or re-enable dynamic reordering.

## Source references
- [archive/README.md](archive/README.md)
- [archive/2026-02-11/README.md](archive/2026-02-11/README.md)

## Related commits
- `UNCOMMITTED` — docs(historical): archive pending follow-up notes under docs/historical
