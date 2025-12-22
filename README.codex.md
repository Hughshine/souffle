# Codex Quickstart

Use this file to orient Codex sessions. It lists the essential docs, code hotspots, and how to run experiments.

## Scope
- Online incremental path (`--online`) is the active implementation; legacy `--inc` backend is deprecated.
- Incremental modes skip rewrite; rewrite docs apply to full-mode runs.

## Key Docs (full/rewrite)
- `README.rewrite.md` — current rewrite pipeline design, status, perf notes.
- `README.rewrite.2.md` — earlier/alternative optimization plan.
- `README.rewrite.impl.md` — experiment logs and insights collected so far.
- `README.siso.md` — SISO definition/assumptions.
- `README.eval.md` — how to run full experiments and compare outputs/timings.

## Key Docs (online incremental)
- `README.eval.inc.md` — how to run incremental evals and compare outputs/timings.
- `README.inc.region.md` — regional incremental pipeline notes/usage.
- `experiments/side_channel_inc_mini/README.md` — mini incremental benchmark notes.

## Core Code (full/rewrite)
- `src/include/souffle/problog/GraphRewriter.h` — rewrite logic (SISO detection loop, local DD/WMC, edge summarization).
- `src/include/souffle/problog/GraphAnalyzer.h` — SISO detection, heuristics/filters.
- `src/include/souffle/problog/Pipeline.h` + `ForwardCompilation.h` — orchestration and BDD construction.

## Core Code (online incremental)
- `src/include/souffle/problog/RegionalIncremental.h` — inc-regional pipeline.
- `src/include/souffle/problog/IncRegionAnalyzer.h` — regional impact analysis and timing breakdown.
- `src/include/souffle/cli/Cli.h` — iter probability dumps (inc-naive/inc-regional/full).
- `src/ast2ram/online/*` — online translation for DRed-like delta relations and `_inc` strata.

## Experiment How-To (full/rewrite; side_channel_full)
- Per benchmark dir (e.g., `experiments/side_channel_full/P12`):
  - No rewrite: `./compute_new -F ./input -D ./output_no_rewrite_xx > run_no_rewrite_xx.log`
  - With rewrite: `./compute_new -r -F ./input -D ./output_rewrite_xx > run_rewrite_xx.log`
  - Outputs should match: `diff output_no_rewrite_xx/facts.prob output_rewrite_xx/facts.prob`
  - Logs contain `[pipeline]` timings (create graph, pruning, SISO detection/rewrite, BDD init/build, WMC).

## Experiment How-To (online incremental; side_channel_inc_eval)
- Per benchmark dir (e.g., `experiments/side_channel_inc_eval/P9`):
  - inc-naive: `./compute -F input -D output_run_inc_naive --setmode inc < delta/inc10_1.txt`
  - inc-regional: `./compute -F input -D output_run_inc_regional --setmode inc-regional < delta/inc10_1.txt`
  - full baseline (no rewrite): `./compute -F input -D output_run_full --setmode full < delta/inc10_1.txt`
  - Outputs should match: `diff output_run_inc_naive/facts.prob output_run_full/facts.prob`
  - Debugger JSON reports now land in the output dir; the filename uses the basename of `--logfile` plus a timestamp.

## Current State (full/rewrite)
- Reverted aggressive fact-prefix/folding; simple SISO only updates edge probability, not folding nodes.
- Region filter: edgeCount ≥1 or nodeCount >2, with `kMaxEdges` default 5; RV count not enforced.
- P5: rewrite ≈ no-rewrite (~0.33–0.34s, BDD ~210 nodes).
- P12: rewrite slower (~6.5s vs ~3.3s) mainly due to SISO rewrite (~3.3s); outputs match.

## Current State (online incremental)
- Online incremental pipelines are delta-driven and do not run rewrite (`--setmode inc` / `--setmode inc-regional`).
- Bi-imp merge is disabled for inc/inc-regional; a merged graph asserts if you try to switch to incremental.
- Session focus: inc-naive vs inc-regional timing + consistency; see Context Dump for latest P4–P13 runs and P1/P3 skip note.

## Investigation Tips
- Profile per-region cost (detect/build/WMC/apply, live nodes) if slowing down.
- Consider heuristic skips for low-payoff regions and analytic shortcuts for very simple SISOs (facts/linear chains).
- Keep changes small; test on both small (P5) and larger (P12/P1x) cases and report timings plus BDD sizes.

## Context Dump (2025-12-20)
- Recent code changes:
  - `src/include/souffle/cli/Cli.h`: iter probability dumps now use `opt.getOutputFileDir()` (no hardcoded `./output/`), with iter filenames tagged `-inc-naive`, `-inc-regional`, `-full`.
  - `src/include/souffle/problog/IncRegionAnalyzer.h`: emits per-step timing line `[inc-analyze]` (leastParents, scopes, reach, expand, deltaReach, mergeable, total).
  - `src/include/souffle/problog/RegionalIncremental.h`: regional pipeline asserts baseline formulas exist before update; tracks region/boundary stats.
  - `src/include/souffle/problog/formula/SddManager.h`: raw-to-internal var mapping (`rawToInternalIndex`) and related caches.
  - `src/include/souffle/problog/DerivationGraph.h` + `IncRegionAnalyzer.h`: impacted maps use `unordered_set`, and delta-insert reachable union cache is built during prune-inc and reused by `deltaReachable_`.
  - Other files currently modified in worktree include `ForwardCompilation.h`, `CuddManager.h`, `FormulaManager.h`, `MainDriver.cpp`, `CompiledOptions.h` (check `git status` for full list).
