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
- Fixpoint: loop detection + rewrite until no region is rewritten. A split pass may run at fixpoint depending on `--split-mode` (default `naive-split`); if split rewires edges, rewrite continues until both rewrite and split reach fixpoint.
- Final global forward compilation runs on the rewritten view.
- Logging: pipeline logs timings; rewrite stats include iterations, region counts, nodes/edges removed/added.

## Latest evaluation (2026-01-04, full rule set)
Settings:
- `--det-opt` always on; `--rewrite` toggled.
- Compile with `--full-only`.
- Split mode: default `naive-split`.
- Backend: BDD (CUDD); bucketed init for small/medium graphs and defaults for large graphs.
- `rand_vars` counts probabilistic facts + probabilistic edges in the pruned view used for FC.

| case | variant | rand_vars | total_s | seminaive_s | create_s | prune_s | rewrite_s | fc_s | wmc_s | manager_init_ms | reorder_s | dd_live_nodes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| P1 | norewrite | 2 | 0.022 | 0.013 | 0.000 | 0.000 | 0.000 | 0.008 | 0.000 | 7 | 0.000 | 3 |
| P1 | rewrite | 2 | 0.021 | 0.015 | 0.000 | 0.000 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 3 |
| P3 | norewrite | 2 | 0.018 | 0.012 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 4 | 0.000 | 3 |
| P3 | rewrite | 2 | 0.020 | 0.013 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 4 | 0.000 | 3 |
| P4 | norewrite | 1 | 0.008 | 0.002 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 4 | 0.000 | 2 |
| P4 | rewrite | 1 | 0.006 | 0.001 | 0.000 | 0.000 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 2 |
| P5 | norewrite | 12 | 0.006 | 0.002 | 0.000 | 0.000 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 33 |
| P5 | rewrite | 0 | 0.006 | 0.002 | 0.000 | 0.000 | 0.000 | 0.003 | 0.000 | 3 | 0.000 | 1 |
| P6 | norewrite | 39 | 0.009 | 0.003 | 0.000 | 0.000 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 117 |
| P6 | rewrite | 6 | 0.013 | 0.007 | 0.000 | 0.000 | 0.001 | 0.004 | 0.000 | 3 | 0.000 | 18 |
| P7 | norewrite | 56 | 0.012 | 0.004 | 0.001 | 0.001 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 194 |
| P7 | rewrite | 18 | 0.012 | 0.004 | 0.001 | 0.001 | 0.002 | 0.004 | 0.000 | 3 | 0.000 | 59 |
| P8 | norewrite | 53 | 0.017 | 0.008 | 0.001 | 0.001 | 0.000 | 0.005 | 0.000 | 3 | 0.000 | 187 |
| P8 | rewrite | 18 | 0.019 | 0.008 | 0.001 | 0.001 | 0.002 | 0.005 | 0.000 | 4 | 0.000 | 60 |
| P9 | norewrite | 113 | 0.016 | 0.006 | 0.001 | 0.001 | 0.000 | 0.005 | 0.000 | 3 | 0.000 | 432 |
| P9 | rewrite | 31 | 0.017 | 0.006 | 0.001 | 0.001 | 0.003 | 0.005 | 0.000 | 4 | 0.000 | 90 |
| P10 | norewrite | 32 | 0.047 | 0.024 | 0.013 | 0.001 | 0.000 | 0.005 | 0.000 | 4 | 0.000 | 49 |
| P10 | rewrite | 0 | 0.045 | 0.023 | 0.012 | 0.002 | 0.001 | 0.004 | 0.000 | 3 | 0.000 | 1 |
| P11 | norewrite | 16 | 0.043 | 0.021 | 0.013 | 0.001 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 17 |
| P11 | rewrite | 16 | 0.043 | 0.021 | 0.012 | 0.001 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 17 |
| P12 | norewrite | 466 | 0.132 | 0.063 | 0.012 | 0.010 | 0.000 | 0.035 | 0.000 | 25 | 0.000 | 1693 |
| P12 | rewrite | 0 | 0.111 | 0.060 | 0.012 | 0.010 | 0.020 | 0.003 | 0.000 | 3 | 0.000 | 1 |
| P13 | norewrite | 1143 | 0.494 | 0.109 | 0.026 | 0.026 | 0.000 | 0.305 | 0.001 | 65 | 0.210 | 4107 |
| P13 | rewrite | 0 | 0.234 | 0.114 | 0.028 | 0.026 | 0.052 | 0.003 | 0.000 | 2 | 0.000 | 1 |
| P14 | norewrite | 1643 | 0.928 | 0.166 | 0.042 | 0.046 | 0.000 | 0.629 | 0.003 | 52 | 0.530 | 7090 |
| P14 | rewrite | 449 | 0.366 | 0.162 | 0.043 | 0.044 | 0.084 | 0.017 | 0.000 | 9 | 0.000 | 1885 |
| P15 | norewrite | 3262 | 3.226 | 0.369 | 0.096 | 0.137 | 0.000 | 2.527 | 0.005 | 72 | 2.220 | 28140 |
| P15 | rewrite | 1225 | 1.448 | 0.378 | 0.104 | 0.133 | 0.142 | 0.653 | 0.001 | 60 | 0.560 | 5425 |
| P16 | norewrite | 5635 | 5.411 | 0.603 | 0.178 | 0.281 | 0.000 | 4.164 | 0.011 | 269 | 3.570 | 73245 |
| P16 | rewrite | 2210 | 2.698 | 0.588 | 0.186 | 0.276 | 0.275 | 1.312 | 0.002 | 80 | 1.130 | 12527 |
| P17 | norewrite | 7635 | 6.621 | 0.812 | 0.231 | 0.505 | 0.000 | 4.812 | 0.025 | 305 | 4.050 | 136607 |
| P17 | rewrite | 3010 | 4.801 | 0.788 | 0.283 | 0.501 | 0.431 | 2.710 | 0.003 | 86 | 2.400 | 22181 |
| P18 | norewrite | 9641 | 7.039 | 1.050 | 0.337 | 0.852 | 0.000 | 4.430 | 0.049 | 322 | 3.000 | 236563 |
| P18 | rewrite | 3806 | 5.354 | 1.035 | 0.348 | 0.855 | 0.786 | 2.210 | 0.005 | 85 | 1.850 | 39675 |
| P19 | norewrite | 12143 | 10.403 | 1.326 | 0.451 | 1.346 | 0.000 | 6.759 | 0.060 | 306 | 5.210 | 380873 |
| P19 | rewrite | 4810 | 8.666 | 1.313 | 0.460 | 1.286 | 1.147 | 4.313 | 0.008 | 322 | 3.780 | 60507 |
| P20 | norewrite | 17852 | 6.799 | 1.046 | 0.417 | 0.654 | 0.000 | 4.161 | 0.027 | 301 | 1.580 | 75871 |
| P20 | rewrite | 3632 | 4.412 | 0.999 | 0.406 | 0.867 | 1.547 | 0.446 | 0.002 | 50 | 0.180 | 9237 |

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
