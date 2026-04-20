# Rewrite Pipeline Notes

## Source references
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/problog/GraphRewriter.h](src/include/souffle/problog/GraphRewriter.h)
- [src/include/souffle/problog/GraphAnalyzer.h](src/include/souffle/problog/GraphAnalyzer.h)


This file summarizes the full-mode rewrite pipeline family: the legacy explicit
SISO rewrite (`--rewrite`) and the newer implicit-split pipeline
(`--implicit-rewrite`), along with current behavior and near-term plans.

## Status
- Active pipeline summary; update with new runs and behavior changes.

## Scope
- Full-mode pipeline only; if rewrite runs, the incremental CLI is skipped for that run.
- Online compilation is default; `--online` is optional (kept in commands when shown).

## Goal
- Iteratively find SISO regions (single entry/exit) in the derivation graph, summarize each region into a single probabilistic edge, then run the usual forward compilation on the smaller view.

## Rewrite modes
- Default full-mode runs do not rewrite.
- `--rewrite`: explicit SISO rewrite. The pipeline rewrites the live derivation
  graph in place via `GraphRewriter` and then runs the usual hybrid FC/WMC stage.
- `--implicit-rewrite`: implicit-split rewrite. The pipeline first runs the
  implicit split/materialize flow, carries precomputed tuple probabilities
  forward, rebuilds the live graph/view, and then hands the result to the same
  full-mode FC/WMC machinery.
- Both modes still report their end-to-end cost inside `FC_WMC_HYBRID`; the
  maintained artifact compares plain vs implicit rewrite by default, while the
  appendix taint benchmark contrasts plain, explicit, and implicit rewrite.

## Implementation (current state)
- Detection: `GraphAnalyzer::detectAllSISOStrictFromExit(view)` on the working `IncSubgraphView`; cached incoming edges are cleared each iteration.
  The detector currently returns fast-path regions only (single-hyperedge, linear two-edge, parallel-edge,
  fan-out converge, and all-facts). Dominance-based detection is disabled in `GraphAnalyzer`.
- Non‑trivial filter: simple fact regions (entry fact → single edge → exit) always
  pass; otherwise require `edgeCount > 0`, `nodeCount > 2`, and `edgeCount <= maxEdges`.
  Default `maxEdges` is 5 and can be overridden via `SOUFFLE_SISO_MAX_EDGES`.
  Random-variable count is **not** enforced after the revert.
- Local inference: build BDD formulas for the region, compute `Pr(exit | entry)` via weighted model counting.
- Rewrite action: add a new edge `entry -> exit` with that probability; remove internal edges and non‑boundary nodes from the **view** (underlying graph only gains the new edge). Simple SISO currently only updates edge probability; nodes are not folded.
- Fixpoint: loop detection + rewrite until no region is rewritten. A split pass may run at fixpoint depending on `--split-mode` (default `naive-split`); if split rewires edges, rewrite continues until both rewrite and split reach fixpoint.
- After rewrite, component-wise FC/WMC uses a single BDD manager reused across
  slow components; initialization is delayed until slow components exist.
- Single‑randvar and conjunctive fast paths evaluate eligible components without
  DD managers.
- Component‑wise FC + WMC are timed together under `FC_WMC_HYBRID`, which now
  starts before rewrite so its wall time includes rewrite + split.
- Probabilities are stored only for `needOutput` nodes (plus precomputed facts),
  so `IO_DUMP` no longer sorts large internal node sets.
- With `--dumpdot`, the pipeline prints `fc-component-info` lines listing each
  component’s size, rand vars, fast/slow mode, and the slow‑path reason.
- Logging: pipeline logs timings; rewrite stats include iterations, region counts, nodes/edges removed/added.

### Implicit-specific behavior
- CLI surface: `--implicit-rewrite` turns on rewrite mode and selects the
  implicit pipeline; `--rewrite` continues to mean the explicit pass.
- The implicit pipeline records `rewrite_engine=implicit` in the hybrid stage
  metadata and logs overlay/materialization timing such as
  `implicit_overlay_prep_ms` and `implicit_graph_rewrite_ms`.
- In the current-source compiler, implicit full-mode always keeps the overlay
  `all-facts` fast path enabled. Alternative gating / disable switches are no
  longer part of the maintained behavior.
- The pipeline carries precomputed tuple probabilities from the implicit split
  pass into the final output map so correctness checks can compare implicit and
  non-rewrite runs directly on `facts.prob`.

### Implicit profiling and JSON output
- The maintained full artifact writes these values into the default JSON stage
  log whenever a run uses `--implicit-rewrite`; no extra profiling flag is
  required.
- `FC_WMC_HYBRID` remains the top-level rewrite stage for end-to-end timing, and
  its `info` map now includes the implicit substage totals used by the artifact
  benchmark collectors.