- Incremental/full runs (no rewrite; this session focuses on incremental pipelines only):
  - Regenerated P4–P13 (skip P1/P3) with cleanup, then recompiled:
    - `python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py --base-dir experiments/side_channel_inc_eval generate --cases 4-13 --cleanup`
    - `python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py --base-dir experiments/side_channel_inc_eval delta --cases 4-13 --cleanup`
    - `python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py --base-dir experiments/side_channel_inc_eval compile --cases 4-13 --timeout 300`
  - Runs per case use delta `inc10_1`, output dirs `output_run_{inc_naive,inc_regional,full}`, timeout 30s each (via `timeout 30s` + `/usr/bin/time -p`).
  - Consistency: for P4–P13, `facts.prob` and `fact-iter1/2` match full across inc-naive and inc-regional.
  - Wall times (real seconds):
    - P4: inc-naive 0.39, inc-regional 0.12, full 0.12
    - P5: inc-naive 0.13, inc-regional 0.12, full 0.13
    - P6: inc-naive 0.18, inc-regional 0.16, full 0.13
    - P7: inc-naive 0.17, inc-regional 0.17, full 0.14
    - P8: inc-naive 0.36, inc-regional 0.59, full 0.17
    - P9: inc-naive 0.21, inc-regional 0.54, full 0.19
    - P10: inc-naive 0.92, inc-regional 0.73, full 0.68
    - P11: inc-naive 0.75, inc-regional 0.73, full 0.67
    - P12: inc-naive 2.98, inc-regional 4.37, full 2.47
    - P13: inc-naive 8.74, inc-regional 13.06, full 7.83
- Compile note: `side_channel_inc.py compile` does **not** call bare `souffle -o compute`; it runs (per case dir):
  - `souffle --online -F input -D output compute.souffle.dl -o compute` plus any `--souffle-arg` extra flags.
  - `--online` is required for CLI/incremental runs; keep this when compiling manually.
- Pending: P1/P3 runs still skipped due to ForwardCompilation loop on KEY_SENSITIVE eq-cycle.
- Inc-regional profiling focus (P12, delta `inc10_1`, profile2 logs):
  - Analyze time dominated by `deltaReach` (~653 ms of ~679 ms total); other sub-steps are single-digit ms.
  - Rebuild time dominated by `rebuildLoop` (~961 ms of ~974 ms total); init/depGraph/indegree are small.
  - Rebuild stats: edgesProcessed=2333, edgesRebuilt=2333, nodesUpdated=2333, region cycles=4017.
  - CUDD reordering (turn-3, insert): inc-regional ~0.920 s vs inc-naive ~0.220 s vs full ~0.200 s.
  - BDD live_nodes (turn-3): inc-regional 16727 > full 13965 > inc-naive 11904.
  - Interpretation: rebuild itself is heavy; elevated reordering likely happens outside the rebuild loop (post-rebuild FC/WMC/calibration).
  - Logs: `log_P12_inc10_1_inc_naive_profile2_20251220_181410.json`,
    `log_P12_inc10_1_inc_regional_profile2_20251220_181414.json`,
    `log_P12_inc10_1_full_profile2_20251220_181416.json`,
    stdout `experiments/side_channel_inc_eval/P12/run_inc_regional.profile2.stdout`.
- Execution log (2025-12-20, P12 delta `inc10_1`, profile3):
  - Code change: `IncRegionAnalyzer::deltaReachable_` now reuses `getNodeImpactedByDeltaInsert()` /
    `getEdgeImpactedByDeltaInsert()`; no delete seeds; no full-edge scan; fallback only adds delta insert nodes/edges.
  - Build: `cmake --build cmake-build-release --target souffle -j4`
  - Compile P12: `python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py --base-dir experiments/side_channel_inc_eval compile --cases 12 --timeout 300`
  - Run P12 (inc10_1): `--setmode inc`, `inc-regional`, `full` with `--logfile log_P12_inc10_1_*_profile3`
    - stdout: `experiments/side_channel_inc_eval/P12/run_{inc_naive,inc_regional,full}.profile3.stdout`
    - json: `log_P12_inc10_1_inc_naive_profile3_20251220_193358.json`,
      `log_P12_inc10_1_inc_regional_profile3_20251220_19343.json`,
      `log_P12_inc10_1_full_profile3_20251220_19346.json`
  - Analyze timing (inc-regional): `deltaReach≈8.254 ms`, `total≈40.126 ms` (from `run_inc_regional.profile3.stdout`).
  - Rebuild timing (inc-regional): `rebuildLoop≈1211.836 ms`, `total≈1228.702 ms`.
  - Correctness: `facts.prob` and `fact-iter1/2` match full for inc-naive and inc-regional.
