# Rewrite Pipeline Notes

## Source references
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/problog/GraphRewriter.h](src/include/souffle/problog/GraphRewriter.h)
- [src/include/souffle/problog/GraphAnalyzer.h](src/include/souffle/problog/GraphAnalyzer.h)
- [src/include/souffle/problog/ImplicitSplitRewrite.h](src/include/souffle/problog/ImplicitSplitRewrite.h)
- [src/problog/ImplicitSplitRewrite.cpp](src/problog/ImplicitSplitRewrite.cpp)
- [problog-benchmark/taint_inc.py](problog-benchmark/taint_inc.py)
- [problog-benchmark/runs/README.md](problog-benchmark/runs/README.md)


This file summarizes the SISO rewrite pipeline: design, current state (post revert), observed behavior, and near‑term plans.

## Status
- Active pipeline summary; update with new runs and behavior changes.

## Scope
- Full-mode pipeline only; if rewrite runs, the incremental CLI is skipped for that run.
- Online compilation is default; `--online` is optional (kept in commands when shown).

## Companion Docs
- Use `docs/research/README.rewrite.status.md` for the compact trusted reading of
  current rewrite results, rankings, and provenance pitfalls.
- Use `docs/topics/rewrite/README.implicit.md` for the detailed purpose,
  architecture, invariants, and pipeline integration of implicit rewrite.
- Use this file for implementation behavior and detailed optimization notes.

## Goal
- Iteratively find SISO regions (single entry/exit) in the derivation graph, summarize each region into a single probabilistic edge, then run the usual forward compilation on the smaller view.

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

## Latest evaluation (2026-01-05, full rule set)
Settings:
- det-opt behavior always on (`--det-opt` explicit or default-on); `--rewrite` toggled.
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

## Taint Follow-up TODO (2026-03-05)

Recent taint full-pipeline runs (sampled datasets) show strong stage asymmetry:
- `typefilter-dlog` and `pt-obj-dlog` are often highly rewriteable (mostly all-facts regions).
- `cipt-cg-dlog` often gets weak shrink only (typical `rv_ratio` near `0.97-0.99` in hard cases).
- For `cipt-cg-dlog`, weak payoff is mostly semantic (mutual recursion among
  `ci_pt`, `ci_IM`, `ci_reachable*`, `interprocAssign`, `ci_fpt`) rather than
  a pure implementation artifact.

### TODO 1: Move split toward implicit rewrite-time behavior

Goal:
- Reduce repeated full-graph traversals caused by explicit split rounds.

Direction:
- When possible, apply split-equivalent rewiring as part of local rewrite application
  and region maintenance, instead of a separate global split pass.
- Prioritize this path for `typefilter-dlog` and `pt-obj-dlog`, where rewrite can
  already remove most random variables and split overhead is easier to amortize.
- Keep semantic equivalence with existing split modes and preserve regression checks
  (including `rewrite_split_modes_equiv`).

Current status (2026-03-09):
- Runtime implicit rewrite now preserves probability semantics for the checked
  `side_channel_full` cases (`P15`, `P19`, `P20`) and for sampled taint
  `typefilter-dlog`.
- The overlay fast-path layer now covers all fast-path region kinds that are
  currently active in the legacy detector:
  - `single-hyperedge`
  - `linear-two-edge`
  - `parallel-edge`
  - `fan-out-converge`
  - `all-facts`
- The key correctness fix was in materialization: if an overlay fast path rewrites
  an edge probability, the materialized residual edge must be synthetic
  (`rule=nullptr`) so the new probability is not overwritten by the original rule
  weight.
- With that fix in place, runtime overlay `single-hyperedge` can stay enabled.

Known performance issue:
- On highly rewriteable taint stages such as `typefilter-dlog`, the remaining cost
  is no longer legacy `GraphRewriter`; it is the implicit overlay plus the
  runtime handoff around fully precomputed outputs.
- Recent optimization passes improved the representative sampled
  `and-roc/typefilter-dlog` benchmark substantially:
  - standalone implicit benchmark:
    - before split-side fixes: `total_ms ~= 16.78s`
    - after removing per-edge alias index maintenance: `total_ms ~= 10.33s`
    - after all-facts specialized fast path + skip-empty rebuild: `total_ms ~= 5.48s`
  - key sub-metrics moved as follows:
    - `split_alias_apply_ms`: `~8.63s -> ~0.48s`
    - `overlay_fastpath_ms`: `~6.06s -> ~2.41s`
    - `rebuild_index_ms`: `~1.92s -> ~1.43s`
    - `fastpath_iterations`: `2 -> 1`
  - full stage runtime on a fresh compiled binary:
    - earlier implicit: `real ~= 39.10s`
    - current implicit: `real ~= 24.68s`
    - no-rewrite baseline artifact: `real ~= 68.25s`
    - output check: `facts.prob` exact match (`968463` lines, diff `0`)