- Key fields:
  - `implicit_overlay_siso_detect_ms`: time spent identifying SISO / fast-path
    candidates in the overlay view.
  - `implicit_overlay_siso_summarize_ms`: time spent applying SISO
    summarization, including the fast-path rewrite action itself.
  - `implicit_graph_bdd_compile_ms`: total BDD compilation time spent inside the
    implicit graph rewriter.
  - `implicit_graph_bdd_wmc_ms`: total BDD WMC time spent inside the implicit
    graph rewriter.
  - `implicit_graph_apply_ms`: total time applying rewritten region results back
    to the implicit graph.
- `implicit_overlay_fastpath_ms` is a wider umbrella timer than the SISO pair
  above. It includes fast-path control/dispatch overhead in addition to the
  actual rewrite time captured by `implicit_overlay_siso_summarize_ms`.

## Latest explicit evaluation (2026-01-05, full rule set)
Settings:
- `--det-opt` always on; `--rewrite` toggled.
- Compile with `--full-only`.
- Split mode: default `naive-split`.
- Backend: BDD (CUDD); bucketed init for small/medium graphs and defaults for large graphs.
- `rand_vars` counts probabilistic facts + probabilistic edges in the pruned view used for FC.
- `rewrite_s` is derived from `rewrite_ms` recorded inside `FC_WMC_HYBRID`.
- `hybrid_s` is `FC_WMC_HYBRID` wall time; it includes rewrite + component FC/WMC.
- `fc_s`/`wmc_s` come from `fc_build_ms` and `wmc_ms` recorded in the hybrid stage.
- `manager_init_ms` is `0` when no slow components exist.
- `dd_live_nodes` for rewrite is the sum of per‑component live nodes; treat it as
  approximate (not directly comparable to the no‑rewrite single‑manager count).

