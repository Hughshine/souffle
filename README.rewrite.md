# Rewrite Pipeline Notes

This file summarizes the SISO rewrite pipeline: design, current state (post revert), observed behavior, and near‑term plans.

## Scope
- Full-mode pipeline only; incremental modes (`inc`/`inc-regional`) skip rewrite.
- Online compilation is default; `--online` is optional (kept in commands when shown).

## Goal
- Iteratively find SISO regions (single entry/exit) in the derivation graph, summarize each region into a single probabilistic edge, then run the usual forward compilation on the smaller view.

## Implementation (current state)
- Detection: `GraphAnalyzer::detectAllSISOStrictFromExit(view)` on the working `IncSubgraphView`; cached incoming edges are cleared each iteration.
- Non‑trivial filter: regions with `edgeCount >= 1` or `nodeCount > 2` and at most `kMaxEdges` (default 5) are considered; random variable count is **not** enforced after the revert.
- Local inference: build BDD formulas for the region, compute `Pr(exit | entry)` via weighted model counting.
- Rewrite action: add a new edge `entry -> exit` with that probability; remove internal edges and non‑boundary nodes from the **view** (underlying graph only gains the new edge). Simple SISO currently only updates edge probability; nodes are not folded.
- Fixpoint: loop detection + rewrite until no region is rewritten. Final global forward compilation runs on the rewritten view.
- Logging: pipeline logs timings; rewrite stats include iterations, region counts, nodes/edges removed/added.

## Recent measurements
- P5 (small): rewrite vs no‑rewrite both ~0.33–0.34s; BDD live nodes ~210; SISO detection ~2ms.
- P12 (post revert):
  - No rewrite: SISO detection 1.9s; BDD init 211ms; BDD build 823ms; per‑node WMC 155ms; total ~3.3s.
  - Rewrite: SISO detection 1.94s; SISO rewrite 3.33s (iterations=2, regions=1212, nodesRemoved=303, edgesRemoved=101, edgesAdded=101, avgRandomVars≈1.08); BDD init 53ms; BDD build 861ms; per‑node WMC 137ms; total ~6.5s.
  - Outputs match (`facts.prob` identical).

## Observed issues
- Rewrite can be slower on larger graphs (e.g., P12) because detection + per‑region forward comp dominate; global BDD build may not shrink enough to offset that cost.
- Simple SISO rewrite does not fold nodes; potential size reduction is limited.
- Region count can be large (1000+), and most regions have ≤1 random variable, so summarization benefit is small while cost accumulates.

## Near‑term ideas
1) Reduce useless work:
   - Heuristics to skip regions with tiny payoff (e.g., RV count ≤1 and already single edge), or cap regions per iteration.
   - Increase `kMaxEdges` only when payoff is expected; otherwise keep regions small to bound local DD cost.
2) Cheaper handling for “simple” patterns:
   - For linear or single‑edge SISO (including fact inputs), compute probability analytically and fold nodes when safe (unique in/out edges), to actually shrink the view.
3) Better accounting:
   - Per‑region profiling (detect/build/WMC/apply) to pinpoint hotspots; report BDD live nodes per region to catch blow‑ups early.
4) Safeguards:
   - Optional limit on total rewrite time or total regions processed; early exit to avoid regressions.

Use this as a reference before making further optimizations. Keep changes small and measure on both small (P5) and larger (P12/P1x) cases, comparing total time and BDD sizes with and without rewrite.
