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
- After rewrite, forward compilation is done component‑wise with a single BDD manager
  reused across components (largest‑randvar component initializes the manager; each
  component resets it).
- A single‑randvar fast path evaluates eligible components without DD managers.
- Component‑wise FC + WMC are timed together under a new `FC_WMC_HYBRID` stage.
- Logging: pipeline logs timings; rewrite stats include iterations, region counts, nodes/edges removed/added.

## Latest evaluation (2026-01-05, full rule set)
Settings:
- `--det-opt` always on; `--rewrite` toggled.
- Compile with `--full-only`.
- Split mode: default `naive-split`.
- Backend: BDD (CUDD); bucketed init for small/medium graphs and defaults for large graphs.
- `rand_vars` counts probabilistic facts + probabilistic edges in the pruned view used for FC.
- `hybrid_s` is `FC_WMC_HYBRID` wall time; `fc_s`/`wmc_s` come from `fc_build_ms`
  and `wmc_ms` recorded in the hybrid stage.
- `dd_live_nodes` for rewrite is the sum of per‑component live nodes; treat it as
  approximate (not directly comparable to the no‑rewrite single‑manager count).

| case | variant | rand_vars | total_s | seminaive_s | create_s | prune_s | rewrite_s | hybrid_s | fc_s | wmc_s | manager_init_ms | reorder_s | dd_live_nodes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| P1 | norewrite | 2 | 0.025 | 0.015 | 0.000 | 0.000 |  |  | 0.009 | 0.000 | 8 | 0.000 | 3 |
| P1 | rewrite | 2 | 0.015 | 0.013 | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 0 |  | 0 |
| P3 | norewrite | 2 | 0.018 | 0.013 | 0.000 | 0.000 |  |  | 0.004 | 0.000 | 3 | 0.000 | 3 |
| P3 | rewrite | 2 | 0.015 | 0.013 | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 0 |  | 0 |
| P4 | norewrite | 1 | 0.008 | 0.002 | 0.000 | 0.000 |  |  | 0.005 | 0.000 | 4 | 0.000 | 2 |
| P4 | rewrite | 0 | 0.004 | 0.002 | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 0 |  | 0 |
| P5 | norewrite | 12 | 0.008 | 0.003 | 0.000 | 0.000 |  |  | 0.004 | 0.000 | 3 | 0.000 | 33 |
| P5 | rewrite | 0 | 0.005 | 0.003 | 0.000 | 0.000 | 0.001 | 0.000 | 0.000 | 0.000 | 0 |  | 0 |
| P6 | norewrite | 39 | 0.011 | 0.004 | 0.000 | 0.001 |  |  | 0.005 | 0.000 | 3 | 0.000 | 117 |
| P6 | rewrite | 6 | 0.012 | 0.004 | 0.000 | 0.001 | 0.002 | 0.000 | 0.000 | 0.000 | 4 | 0.000 | 16 |
| P7 | norewrite | 56 | 0.013 | 0.005 | 0.001 | 0.001 |  |  | 0.004 | 0.000 | 3 | 0.000 | 194 |
| P7 | rewrite | 18 | 0.016 | 0.007 | 0.001 | 0.001 | 0.002 | 0.000 | 0.000 | 0.000 | 3 | 0.000 | 55 |
| P8 | norewrite | 53 | 0.018 | 0.008 | 0.001 | 0.001 |  |  | 0.005 | 0.000 | 3 | 0.000 | 187 |
| P8 | rewrite | 18 | 0.019 | 0.008 | 0.001 | 0.001 | 0.003 | 0.001 | 0.000 | 0.000 | 3 | 0.000 | 58 |
| P9 | norewrite | 113 | 0.016 | 0.006 | 0.001 | 0.001 |  |  | 0.005 | 0.000 | 3 | 0.000 | 432 |
| P9 | rewrite | 31 | 0.019 | 0.006 | 0.001 | 0.002 | 0.004 | 0.001 | 0.000 | 0.000 | 3 | 0.000 | 92 |
| P10 | norewrite | 32 | 0.047 | 0.024 | 0.013 | 0.001 |  |  | 0.005 | 0.000 | 3 | 0.000 | 49 |
| P10 | rewrite | 0 | 0.043 | 0.023 | 0.013 | 0.001 | 0.001 | 0.000 | 0.000 | 0.000 | 0 |  | 0 |
| P11 | norewrite | 16 | 0.054 | 0.029 | 0.015 | 0.001 |  |  | 0.004 | 0.000 | 3 | 0.000 | 17 |
| P11 | rewrite | 0 | 0.041 | 0.022 | 0.013 | 0.001 | 0.000 | 0.000 | 0.000 | 0.000 | 0 |  | 0 |
| P12 | norewrite | 466 | 0.141 | 0.062 | 0.014 | 0.012 |  |  | 0.039 | 0.000 | 27 | 0.000 | 1693 |
| P12 | rewrite | 0 | 0.119 | 0.063 | 0.014 | 0.012 | 0.023 | 0.000 | 0.000 | 0.000 | 0 |  | 0 |
| P13 | norewrite | 1143 | 0.544 | 0.113 | 0.029 | 0.032 |  |  | 0.339 | 0.002 | 91 | 0.210 | 4107 |
| P13 | rewrite | 0 | 0.238 | 0.118 | 0.026 | 0.029 | 0.054 | 0.000 | 0.000 | 0.000 | 0 |  | 0 |
| P14 | norewrite | 1643 | 0.930 | 0.168 | 0.048 | 0.046 |  |  | 0.623 | 0.002 | 25 | 0.520 | 7090 |
| P14 | rewrite | 449 | 0.386 | 0.164 | 0.047 | 0.051 | 0.086 | 0.008 | 0.006 | 0.000 | 10 | 0.000 | 1952 |
| P15 | norewrite | 3262 | 3.107 | 0.372 | 0.093 | 0.126 |  |  | 2.414 | 0.006 | 71 | 2.030 | 28140 |
| P15 | rewrite | 1225 | 1.499 | 0.385 | 0.097 | 0.135 | 0.148 | 0.621 | 0.614 | 0.001 | 64 | 0.560 | 5402 |
| P16 | norewrite | 5635 | 6.051 | 0.591 | 0.202 | 0.380 |  |  | 4.687 | 0.012 | 296 | 4.000 | 73245 |
| P16 | rewrite | 2210 | 2.970 | 0.600 | 0.193 | 0.335 | 0.362 | 1.314 | 1.302 | 0.002 | 83 | 1.160 | 12416 |
| P17 | norewrite | 7635 | 6.948 | 0.815 | 0.281 | 0.509 |  |  | 5.065 | 0.025 | 320 | 4.240 | 136607 |
| P17 | rewrite | 3010 | 5.082 | 0.872 | 0.264 | 0.533 | 0.481 | 2.733 | 2.717 | 0.003 | 80 | 2.430 | 22908 |
| P18 | norewrite | 9641 | 6.997 | 1.039 | 0.406 | 0.758 |  |  | 4.420 | 0.038 | 321 | 3.030 | 236563 |
| P18 | rewrite | 3806 | 5.413 | 1.092 | 0.372 | 0.789 | 0.647 | 2.281 | 2.261 | 0.005 | 77 | 1.950 | 40088 |
| P19 | norewrite | 12143 | 10.787 | 1.348 | 0.524 | 1.331 |  |  | 7.106 | 0.046 | 325 | 5.410 | 380873 |
| P19 | rewrite | 4810 | 8.974 | 1.359 | 0.481 | 1.152 | 0.851 | 4.623 | 4.595 | 0.008 | 320 | 4.280 | 61378 |
| P20 | norewrite | 17852 | 6.652 | 1.023 | 0.465 | 0.616 |  |  | 4.016 | 0.028 | 318 | 1.620 | 75871 |
| P20 | rewrite | 3632 | 3.876 | 1.026 | 0.469 | 0.814 | 1.334 | 0.054 | 0.005 | 0.000 | 0 | 0.000 | 11996 |

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