- The residual overhead in the full stage is now mostly:
  - derivation graph creation (`~5.1s`)
  - pruning (`~3.4s`)
  - implicit rewrite + handoff (`~9.5s`, of which pure overlay work is `~6.5s`)
  - probability dump (`~3.4s`)
- This means the dominant rewrite-time bottleneck is currently index rebuilds and
  all-facts handling on very large output sets, not downstream DD work.

Working hypothesis:
- `partitionFactOutgoingEdgesNaive()` was previously too expensive because alias
  application updated outgoing-edge indices incrementally. That issue is now
  largely fixed; for all-facts-heavy stages the bigger remaining cost is
  rebuilding those indices and moving large precomputed output maps through the
  pipeline.
- The overlay fast-path loop was previously paying region-selection costs for
  all-facts candidates. A specialized all-facts pass now avoids most of that
  work, but mixed-pattern graphs can still spend noticeable time on residual
  materialized rewrite.

Next optimization directions:
1. Reduce `rebuildActiveEdgeIndices()` cost further; this is now a first-class
   hotspot on all-facts-heavy taint stages.
2. Avoid large precomputed-output bookkeeping in the pipeline when the residual
   graph is empty and outputs are already fully covered by tuple-level results.
3. Add more mixed-pattern benchmarks (for example side-channel `P19/P20`) before
   changing `single-hyperedge` scheduling again; the current all-facts
   specialization helps taint strongly but can slightly shift mixed-case costs.
4. Keep measuring `overlay_split_ms`, `overlay_fastpath_ms`, and
   `rebuild_index_ms` independently; those numbers now reflect the true implicit
   overhead much better than total rewrite time.

Iterative implicit rewrite status (2026-03-10):
- The first working iterative prototype was correct but slower because it still
  paid repeated whole-graph work:
  - re-splitting after every outer round
  - rebuilding overlay indices from materialized shadow facts
  - running extra legacy graph-rewrite rounds on already-small residual graphs
- Two targeted fixes helped substantially on mixed-pattern side-channel cases:
  1. cache per-base-fact split work by the current base outgoing active edge set
  2. do not split materialized shadow facts again when rebuilding an overlay
- Representative mixed case: `side_channel_full/P20/output/derivation.json`
  - iterative standalone before these fixes:
    - `total_ms ~= 5.50s`
    - `overlay_prep_ms ~= 2.72s`
    - `graph_rewrite_ms ~= 1.93s`
  - iterative standalone after these fixes:
    - `total_ms ~= 3.71s`
    - `overlay_prep_ms ~= 1.85s`
    - `graph_rewrite_ms ~= 1.21s`
  - final residual still matches explicit rewrite:
    - `nodes_after = 6529`
    - `edges_after = 6319`
    - `graph_rv_after = 3606`
- Fresh compiled full runtime on `P20` now shows the same ordering, with exact
  output agreement across all variants:
  - `no rewrite`: `real ~= 6.82s`
  - legacy `--rewrite`: `real ~= 5.17s`
  - `--implicit-rewrite`: `real ~= 4.03s`
  - `--implicit-iterate-split-rewrite`: `real ~= 3.89s`
- Current mixed-case bottlenecks are no longer split BFS itself:
  - `rebuild_index_ms` inside overlay fast paths
  - residual `graph_rewrite_ms` after the overlay phase
  - detecting when an extra iterative outer round will not buy further shrink

Persistent-overlay redesign status (2026-03-10, later update):
- The iterative path now keeps one `ImplicitSplitOverlay` alive across rounds.
  The new structure is:
  1. build overlay once
  2. run `applySplit(...)` and overlay fast paths on that same overlay for one
     or more rounds
  3. materialize the residual graph at most once
  4. run legacy `GraphRewriter` on the residual graph at most once
- This removes the earlier overlay/graph/overlay round-trip from iterative
  mode. In other words, iterative is now a true persistent-overlay variant, not
  “multi-round single-pass”.
- Correctness checks after the redesign:
  - `souffle-implicit-split-smoke`: pass
  - `SOUFFLE_BIN=./build/src/souffle examples/running_example/run.sh`: pass
  - `ctest --test-dir build -L regression`: pass (`10/10`)
  - fresh `side_channel_full` outputs for `P15/P19/P20`:
    - legacy vs implicit vs iterative: exact `facts.prob` match for all three cases
- Fresh compiled `side_channel_full` runtime after the persistent-overlay redesign:
  - `P15`
    - legacy `--rewrite`: `0.939s`
    - `--implicit-rewrite`: `1.050s`
    - `--implicit-iterate-split-rewrite`: `0.980s`
  - `P19`
    - legacy `--rewrite`: `5.931s`
    - `--implicit-rewrite`: `6.315s`
    - `--implicit-iterate-split-rewrite`: `6.304s`
  - `P20`
    - legacy `--rewrite`: `6.631s`
    - `--implicit-rewrite`: `5.232s`
    - `--implicit-iterate-split-rewrite`: `5.838s`
