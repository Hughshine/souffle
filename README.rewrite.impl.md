> Note: This file is a running notebook of experiments/ideas, including aggressive versions that are **not** the current state. The active implementation is summarized in `README.rewrite.md`; use this file as historical context.

目前graph rewrite pipeline的问题。

1. siso detection:
  对于一般的SISO，我们不动SI和SO，只简化中间部分。并减少寻找较大的candidate siso，避免过大的detection开销
  但我们仍然detect“最简单的siso”，也就是si -> so（只有一边两点的siso），这种siso我们会希望在rewrite时合并si和so为相同节点。同时我们希望正确更新概率。对于这种siso，限制增加，因为要融合si和so，需要保证si/so也满足限制条件（si只有单独的输出/so只有单独的输入）
2. siso rewrite
  对于特定的SISO模式，我们避免forward compilation，直接用概率公式计算新的概率，避免开销。
  同时支持最简子图的融合。

===


Implementation Notes: SISO Rewrite Pipeline
===========================================

Summary of recent experiments and profiling for the SISO-based rewrite pipeline across side_channel_full benchmarks.

Environment
-----------
- Souffle command: `souffle --online --full-only -F ./input -D ./output compute.souffle.dl -o compute_new`
- Rewrite run: `SOUFFLE_REWRITE_DEBUG=1 ./compute_new --rewrite -F ./input -D ./output_rewrite`
- No-rewrite run: `./compute_new -F ./input -D ./output_no_rewrite`
- All times are wall-clock as printed by the pipeline/logs (ms unless noted).

Key Instrumentation
-------------------
- `GraphRewriter` now logs per-region steps: mgrInit (only first region), build, WMC, apply, forward-comp rounds, BDD live nodes and mem.
- `Pipeline` prints BDD manager init time for both rewrite on/off to align phases.
- Local manager for SISO rewrite is reused across regions; manager init is only charged to the first region.

P9 (baseline, small)
--------------------
- Rewrite on (`run_rewrite_debug6.log`): create 1, prune 3, SISO detect 59, SISO rewrite 533 (10 iters, 164 regions), BDD manager init 70, BDD build 10, rest 0.
  - Per-region build ~0.3–1.5 ms, WMC ~0.0007 ms; first region mgrInit ~154 ms; final BDD live nodes ~349, mem ~0.8 GB.
- Rewrite off (`run_no_rewrite_timed4.log`): create 1, prune 3, SISO detect 54, BDD manager init 318, BDD build 18, per-node WMC 2, dump 1.
- Outputs match.

P10–P14 (copied from problog-benchmark)
---------------------------------------
- P10:
  - Rewrite: create 14, prune 1, detect 3, rewrite 357, BDD init 71, build 2.
  - No-rewrite: create 17, prune 1, detect 3, BDD init 117, build 4.
  - Rewrite faster overall.
- P11:
  - Rewrite: create 14, prune 1, detect 1, rewrite 82, BDD init 68, build 2.
  - No-rewrite: create 15, prune 2, detect 2, BDD init 72, build 3.
  - Rewrite faster overall.
- P12:
  - Rewrite: create 15, prune 32, detect 1787, rewrite 6001 (5 iters, 1190 regions), BDD init 66, build 440, per-node WMC 18.
  - No-rewrite: create 15, prune 28, detect 1685, BDD init 92, build 861, per-node WMC 165.
  - Rewrite faster in build/WMC despite rewrite overhead.
- P13:
  - Rewrite: create 26, prune 64, detect 8608, rewrite 31493 (5 iters, 2810 regions), BDD init 26, build 2473, per-node WMC 124.
  - No-rewrite: create 34, prune 78, detect 8397, BDD init 31, build 1853, per-node WMC 1521.
  - Rewrite slower overall due to large rewrite cost, but WMC stage is much smaller after rewrite.
- P14:
  - Rewrite: create 46, prune 115, detect 19856, rewrite 71379 (5 iters, 4148 regions), BDD init 33, build 2127, per-node WMC 400.
  - No-rewrite: create 65, prune 116, detect 20150, BDD init 30, build 4622, per-node WMC 3621.
  - Rewrite slower overall; still cuts build/WMC roughly in half.

P15 (very large)
----------------
- No-rewrite (`run_no_rewrite.log`): create 129 ms, prune 288 ms, SISO detect 297003 ms, BDD init 719 ms, BDD build 25773 ms, per-node WMC 15997 ms (total ~343 s).
- Rewrite attempts:
  - Debug-on runs hit timeout (>600 s).
  - Non-debug rewrite run (600 s limit) stalled; log only shows create 135 ms, prune 324 ms before timeout.
- Takeaway: rewrite currently too slow on P15; SISO detection alone is very heavy. Needs algorithmic improvement for large instances.

P16–P19
-------
- Builds done; no-rewrite/rewrite not fully executed in batch due to time/memory.
  - Rewrite runs for P16–P19 were killed (Signal 9), likely OOM/time.
  - No-rewrite for P16–P19 not run yet (expected to be heavy, given P15).

Open Items / Ideas
------------------
- Optional GC after each SISO (call `tryGarbageCollection()` in rewrite loop) if we want to keep manager footprint smaller; not yet implemented.
- Reuse manager from rewrite for global build would require unified variable mapping/order; currently global build creates a fresh manager.
- Investigate faster SISO detection or region filtering for large instances (P15+).

Hypotheses on Slowness (brainstorm)
-----------------------------------
- SISO detection cost dominates on large inputs (e.g., P15 detect ~297 s even without rewrite; P14 detect ~20 s). GraphAnalyzer likely O(E*V) with heavy traversals; region counts grow fast.
- Rewrite iterations explode: many small regions (P14: 4148 regions, 5 iters; P13: 2810) → overhead of building subviews and per-region forward comp, even if each region is tiny.
- BDD manager init for the global build is still costly when graph is large (no-rewrite P15: 719 ms; build 25.7 s). Rewrite reduces build/WMC but not enough to offset detect+rewrite overhead.
- Variable ordering heuristic in `WeightedBDDManager::preConfig` recomputes and reorders for each manager; for big graphs this adds to init time (though for rewrite the manager is smaller).
- CUDD GC/autodyn may kick in mid-run; we don’t profile cache/GC hits per region, so hidden costs may exist.
- Region construction may be copying sets/vectors (SubgraphView from unordered_sets) → allocator churn; could switch to views on indices.
- For P15+ the number of regions/iterations might cause quadratic behavior if we rescan the whole graph each round.
- Evidence/WMC stages are negligible; bottlenecks are SISO detection and rewrite bookkeeping, not BDD WMC.

Profiling Insights (current)
----------------------------
- For small/medium (P10–P12) rewrite reduces global BDD build/WMC and wins overall.
- For larger (P13–P14) rewrite cuts BDD build/WMC but total time dominated by detection+rewrite; net slower.
- Per-region forward comp is very fast (sub-ms build, ~0.0007 ms WMC); first region pays manager init (~150 ms on P9).
- BDD live nodes after all P9 regions: ~349 nodes, ~0.8 GB reported by CUDD (likely including manager overhead).
- Global BDD manager init grows with graph size: 70 ms (P9 rewrite) vs 318 ms (P9 no-rewrite); 719 ms on P15 no-rewrite.
- Global BDD build/WMC: rewrite helps (P14 build 2.1 s vs 4.6 s; per-node WMC 0.4 s vs 3.6 s), but SISO detect/rewrite dwarfs savings.