| case | variant | rand_vars | total_s | seminaive_s | create_s | prune_s | rewrite_s | hybrid_s | fc_s | wmc_s | manager_init_ms | reorder_s | dd_live_nodes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| P1 | norewrite | 2 | 0.027135 | 0.014387 | 0.000382 | 0.000447 |  |  | 0.010204 | 0.000055 | 9 |  | 3 |
| P1 | rewrite | 2 | 0.016597 | 0.013901 | 0.000397 | 0.000363 | 0.000000 | 0.000862 | 0.000000 | 0.000000 | 0 |  | 0 |
| P3 | norewrite | 2 | 0.024102 | 0.017207 | 0.000364 | 0.000365 |  |  | 0.004686 | 0.000044 | 4 |  | 3 |
| P3 | rewrite | 2 | 0.017603 | 0.014799 | 0.000329 | 0.000468 | 0.000000 | 0.000972 | 0.000000 | 0.000000 | 0 |  | 0 |
| P4 | norewrite | 1 | 0.010230 | 0.002478 | 0.000308 | 0.000249 |  |  | 0.005823 | 0.000038 | 5 |  | 2 |
| P4 | rewrite | 0 | 0.003259 | 0.001687 | 0.000235 | 0.000120 | 0.000000 | 0.000140 | 0.000000 | 0.000000 | 0 |  | 0 |
| P5 | norewrite | 12 | 0.010050 | 0.002981 | 0.000297 | 0.000336 |  |  | 0.004732 | 0.000065 | 4 |  | 35 |
| P5 | rewrite | 0 | 0.004404 | 0.002201 | 0.000236 | 0.000267 | 0.000000 | 0.000573 | 0.000000 | 0.000000 | 0 |  | 0 |
| P6 | norewrite | 39 | 0.017001 | 0.007366 | 0.000583 | 0.000798 |  |  | 0.006356 | 0.000058 | 5 |  | 117 |
| P6 | rewrite | 6 | 0.009012 | 0.004329 | 0.000460 | 0.000632 | 0.001000 | 0.002142 | 0.000000 | 0.000000 | 0 |  | 0 |
| P7 | norewrite | 56 | 0.017641 | 0.006540 | 0.001445 | 0.000918 |  |  | 0.007017 | 0.000093 | 5 |  | 196 |
| P7 | rewrite | 18 | 0.021883 | 0.004536 | 0.001278 | 0.001889 | 0.003000 | 0.012404 | 0.000000 | 0.000000 | 8 |  | 18 |
| P8 | norewrite | 53 | 0.020783 | 0.009727 | 0.001683 | 0.001290 |  |  | 0.005608 | 0.000093 | 4 |  | 188 |
| P8 | rewrite | 18 | 0.027677 | 0.012210 | 0.001701 | 0.001570 | 0.004000 | 0.010268 | 0.000000 | 0.000000 | 5 |  | 17 |
| P9 | norewrite | 113 | 0.021801 | 0.010571 | 0.001167 | 0.001619 |  |  | 0.006001 | 0.000134 | 4 |  | 432 |
| P9 | rewrite | 31 | 0.025470 | 0.008693 | 0.001375 | 0.001926 | 0.004000 | 0.011391 | 0.000000 | 0.000000 | 5 |  | 18 |
| P10 | norewrite | 32 | 0.056855 | 0.027486 | 0.016697 | 0.001642 |  |  | 0.005771 | 0.000061 | 5 |  | 49 |
| P10 | rewrite | 0 | 0.049272 | 0.025122 | 0.015693 | 0.001507 | 0.000000 | 0.000785 | 0.000000 | 0.000000 | 0 |  | 0 |
| P11 | norewrite | 16 | 0.055691 | 0.025361 | 0.016603 | 0.002846 |  |  | 0.005606 | 0.000111 | 4 |  | 17 |
| P11 | rewrite | 0 | 0.070663 | 0.040482 | 0.020363 | 0.004089 | 0.000000 | 0.000375 | 0.000000 | 0.000000 | 0 |  | 0 |
| P12 | norewrite | 466 | 0.154763 | 0.071156 | 0.017927 | 0.012741 |  |  | 0.045361 | 0.000774 | 31 |  | 1693 |
| P12 | rewrite | 0 | 0.129627 | 0.066559 | 0.014860 | 0.014257 | 0.027000 | 0.027580 | 0.000000 | 0.000000 | 0 |  | 0 |
| P13 | norewrite | 1143 | 0.566284 | 0.125265 | 0.032700 | 0.030995 |  |  | 0.359206 | 0.004237 | 80 |  | 4056 |
| P13 | rewrite | 0 | 0.271306 | 0.123768 | 0.034156 | 0.035865 | 0.065000 | 0.065386 | 0.000000 | 0.000000 | 0 |  | 0 |
| P14 | norewrite | 1643 | 1.057596 | 0.186317 | 0.064414 | 0.069485 |  |  | 0.710356 | 0.005460 | 33 |  | 7112 |
| P14 | rewrite | 449 | 0.414165 | 0.181824 | 0.049822 | 0.057444 | 0.097000 | 0.108727 | 0.000000 | 0.005000 | 0 |  | 0 |
| P15 | norewrite | 3262 | 3.587351 | 0.426634 | 0.131338 | 0.168988 |  |  | 2.805829 | 0.013068 | 49 |  | 28063 |
| P15 | rewrite | 1225 | 0.953004 | 0.400168 | 0.109871 | 0.169324 | 0.200000 | 0.237010 | 0.000000 | 0.021000 | 0 |  | 0 |
| P16 | norewrite | 5635 | 6.571363 | 0.642035 | 0.250246 | 0.397444 |  |  | 5.191910 | 0.025387 | 413 |  | 73472 |
| P16 | rewrite | 2210 | 1.896653 | 0.652436 | 0.229598 | 0.443999 | 0.430000 | 0.507301 | 0.000000 | 0.040000 | 0 |  | 0 |
| P17 | norewrite | 7635 | 8.543895 | 0.910980 | 0.292407 | 0.725272 |  |  | 6.472285 | 0.042987 | 426 |  | 136403 |
| P17 | rewrite | 3010 | 2.800648 | 0.905866 | 0.322921 | 0.668448 | 0.689000 | 0.811796 | 0.000000 | 0.061000 | 0 |  | 0 |
| P18 | norewrite | 9641 | 8.511692 | 1.182473 | 0.503956 | 1.037593 |  |  | 5.590781 | 0.056403 | 396 |  | 236625 |
| P18 | rewrite | 3806 | 3.823239 | 1.150737 | 0.467236 | 0.996635 | 0.906000 | 1.091150 | 0.000000 | 0.096000 | 0 |  | 0 |
| P19 | norewrite | 12143 | 12.326357 | 1.530985 | 0.607021 | 1.453568 |  |  | 8.480337 | 0.078253 | 374 |  | 380558 |
| P19 | rewrite | 4810 | 5.252128 | 1.487400 | 0.572956 | 1.511006 | 1.273000 | 1.538884 | 0.000000 | 0.147000 | 0 |  | 0 |
| P20 | norewrite | 17852 | 8.326195 | 1.143798 | 0.624185 | 0.995282 |  |  | 5.290098 | 0.080939 | 370 |  | 75932 |
| P20 | rewrite | 3632 | 4.892494 | 1.147562 | 0.639239 | 0.972134 | 1.848000 | 1.974036 | 0.001000 | 0.005000 | 1 |  | 319 |

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

## Related commits
- `60bbdc2e4` — perf(problog): expose implicit rewrite profiling in JSON
- `UNCOMMITTED` — docs(rewrite): document explicit vs implicit rewrite modes for full-artifact
- `812ea4081` — docs(repo): refine README narratives
- `3e9b024ca` — docs(readme): refresh eval and pipeline notes
- `4dd403de4` — Translate Chinese comments and docs to English