- Execution log (2025-12-20, P12 delta `inc10_1`, profile5):
  - Code change: impacted maps now `unordered_set`, with delta-insert reachable union cache built in prune-inc and reused by `deltaReachable_`.
  - Build: `cmake --build cmake-build-release --target souffle -j4`
  - Compile P12: `python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py --base-dir experiments/side_channel_inc_eval compile --cases 12 --timeout 300`
  - Run P12 (inc10_1): output dirs `output_run_*_profile5`, `--logfile log_P12_inc10_1_*_profile5`
    - stdout: `experiments/side_channel_inc_eval/P12/run_{inc_naive,inc_regional,full}.profile5.stdout`
  - Correctness: `facts.prob` and `fact-iter1/2` match full for inc-naive and inc-regional.
  - Wall times (real seconds): inc-naive 3.61, inc-regional 4.06, full 2.99.
  - Analyze timing (inc-regional): `deltaReach≈0.591 ms`, `total≈23.283 ms` (from `run_inc_regional.profile5.stdout`).
  - Rebuild timing (inc-regional): `rebuildLoop≈1014.395 ms`, `total≈1026.708 ms`.
- Execution log (2025-12-20, P12 delta `inc10_1`, profile8):
  - Code change: `reach_filter_` now uses only delta-reachable cache (no patching), and `ReachInfo` stores `unordered_set`.
  - Build: `cmake --build cmake-build-release --target souffle -j4`
  - Compile P12: `python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py --base-dir experiments/side_channel_inc_eval compile --cases 12 --timeout 300`
  - Run P12 (inc10_1): `--setmode inc-regional` with `--logfile log_P12_inc10_1_inc_regional_profile8`
    - stdout: `experiments/side_channel_inc_eval/P12/run_inc_regional.profile8.stdout`
  - Analyze timing (inc-regional): `reach≈0.586 ms`, `total≈23.841 ms` (from `run_inc_regional.profile8.stdout`).

## Context Dump (2025-12-21)
- Recent code changes:
  - Added `--dumpjson`, `--dumpdot`, `--dumpstat` options (default false) in `src/include/souffle/CompiledOptions.h` and CLI `set/unset` handling in `src/include/souffle/cli/Cli.h`.
  - `DerivationGraphViewInterface` gates `dumpJson`/`dumpDot`/`dumpStatistics` outputs; `dumpStatisticsInc` and `dumpStatistics` now no-op unless `dumpstat` is enabled.
  - `dumpJsonInc` no longer emits `impact_by_*` blocks (reduced JSON size).
  - Pipeline wiring: `src/include/souffle/problog/Pipeline.h` sets dump flags so CLI and batch runs are consistent.
  - Non-tty CLI runs now consume stdin via `std::getline` (so `< delta/*.txt` works reliably); `running` is initialized to `true`.
  - Debugger JSON reports now write into the `-D` output dir; `--logfile` paths are reduced to basename and stdout prints only the filename.
  - Bi-imp merge tracking: `src/include/souffle/problog/DerivationGraph.h` records `biImpMerged` and asserts if merge is enabled with delta changes; `src/include/souffle/cli/Cli.h` forbids inc modes when a graph is merged.
  - Incremental prune skips bi-imp merge and delta-delete canonicalisation; prune now logs delta-delete counts per phase.
  - `tryGarbageCollection()` is invoked at the end of each turn in CLI runs.
  - Forward compilation logs delta counts (inc-naive/inc-regional) and deletion `deletedVarsIndex` size.
  - Inc-regional forward compilation now runs the inc-naive deletion phase before regional insertion.
- P12 repro (inc10_1, manual runs; output dirs suffixed `*_repro`):
  - Compile: `python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py --base-dir experiments/side_channel_inc_eval compile --cases 12 --timeout 300`
  - Runs:
    - inc-naive: `./compute -F input -D output_run_inc_naive_repro --setmode inc < delta/inc10_1.txt`
    - inc-regional: `./compute -F input -D output_run_inc_regional_repro --setmode inc-regional < delta/inc10_1.txt`
    - full: `./compute -F input -D output_run_full_repro --setmode full < delta/inc10_1.txt`
  - Consistency: `facts.prob` and `fact-iter1/2` match full for both inc-naive and inc-regional.
  - Latest repro (2025-12-21 04:06, delta inc10_1):
    - Logs: `log_P12_inc10_1_inc_naive_repro_20251221_040639.json_20251221_4642.json`,
      `log_P12_inc10_1_inc_regional_repro_20251221_040648.json_20251221_4650.json`,
      `log_P12_inc10_1_full_repro_20251221_040656.json_20251221_4659.json`.
    - Wall time (real s): inc-naive 2.67, inc-regional 2.63, full 2.62.
    - Reordering (turn-3): ≈0.210s across all three pipelines.
    - live_nodes (turn-2/3): inc-naive 1642/11870; inc-regional 1625/10545; full 1868/13817.
  - Note: `time` can report `user > real` when OpenMP uses multiple threads. For strict single-thread runs, set `OMP_NUM_THREADS=1 OMP_THREAD_LIMIT=1` (or pass `-j 1`).