- Internal `FC_WMC_HYBRID` timings on those same fresh runs:
  - `P15`
    - legacy: `rewrite_ms=316`
    - implicit: `rewrite_ms=403`, `implicit_overlay_prep_ms=62`, `implicit_graph_rewrite_ms=212`
    - iterative: `rewrite_ms=369`, `implicit_overlay_prep_ms=61`, `implicit_graph_rewrite_ms=190`
  - `P19`
    - legacy: `rewrite_ms=2778`
    - implicit: `rewrite_ms=3222`, `implicit_overlay_prep_ms=877`, `implicit_graph_rewrite_ms=1602`
    - iterative: `rewrite_ms=3158`, `implicit_overlay_prep_ms=917`, `implicit_graph_rewrite_ms=1552`
  - `P20`
    - legacy: `rewrite_ms=3920`
    - implicit: `rewrite_ms=2634`, `implicit_overlay_prep_ms=1138`, `implicit_graph_rewrite_ms=925`
    - iterative: `rewrite_ms=3245`, `implicit_overlay_prep_ms=1452`, `implicit_graph_rewrite_ms=1146`
- Current conclusion:
  - The persistent-overlay redesign fixed the architectural issue that made
    iterative pay repeated overlay<->graph handoff costs.
  - However, iterative is still not consistently better than single-pass
    implicit on fresh mixed-pattern cases.
  - The remaining issue is semantic/payoff, not basic architecture: under the
    current `naive-split` definition, a second split round often finds no new
    profitable aliases, so extra overlay work becomes pure overhead.
  - `P20` is the representative case to watch: even there, the fresh residual
    random-variable count after rewrite stayed the same for implicit and
    iterative (`rand_vars=4472`), which explains why the extra round did not
    pay for itself.
- Near-term implication:
  - Further iterative tuning should focus on deciding whether another split
    round can create new aliases before paying its bookkeeping cost. Without
    that signal, the persistent-overlay implementation is correct but will not
    reliably beat the single-pass path.

Taint rerun pitfall (2026-03-10):
- A direct rerun of `and-roc / typefilter-dlog` can silently become
  incomparable if the dataset/binary pair is mixed.
- The validated comparison used the previously generated v2 sampled artifact
  pair under:
  - `problog-benchmark/runs/taint_v2_androc_typefilter_compare_20260309`
  - corresponding sampled v2 bundle metadata under
    `problog-benchmark/taint_datasets/final-bundles/v2_semantic_subsetprob_noderv0`
- That checked run produced:
  - `facts.prob` lines: `968463`
  - `typeFilter.csv` lines: `968463`
  - implicit handoff: `precomputed_nodes=968463`
- A later quick rerun accidentally used:
  - source: `problog-benchmark/.worktree/full-artifact/taint_full/shared_build/typefilter-dlog/compute.souffle.dl`
  - input: `problog-benchmark/.worktree/full-artifact/taint_full/and-roc/stages/typefilter-dlog/input`
- That is a different generated artifact set. On this pair:
  - `typeFilter.csv` had only `8477` rows
  - `ipFilter.csv` had `8187856` rows
  - pruning saw `output_nodes=0`
  - `facts.prob` was empty
- Therefore that rerun is not evidence about rewrite performance. It is only a
  path-mismatch pitfall.
- Rule going forward:
  - for taint rewrite comparisons, always record and reuse the exact generated
    `compute` binary and matching stage input directory from the same sampled
    bundle/run directory
  - do not mix them with `.worktree/full-artifact/taint_full/*` unless the run
    is being regenerated from scratch for that exact artifact set

### TODO 2: Add per-query formula/derivation rewrite path

Goal:
- Get more rewrite opportunities on cyclic programs by breaking work at
  query/formula boundaries instead of rewriting one whole derivation graph.

Direction:
- Build per-query (or per output component) formula graphs.
- Define sharing strictly by stratum boundary:
  - allowed: reuse derivations of nodes from earlier strata (already fixed);
  - disallowed: sharing inside the same stratum (must be broken for rewrite).
- In other words, "shared substructure" means cross-stratum reuse only, not
  intra-stratum DAG merging.
- Run rewrite at per-query granularity, then compile rewritten formulas to DD/WMC.
- Use this as an equivalence-preserving way to reduce whole-graph cycle pressure
  before DD construction.
- Ensure result parity with current pipeline (same marginals under identical inputs).

## Related commits
- `UNCOMMITTED` — perf(problog): redesign iterative implicit rewrite around a persistent overlay
- `812ea4081` — docs(repo): refine README narratives
- `3e9b024ca` — docs(readme): refresh eval and pipeline notes
- `4dd403de4` — Translate Chinese comments and docs to English
