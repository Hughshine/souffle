# Incremental Side-Channel Benchmark Guide

## Source references
- [problog-benchmark/side_channel_inc.py](problog-benchmark/side_channel_inc.py)
- [problog-benchmark/side_channel_common.py](problog-benchmark/side_channel_common.py)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [src/include/souffle/problog/ForwardCompilation.h](src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)


Incremental side-channel benchmark notes (context + procedures). Experiments live
under `experiments/side_channel_inc_eval/` (legacy),
`experiments/side_channel_inc_trimmed_eval/` (trimmed ruleset, no equal_assign),
`experiments/side_channel_inc_trimmed_eval_small/` (trimmed ruleset, 0.1/0.3/0.5% deltas),
and `experiments/side_channel_inc_eval_small/` (full ruleset, 0.1/0.3/0.5% deltas),
using the Souffle binary built from this repo. Do not git-add anything under
`experiments/` or other generated artifacts.

## Status
- Evaluation log for an external benchmark repo; results below are historical snapshots.
- Active evaluation workflow (update with new runs after code changes).
- 2026-01-12 (full ruleset, det-opt, apply_delta_graph): P1,P3,P4-P20, inc0p1/inc0p3/inc0p5, sample=1, all OK.
- 2026-01-11 (trimmed ruleset, det-opt, apply_delta_graph): P1,P3,P4-P20, inc0p1/inc0p3/inc0p5, sample=1, all OK.
- 2026-01-11 (full ruleset, det-opt, apply_delta_graph): P1,P3,P4-P20, inc1/inc3/inc5, sample=1, all OK.
- 2026-01-11 (trimmed ruleset, det-opt, apply_delta_graph): P1,P3,P4-P20, inc1/inc3/inc5, sample=1, all OK.
- 2026-01-10 (trimmed ruleset): P1,P3,P4-P20, inc1/inc3/inc5, sample=1, all OK.
- 2026-01-10 (full ruleset): P1,P3,P4-P20, inc1/inc3/inc5, sample=1, all OK (P1/P3 forced trimmed).
- 2026-01-09 (post-fix rerun): P1,P3,P4-P20, inc1/inc3/inc5, sample=1, all OK.
- 2026-01-09 (pre-fix run): P1,P3,P4-P20, inc1/inc3/inc5, sample=1, P7/P20 mismatches (see Results).
- 2025-12-21 (inc10 sample=1): P4-P13 snapshot + notes preserved below.

## Scope
- Uses the compiled-program CLI with `--setmode` (default `inc-naive` in CmdOptions).
  Compiler `--online` flags do not affect the runtime CLI behavior here.
- Incremental modes do not run rewrite; they reuse the online DRed-like delete/insert paths.
- Full baseline runs use `--setmode full` (alias `full-hard`); `full-soft` reuses DD state.

## Prerequisites
- Build release Souffle:
  ```bash
  cmake -S . -B build
  JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
  cmake --build build -j${JOBS}
  ```
- Put the build binary on PATH before running the Python scripts:
  ```bash
  export PATH="/home/hugh/research/datalog/souffle/build/src:$PATH"
  ```
- If you build into `cmake-build-release`, adjust the PATH to
  `.../cmake-build-release/src`.

## End-to-end for P1 (from repo root)
```bash
# 1) Generate inputs from SMT
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval generate --cases 1 --cleanup

# 2) Generate delta workloads (0.1/0.3/0.5% deltas; overwrites delta/)
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval delta --cases 1 --cleanup --change-spec inc0p1=0.001,inc0p3=0.003,inc0p5=0.005

# 3) Compile Souffle (online default; produces ./compute in P1/)
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval compile --cases 1 --timeout 600

# 4) Run baseline (full+inc) and one sample for inc0p1/inc0p3/inc0p5
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval run   --cases 1 --delta-labels inc0p1,inc0p3,inc0p5 --delta-samples 1 --timeout 600 --run-arg=--det-opt

# 5) Collect summary TSV
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval collect --cases 1
```

## What to look at
- Outputs: `output/facts.full.prob`, `output/facts.inc.prob`, and per-turn
  `output/fact-iterN-{inc-naive,inc-regional,full}.prob`. Runner outputs are
  prefixed per delta, e.g.
  `output/delta-<label>-<sample>-{inc-naive,inc-regional,full}-fact-iterN-<mode>.prob`.
- Logs: debugger JSON reports land in `output/` (e.g.,
  `output/log_P1_inc0p1_1_inc_*.json`); stdout from manual runs (e.g.,
  `log_inc_applyDelta_inc*.stdout`) still holds CLI prints.
- Debugger JSON `turns[].mode` shows `FULL-HARD` / `FULL-SOFT` / `INC`.
- CLI runs in inc mode print delta size:
  `[applyDelta] delTuples=... delRuleApps=... delFacts=... insTuples=... insRuleApps=... insFacts=...`.
- CLI logs pre-prune applyDelta counts (symmetric) and view-update counts (asymmetric):
  `[inc-iter N] mode=INC_NAIVE apply_delta_ops: delTuples=... delRuleApps=... delFacts=... insTuples=... insRuleApps=... insFacts=...`.
  `[inc-iter N] mode=INC_NAIVE apply_delta_view: insNodes=... insEdges=... delNodes=... delEdges=...` (view/prune-driven).
  `[inc-iter N] mode=INC_NAIVE apply_delta_graph: totalNodes=... totalEdges=...` (full graph after applyDelta, before prune).
- Forward compilation logs print delta counts:
  `[inc-naive] delta counts: insNodes=... insEdges=... delNodes=... delEdges=...`.
- Deletion stage prints `Deletion deletedVarsIndex size: N` before postprocessing
  variables.
- Prune prints delta-delete counts per phase:
  `[prune-inc] delta-delete counts (start|post-mark-pruned|filtered|canonicalised|view): nodes=... edges=...`.
- Full prune prints graph sizes:
  `[prune-full] pre nodes=... edges=...` and `[prune-full] post nodes=... edges=...`.
- Prune now rebuilds impacted maps on the pruned subgraph; applyDelta no longer
  does impacted BFS.
- Optional debug outputs (default off): `--dumpjson`, `--dumpdot`, `--dumpstat`
  gate JSON/DOT/stats dumps after prune, all written under the `-D` output dir.

## Mismatch debugging workflow
- Identify what mismatched first: open `output/delta-<label>-<sample>.json` and
  note which comparison failed (`inc_iter1_vs_full_iter1`, `inc_regional_iter2_vs_full_iter2`, etc.).
  Use the `output/delta-<label>-<sample>-<mode>-fact-iterN-*.prob` files to diff
  the exact tuples/probabilities for the failing iter (iter1 = delete, iter2 = insert).
- Re-run with derivation dumps to compare graphs:
  - `PATH=/home/hugh/research/datalog/souffle/build/src:$PATH python3 problog-benchmark/side_channel_inc.py --base-dir <base> run --cases <case> --delta-labels <label> --delta-samples <n> --timeout <sec> --run-arg=--det-opt --run-arg=--dumpjson --compare-all`
  - The CLI writes timestamped dumps in `output/`, e.g.
    `derivation-inc-after-prune<iter>-<timestamp>.json` and
    `derivation-full-after-prune<iter>-<timestamp>.json`.
  - Compare node/edge counts and search for missing tuples/edges; if the
    derivation graphs differ, track whether the missing pieces align with the
    delta input relations (bad delta ingestion) or derived edges (FC update).
- If derivation graphs match but probabilities differ, focus on delete/insert FC:
  - Inspect CLI stdout for `[applyDelta]` and `[inc-iter N]` counters, and
    per-stage timers (`--profile-inc-regional`, `--profile-dep-graph`).
  - For region-closure diagnostics, add `--profile-inc-regional-heavy` plus
    `--inc-regional-trace-tuples=<Tuple>` to print backward-closure stats.
  - Add targeted prints around the suspected phase (delete vs insert, regional
    analyze/plan/rebuild) and rebuild; re-run the same delta until the first
    divergence is localized.

## Implementation pointers (online path)
- `src/ast2ram/online/UnitTranslator.cpp`: `inc_table_update` + `_inc` strata.
- `src/synthesiser/Synthesiser.cpp`: `runFunctionInc` and `runAllInc` in generated code.
- `src/include/souffle/cli/Cli.h`: applyDelta + mode dispatch (inc-naive/inc-regional).

## Latest status / known issues
- 2026-01-12 (full ruleset, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5): correctness OK for P1,P3,P4-P20 (P2 missing in source). Summary counts: inc0p1 15/19 faster, inc0p3 15/19 faster, inc0p5 15/19 faster. Avg speedup 1.28/1.22/1.17x (inc0p1/inc0p3/inc0p5).
- 2026-01-11 (trimmed ruleset, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5): correctness OK for P1,P3,P4-P20 (P2 missing in source). Summary counts: inc0p1 15/19 faster, inc0p3 16/19 faster, inc0p5 14/19 faster. Avg ΔE/|E| now 9.8–33.9% (ins) and 10.9–51.2% (del).
- 2026-01-11 (full ruleset, det-opt, apply_delta_graph): correctness OK for P1,P3,P4-P20 (P2 missing in source). Summary counts: inc1 6/19 faster, inc3 3/19 faster, inc5 2/19 faster. ΔE/|E| now uses post-applyDelta full-graph edges (not pruned view edges).
- 2026-01-11 (trimmed ruleset, det-opt, apply_delta_graph): correctness OK for P1,P3,P4-P20 (P2 missing in source). Summary counts: inc1 7/19 faster, inc3 6/19 faster, inc5 1/19 faster.
- 2026-01-10 (trimmed ruleset): correctness OK for P1,P3,P4-P20 (P2 missing in source). Summary counts: inc1 7/19 faster, inc3 7/19 faster, inc5 1/19 faster (legacy, pre apply_delta_graph).
- 2026-01-10 (full ruleset): correctness OK for P1,P3,P4-P20 (P2 missing in source). Summary counts: inc1 17/19 faster, inc3 14/19 faster, inc5 6/19 faster; P1/P3 forced trimmed ruleset in generator.
- 2026-01-09 (post-fix rerun): correctness OK for P1,P3,P4-P20 (P2 missing in source). Summary counts: inc1 16/19 faster (ok=19/19), inc3 8/19 faster (ok=19/19), inc5 2/19 faster (ok=19/19).
- 2026-01-09 (pre-fix run): correctness mismatches (inc_iter1_vs_full_iter1): P7 inc1/inc3/inc5 mismatches=1 max|d|=0.10239319; P20 inc5 mismatches=2 max|d|=0.2522536. Summary counts: inc1 16/19 faster (ok=18/19), inc3 10/19 faster (ok=18/19), inc5 6/19 faster (ok=17/19).
- Delta-size sensitivity: 0.1/0.3/0.5% deltas are mostly faster in full ruleset; 1/3/5% deltas remain mixed-to-slower (full recompute often wins for larger deltas).
- PRUNING_INC dominates on larger cases and especially on insert turns; see the
  per-stage breakdown table for ratios (Speedup < 1.0 indicates inc slower).
- ΔE/|E| is based on rule-app deltas vs total derived edges after applyDelta; small
  input deltas can fan out through recursive rules and drive ΔE/|E| well above the
  input change rate (especially on delete, where the graph also shrinks).
- Delete SEM speedup remains <1 for inc3/inc5 in the full ruleset and mixed in the
  trimmed ruleset; see the delete speedup aggregate table in the SEM summary.
- Delta node/edge counts are taken from forward compilation logs and can be
  asymmetric due to prune/derivation changes; they are not the same as input facts.
- inc-regional is not exercised in the current runs (logs show inc-naive only).
- Current inc1 workloads have no disjunctions, so inc-regional degenerates to inc-naive.
- Analyzer cost has been reduced and is now negligible relative to forward compilation.
- The printed delete/insert timers are small and do not trigger reordering, yet
  incremental delete/insert still show long forward-compilation time relative to
  the delta size; needs profiling to confirm whether this is expected.
- DRed deletion is still inefficient; no fix yet.
- Incrementalizing prune is under consideration but lower priority; prune time
  still contributes non-trivially.
- Elastic incremental pipeline remains a plan; design/implementation is not settled.
- To exercise inc-regional properly we likely need HV-related rules (richer
  side-channel reasoning) or another benchmark (e.g., data race).
- inc-naive (and current inc-regional) is faster than full largely because delta
  impact is small and variable ordering from turn-1 is reused (no reordering on
  insert/delete).
- Continuous inc-regional state maintenance/correctness is still pending; current
  experiments only include a single inc-regional turn (no calibration, so no
  outdated DD applied).
- Earlier runs saw P1/P3 incremental insertion loop in ForwardCompilation on the
  KEY_SENSITIVE eq-cycle; if it reappears, skip those cases.
- 2025-12-21 (inc10): incremental pruning still dominates time for larger cases
  (P12/P13), e.g. P13 inc10: prune ~= 2.6s, forward ~= 1.07s, total ~= 3.71s;
  full run forward+wmc is heavy but still faster overall (~=3.40s).
- 2025-12-21 (inc10): all P4-P13 inc10 runs (delta sample 1) match full results
  (`max|d|=0`) as recorded in `output/delta-inc10-1.json` per case.
- 2025-12-21 (inc10): generated logs per case
  `output/log_P*_inc10_1_inc_*.json` and `output/log_P*_inc10_1_full_*.json`;
  `output/delta-inc10-1.json` holds consistency checks.
- 2025-12-21 (inc10): P12 manual repro (delta inc10_1) still matches; turn-3
  reordering ~=0.21s; live_nodes turn2/3 reflect deletions (inc-naive 1642/11870,
  inc-regional 1625/10545, full 1868/13817).
- Incremental runs disable bi-imp merge; if a graph is merged (full mode),
  switching to inc/inc-regional asserts to avoid cache mismatch.

## Results (inc1/inc3/inc5 + inc0p1/inc0p3/inc0p5, sample=1)
Data sources:
- `experiments/side_channel_inc_eval_small/results-souffle-inc.tsv` (end-to-end, 2026-01-12 det-opt run, apply_delta_graph, inc0p1/inc0p3/inc0p5)
- `experiments/side_channel_inc_eval_small/P*/output/log_P*_<label>_1_{inc,full}_*.json`
  (per-stage breakdown, delta counts, 2026-01-12 inc0p1/inc0p3/inc0p5 run)
- `experiments/side_channel_inc_eval/end-to-end-inc-vs-full.tsv` (end-to-end, 2026-01-11 det-opt run, apply_delta_graph)
- `experiments/side_channel_inc_eval/stage-breakdown.tsv` (per-stage breakdown, 2026-01-11 det-opt run, apply_delta_graph)
- `experiments/side_channel_inc_eval/semnaive-comparison-with-graph.tsv` (SEM + graph/delta counts, 2026-01-11 det-opt run, apply_delta_graph)
- `experiments/side_channel_inc_eval/semnaive-sem-delta-ratio.tsv` (SEM + ΔE/|E| ratio, 2026-01-11 det-opt run, apply_delta_graph)
- `experiments/side_channel_inc_eval/per-case-runtime-summary.tsv` (per-case runtime totals, 2026-01-11 det-opt run, apply_delta_graph)
- `experiments/side_channel_inc_trimmed_eval/end-to-end-inc-vs-full.tsv` (end-to-end, 2026-01-11 det-opt run, apply_delta_graph)
- `experiments/side_channel_inc_trimmed_eval/stage-breakdown.tsv` (per-stage breakdown, 2026-01-11 det-opt run, apply_delta_graph)
- `experiments/side_channel_inc_trimmed_eval/semnaive-comparison-with-graph.tsv` (SEM + graph/delta counts, 2026-01-11 det-opt run, apply_delta_graph)
- `experiments/side_channel_inc_trimmed_eval/semnaive-sem-delta-ratio.tsv` (SEM + ΔE/|E| ratio, 2026-01-11 det-opt run, apply_delta_graph)
- `experiments/side_channel_inc_trimmed_eval/per-case-runtime-summary.tsv` (per-case runtime totals, 2026-01-11 det-opt run, apply_delta_graph)
- `experiments/side_channel_inc_trimmed_eval_small/end-to-end-inc-vs-full.tsv` (end-to-end, 2026-01-11 det-opt run, apply_delta_graph, inc0p1/inc0p3/inc0p5)
- `experiments/side_channel_inc_trimmed_eval_small/stage-breakdown.tsv` (per-stage breakdown, 2026-01-11 det-opt run, apply_delta_graph, inc0p1/inc0p3/inc0p5)
- `experiments/side_channel_inc_trimmed_eval_small/semnaive-comparison-with-graph.tsv` (SEM + graph/delta counts, 2026-01-11 det-opt run, apply_delta_graph, inc0p1/inc0p3/inc0p5)
- `experiments/side_channel_inc_trimmed_eval_small/semnaive-sem-delta-ratio.tsv` (SEM + ΔE/|E| ratio, 2026-01-11 det-opt run, apply_delta_graph, inc0p1/inc0p3/inc0p5)
- `experiments/side_channel_inc_trimmed_eval_small/per-case-runtime-summary.tsv` (per-case runtime totals, 2026-01-11 det-opt run, apply_delta_graph, inc0p1/inc0p3/inc0p5)
- `experiments/side_channel_inc_trimmed_eval_small/results-souffle-inc.tsv` (end-to-end, 2026-01-11 det-opt run, apply_delta_graph, inc0p1/inc0p3/inc0p5)
- `experiments/side_channel_inc_trimmed_eval_small/P*/output/log_P*_<label>_1_{inc,full}_*.json`
  (per-stage breakdown, delta counts, 2026-01-11 inc0p1/inc0p3/inc0p5 run)
- `experiments/side_channel_inc_eval_small/stage-breakdown-20260113_040129.tsv`
  (per-turn stage breakdown, 2026-01-13 det-opt run, full ruleset)
- `experiments/side_channel_inc_eval_small/stage-compare-20260113_040129.tsv`
  (per-turn full/inc stage comparison, 2026-01-13 det-opt run, full ruleset)
- `experiments/side_channel_inc_trimmed_eval/results-souffle-inc.tsv` (end-to-end, 2026-01-10 legacy)
- `experiments/side_channel_inc_trimmed_eval/P*/output/log_P*_<label>_1_{inc,full}_*.json`
  (per-stage breakdown, delta counts, 2026-01-10 legacy)
- `experiments/side_channel_inc_eval/results-souffle-inc.tsv` (end-to-end, 2026-01-10 full ruleset)
- `experiments/side_channel_inc_eval/P*/output/log_P*_<label>_1_{inc,full}_*.json`
  (per-stage breakdown, delta counts, 2026-01-10 full ruleset)
- 2026-01-09/2026-01-10 tables are preserved below; source files under
  `experiments/side_channel_inc_eval/` and `experiments/side_channel_inc_trimmed_eval/` were overwritten by the
  2026-01-11 apply_delta_graph runs. The 0.1/0.3/0.5% run uses
  `experiments/side_channel_inc_trimmed_eval_small/`.

#### 2026-01-13 run notes (full ruleset, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5)
- Config change: CUDD init uses `numVars = 2 * rand_vars` from the initial graph (random facts only),
  `numSlots = 512`, cache/memory tiers unchanged; this fixes the initial manager sizing while keeping the rest of the
  allocator policy the same.
- Correctness: all P1,P3,P4-P20 OK (P2 missing in source); see `experiments/side_channel_inc_eval_small/results-souffle-inc.tsv`.
- End-to-end wins: inc faster in 17/19 (inc0p1), 18/19 (inc0p3), 17/19 (inc0p5).
- Avg speedup (end-to-end): 1.50/1.33/1.28 mean and 1.46/1.28/1.18 median (inc0p1/inc0p3/inc0p5).
- Total delta-run time: sum of inc+full across all cases/deltas ≈ 6.4 min (from `results-souffle-inc.tsv`).
- Per-turn full vs inc stage speedups (median with mean in parentheses), from `stage-compare-20260113_040129.tsv`:

Turn2 = delete:
```tsv
Delta	N	Total	SEM	PRN	FC	WMC
inc0p1	19	4.271 (4.871)	3.799 (3.129)	1.491 (1.554)	2.187 (6.349)	1.320 (1.962)
inc0p3	19	3.114 (3.443)	2.469 (1.991)	1.233 (1.231)	2.470 (6.018)	1.058 (1.660)
inc0p5	19	2.641 (2.926)	1.940 (1.685)	0.908 (1.028)	2.216 (6.160)	1.231 (1.482)
ALL	57	3.114 (3.746)	2.206 (2.268)	1.247 (1.271)	2.264 (6.175)	1.144 (1.702)
```

Turn3 = insert:
```tsv
Delta	N	Total	SEM	PRN	FC	WMC
inc0p1	19	1.607 (3.114)	4.550 (3.290)	1.702 (1.825)	2.221 (3.280)	1.006 (1.091)
inc0p3	19	1.411 (2.083)	3.442 (2.581)	1.440 (1.442)	1.721 (2.199)	0.984 (0.967)
inc0p5	19	1.537 (2.033)	3.076 (2.469)	0.984 (1.226)	2.117 (2.236)	0.935 (0.958)
ALL	57	1.537 (2.410)	3.434 (2.780)	1.539 (1.498)	1.979 (2.572)	0.975 (1.005)
```
- Interpretation: delete turn still shows strong SEM/FC gains; insert turn is improved but smaller (PRN/FC closer to 1x).
  WMC remains ~1x overall; gains primarily come from SEM/FC.

#### 2026-01-14 run notes (full ruleset, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, P12-P20 only, no fc-profile)
- Base dir: `experiments/side_channel_inc_eval_p12_p20_nofcprofile/`.
- Cases: P12-P20 (P1-P11 skipped to avoid tiny/noisy runtimes).
- Correctness: all deltas OK (see `results-souffle-inc.tsv` under base dir).
- Purpose: recheck FC speedups without the heavy `--fc-profile` overhead.

#### 2026-01-14 run notes (reuse-var-index enabled, full ruleset, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, P12-P20 only, no fc-profile)
- Base dir: `experiments/side_channel_inc_eval_p12_p20_nofcprofile/`.
- Cases: P12-P20.
- Run args: `--det-opt` (reuse-var-index enabled by default; disable with `--no-reuse-var-index`).
- Note: CUDD does not natively support variable deletion/reuse; we enable reuse by default to reuse freed variable indices
  for deleted nodes/edges and avoid unbounded growth of the manager’s variable space.
- Note: setmode full always reconstructs the BDD manager each turn, so variable indices are not reused across turns;
  reuse only applies to incremental runs that keep a persistent manager.
- Correctness: all deltas OK (see `results-souffle-inc.tsv` under base dir).
- FC insert improvements: all 27 insert rows improved vs the earliest no-reuse run; previously slow cases
  (P13/P14/P15/P17/P18) now show FC speedups >1x. Comparison table:
  `experiments/side_channel_inc_eval_p12_p20_nofcprofile/fc-insert-speedup-reuse-compare.tsv`.

#### 2026-01-12 run notes (full ruleset, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5)
- Correctness: all P1,P3,P4-P20 OK (P2 missing in source); see `experiments/side_channel_inc_eval_small/results-souffle-inc.tsv`.
- End-to-end wins: inc faster in 15/19 for inc0p1, 15/19 for inc0p3, 15/19 for inc0p5.
- Avg speedup (end-to-end): 1.28/1.22/1.17 (inc0p1/inc0p3/inc0p5).
- Total delta-run time: sum of inc+full across all cases/deltas ≈ 9.5 min (from `results-souffle-inc.tsv`).
- Small-case overheads persist (e.g., P1 inc0p1/inc0p3/inc0p5 slower than full).

#### 2026-01-11 run notes (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5)
- Correctness: all P1,P3,P4-P20 OK (P2 missing in source); see `experiments/side_channel_inc_trimmed_eval_small/results-souffle-inc.tsv`.
- End-to-end wins: inc faster in 15/19 (inc0p1), 16/19 (inc0p3), 14/19 (inc0p5).
- SEM workload proxy: Avg ΔE/|E| = 9.8/24.7/33.9% (ins) and 10.9/32.7/51.2% (del) for 0.1/0.3/0.5%.
- SEM speedup (avg): 4.74/3.71/3.55 (ins) and 4.47/3.35/2.48 (del) for 0.1/0.3/0.5%.
- Total delta-run time: sum of inc+full across all cases/deltas ≈ 10.3 min (from `per-case-runtime-summary.tsv`).
- Small-case SEM anomalies: P4/P5/P6/P7/P9 show inc slower than full in SEM for some deltas; likely fixed overheads (applyDelta bookkeeping, logging, tiny runtime noise) dominate. This is plausibly engineering overhead rather than algorithmic work.
- TODO: reduce fixed overhead in inc SEM for very small cases; insertion should be strictly lower work than full when delta is tiny, so we should aim to make inc faster even at small scales (low priority).
- Note: for 0.1% input deltas, some cases still show ΔE/|E| ≈ 30%+, indicating dense derivation interactions even under tiny input changes (useful for characterizing network sensitivity).

### End-to-end wall time (inc vs full)
Speedup = FullTime / IncTime (>1.0 means inc faster).
#### 2026-01-12 (full ruleset, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5)
Run: 2026-01-12, full ruleset, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_eval_small.
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc0p1	0.042899	0.033619	0.784	1
P1	inc0p3	0.041344	0.028463	0.688	1
P1	inc0p5	0.040129	0.026489	0.660	1
P3	inc0p1	0.018466	0.029382	1.591	1
P3	inc0p3	0.019826	0.029007	1.463	1
P3	inc0p5	0.017766	0.029328	1.651	1
P4	inc0p1	0.013757	0.015915	1.157	1
P4	inc0p3	0.013818	0.018020	1.304	1
P4	inc0p5	0.014983	0.015101	1.008	1
P5	inc0p1	0.016296	0.016929	1.039	1
P5	inc0p3	0.020293	0.015727	0.775	1
P5	inc0p5	0.017483	0.016596	0.949	1
P6	inc0p1	0.023806	0.021884	0.919	1
P6	inc0p3	0.023422	0.019675	0.840	1
P6	inc0p5	0.021260	0.021835	1.027	1
P7	inc0p1	0.027216	0.027934	1.026	1
P7	inc0p3	0.024882	0.026946	1.083	1
P7	inc0p5	0.027564	0.029162	1.058	1
P8	inc0p1	0.028370	0.038650	1.362	1
P8	inc0p3	0.032353	0.035046	1.083	1
P8	inc0p5	0.028386	0.036604	1.290	1
P9	inc0p1	0.028358	0.032846	1.158	1
P9	inc0p3	0.029802	0.033661	1.129	1
P9	inc0p5	0.034823	0.033740	0.969	1
P10	inc0p1	0.083247	0.117125	1.407	1
P10	inc0p3	0.095453	0.132324	1.386	1
P10	inc0p5	0.121340	0.130274	1.074	1
P11	inc0p1	0.073135	0.117683	1.609	1
P11	inc0p3	0.080932	0.121896	1.506	1
P11	inc0p5	0.107690	0.141504	1.314	1
P12	inc0p1	0.258476	0.244762	0.947	1
P12	inc0p3	0.225484	0.265559	1.178	1
P12	inc0p5	0.259122	0.273533	1.056	1
P13	inc0p1	1.522332	1.267542	0.833	1
P13	inc0p3	1.478988	1.362523	0.921	1
P13	inc0p5	1.473755	1.403295	0.952	1
P14	inc0p1	2.307727	2.621900	1.136	1
P14	inc0p3	2.304292	2.748651	1.193	1
P14	inc0p5	2.257188	2.857119	1.266	1
P15	inc0p1	5.145898	8.729962	1.696	1
P15	inc0p3	5.177864	9.121451	1.762	1
P15	inc0p5	5.461147	7.523133	1.378	1
P16	inc0p1	8.410716	16.115973	1.916	1
P16	inc0p3	10.706071	14.303890	1.336	1
P16	inc0p5	10.878710	15.125275	1.390	1
P17	inc0p1	15.367531	19.115634	1.244	1
P17	inc0p3	15.299272	18.356729	1.200	1
P17	inc0p5	16.255446	18.713376	1.151	1
P18	inc0p1	13.854728	21.760777	1.571	1
P18	inc0p3	13.658217	20.727942	1.518	1
P18	inc0p5	14.306483	21.464643	1.500	1
P19	inc0p1	16.655813	26.738287	1.605	1
P19	inc0p3	17.692098	25.783089	1.457	1
P19	inc0p5	18.762402	25.926468	1.382	1
P20	inc0p1	11.782686	15.371419	1.305	1
P20	inc0p3	12.201853	15.349571	1.258	1
P20	inc0p5	12.402981	15.380011	1.240	1
```
#### 2026-01-11 (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_trimmed_eval_small.
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc0p1	0.020318	0.033605	1.654	1
P1	inc0p3	0.020449	0.029070	1.422	1
P1	inc0p5	0.022706	0.029715	1.309	1
P3	inc0p1	0.018827	0.029582	1.571	1
P3	inc0p3	0.019552	0.031180	1.595	1
P3	inc0p5	0.020197	0.029833	1.477	1
P4	inc0p1	0.014072	0.017787	1.264	1
P4	inc0p3	0.012907	0.015358	1.190	1
P4	inc0p5	0.013097	0.015567	1.189	1
P5	inc0p1	0.018084	0.014479	0.801	1
P5	inc0p3	0.016854	0.017161	1.018	1
P5	inc0p5	0.016811	0.017935	1.067	1
P6	inc0p1	0.018545	0.019976	1.077	1
P6	inc0p3	0.025645	0.021738	0.848	1
P6	inc0p5	0.019686	0.019058	0.968	1
P7	inc0p1	0.021382	0.021235	0.993	1
P7	inc0p3	0.022101	0.027373	1.239	1
P7	inc0p5	0.022145	0.026492	1.196	1
P8	inc0p1	0.023612	0.029662	1.256	1
P8	inc0p3	0.022675	0.029965	1.322	1
P8	inc0p5	0.026518	0.033781	1.274	1
P9	inc0p1	0.024759	0.030862	1.246	1
P9	inc0p3	0.025347	0.030787	1.215	1
P9	inc0p5	0.031550	0.030192	0.957	1
P10	inc0p1	0.061523	0.086442	1.405	1
P10	inc0p3	0.070595	0.089794	1.272	1
P10	inc0p5	0.087539	0.096636	1.104	1
P11	inc0p1	0.061812	0.083741	1.355	1
P11	inc0p3	0.067250	0.082829	1.232	1
P11	inc0p5	0.086138	0.108652	1.261	1
P12	inc0p1	0.226723	0.176796	0.780	1
P12	inc0p3	0.185996	0.180181	0.969	1
P12	inc0p5	0.191760	0.197595	1.030	1
P13	inc0p1	1.552602	1.238655	0.798	1
P13	inc0p3	1.373745	1.279263	0.931	1
P13	inc0p5	1.368756	1.266652	0.925	1
P14	inc0p1	2.244685	2.617350	1.166	1
P14	inc0p3	2.411816	2.660230	1.103	1
P14	inc0p5	2.250168	2.710688	1.205	1
P15	inc0p1	4.623407	9.424624	2.038	1
P15	inc0p3	5.695245	8.069352	1.417	1
P15	inc0p5	5.782716	7.668204	1.326	1
P16	inc0p1	8.296726	15.439653	1.861	1
P16	inc0p3	12.185284	13.864005	1.138	1
P16	inc0p5	12.394709	14.701200	1.186	1
P17	inc0p1	18.143024	19.752326	1.089	1
P17	inc0p3	18.134930	19.378225	1.069	1
P17	inc0p5	19.975100	19.165821	0.959	1
P18	inc0p1	15.476658	23.190252	1.498	1
P18	inc0p3	18.565776	21.619346	1.164	1
P18	inc0p5	19.582045	19.830833	1.013	1
P19	inc0p1	21.501694	29.577780	1.376	1
P19	inc0p3	25.684923	26.230354	1.021	1
P19	inc0p5	27.311604	26.849092	0.983	1
P20	inc0p1	14.422260	14.889377	1.032	1
P20	inc0p3	12.169796	14.474115	1.189	1
P20	inc0p5	13.316359	15.898888	1.194	1
```
#### 2026-01-11 (full ruleset, det-opt, apply_delta_graph)
Run: 2026-01-11, full ruleset, det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc1	0.028178	0.033582	1.192	1
P1	inc3	0.043144	0.035740	0.828	1
P1	inc5	0.053348	0.033148	0.621	1
P3	inc1	0.042752	0.027407	0.641	1
P3	inc3	0.042514	0.027267	0.641	1
P3	inc5	0.043842	0.030105	0.687	1
P4	inc1	0.018551	0.016141	0.870	1
P4	inc3	0.021788	0.018486	0.848	1
P4	inc5	0.016210	0.020464	1.262	1
P5	inc1	0.016542	0.016584	1.002	1
P5	inc3	0.014902	0.017951	1.205	1
P5	inc5	0.019005	0.017609	0.927	1
P6	inc1	0.027713	0.027013	0.975	1
P6	inc3	0.033642	0.026583	0.790	1
P6	inc5	0.029518	0.024389	0.826	1
P7	inc1	0.039608	0.032672	0.825	1
P7	inc3	0.041087	0.039764	0.968	1
P7	inc5	0.048850	0.045021	0.922	1
P8	inc1	0.047085	0.044502	0.945	1
P8	inc3	0.046412	0.059150	1.274	1
P8	inc5	0.069400	0.061949	0.893	1
P9	inc1	0.045243	0.038667	0.855	1
P9	inc3	0.059417	0.045368	0.764	1
P9	inc5	0.063161	0.049588	0.785	1
P10	inc1	0.170394	0.169448	0.994	1
P10	inc3	0.265261	0.230197	0.868	1
P10	inc5	0.348250	0.312959	0.899	1
P11	inc1	0.131460	0.145658	1.108	1
P11	inc3	0.227782	0.241238	1.059	1
P11	inc5	0.307969	0.311062	1.010	1
P12	inc1	0.489833	0.309209	0.631	1
P12	inc3	0.481141	0.373662	0.777	1
P12	inc5	0.698120	0.438985	0.629	1
P13	inc1	1.592549	1.527124	0.959	1
P13	inc3	1.848791	1.238764	0.670	1
P13	inc5	1.762857	1.374947	0.780	1
P14	inc1	2.457870	2.797754	1.138	1
P14	inc3	2.492091	2.414316	0.969	1
P14	inc5	3.988358	2.755228	0.691	1
P15	inc1	6.498690	8.499480	1.308	1
P15	inc3	8.319964	7.157752	0.860	1
P15	inc5	11.160457	8.347403	0.748	1
P16	inc1	15.557071	14.150498	0.910	1
P16	inc3	18.827648	13.240545	0.703	1
P16	inc5	20.065891	14.214730	0.708	1
P17	inc1	36.648351	17.928072	0.489	1
P17	inc3	29.203291	17.025071	0.583	1
P17	inc5	29.861553	18.232292	0.611	1
P18	inc1	24.201893	20.268406	0.837	1
P18	inc3	29.540257	19.455472	0.659	1
P18	inc5	31.654243	20.964235	0.662	1
P19	inc1	38.998415	26.669192	0.684	1
P19	inc3	44.134623	26.614771	0.603	1
P19	inc5	46.978561	28.079509	0.598	1
P20	inc1	16.206111	18.035626	1.113	1
P20	inc3	65.678594	23.662061	0.360	1
P20	inc5	98.144037	23.689803	0.241	1
```

Legacy tables below are pre apply_delta_graph and kept for audit only.
#### 2026-01-11 (full ruleset, det-opt)
Legacy run: 2026-01-11, full ruleset, det-opt, pre apply_delta_graph; ΔE/|E| uses pruned view edges (do not compare).
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc1	0.024590	0.028496	1.159	1
P1	inc3	0.025747	0.031953	1.241	1
P1	inc5	0.031007	0.029204	0.942	1
P3	inc1	0.042083	0.027340	0.650	1
P3	inc3	0.030971	0.026801	0.865	1
P3	inc5	0.031071	0.025746	0.829	1
P4	inc1	0.021447	0.016012	0.747	1
P4	inc3	0.015920	0.014436	0.907	1
P4	inc5	0.016310	0.017382	1.066	1
P5	inc1	0.027326	0.015328	0.561	1
P5	inc3	0.016283	0.015693	0.964	1
P5	inc5	0.022869	0.016411	0.718	1
P6	inc1	0.041006	0.026507	0.646	1
P6	inc3	0.041995	0.024159	0.575	1
P6	inc5	0.031903	0.027147	0.851	1
P7	inc1	0.055094	0.033358	0.605	0
P7	inc3	0.041475	0.039726	0.958	0
P7	inc5	0.038732	0.040661	1.050	0
P8	inc1	0.070456	0.040190	0.570	1
P8	inc3	0.055275	0.046884	0.848	0
P8	inc5	0.053569	0.054194	1.012	0
P9	inc1	0.071813	0.037202	0.518	1
P9	inc3	0.055736	0.043502	0.781	1
P9	inc5	0.067330	0.048526	0.721	0
P10	inc1	0.164333	0.157620	0.959	1
P10	inc3	0.248252	0.250650	1.010	1
P10	inc5	0.325507	0.319812	0.983	1
P11	inc1	0.144309	0.157171	1.089	1
P11	inc3	0.253178	0.281968	1.114	1
P11	inc5	0.361273	0.315088	0.872	1
P12	inc1	0.450488	0.309551	0.687	0
P12	inc3	0.462808	0.387834	0.838	0
P12	inc5	0.586862	0.510967	0.871	0
P13	inc1	1.787361	1.597985	0.894	1
P13	inc3	1.594393	1.322037	0.829	0
P13	inc5	2.104543	1.528060	0.726	0
P14	inc1	2.634469	2.647659	1.005	1
P14	inc3	2.544650	2.390040	0.939	1
P14	inc5	3.678997	2.620160	0.712	1
P15	inc1	6.970433	7.914148	1.135	1
P15	inc3	8.642125	7.728032	0.894	1
P15	inc5	10.741151	8.976752	0.836	1
P16	inc1	15.073081	13.926551	0.924	0
P16	inc3	18.768468	13.155517	0.701	0
P16	inc5	18.257157	14.322621	0.784	0
P17	inc1	34.566305	18.888610	0.546	1
P17	inc3	32.701322	17.784819	0.544	1
P17	inc5	34.999886	19.005227	0.543	0
P18	inc1	35.110283	21.676021	0.617	1
P18	inc3	39.337119	21.577558	0.549	1
P18	inc5	45.919875	23.931545	0.521	0
P19	inc1	46.492906	27.609727	0.594	0
P19	inc3	58.440121	30.306610	0.519	0
P19	inc5	51.975533	27.879079	0.536	0
P20	inc1	15.954860	17.810268	1.116	0
P20	inc3	76.357024	26.046637	0.341	0
P20	inc5	116.144302	25.061241	0.216	0
```

#### 2026-01-11 per-case runtime totals (inc+full, 0.1/0.3/0.5% deltas, apply_delta_graph, trimmed ruleset)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, sample=1, base-dir experiments/side_channel_inc_trimmed_eval_small.
```tsv
Case	IncTotal_s	FullTotal_s	IncPlusFull_s
P1	0.063	0.092	0.156
P3	0.059	0.091	0.149
P4	0.040	0.049	0.089
P5	0.052	0.050	0.101
P6	0.064	0.061	0.125
P7	0.066	0.075	0.141
P8	0.073	0.093	0.166
P9	0.082	0.092	0.173
P10	0.220	0.273	0.493
P11	0.215	0.275	0.490
P12	0.604	0.555	1.159
P13	4.295	3.785	8.080
P14	6.907	7.988	14.895
P15	16.101	25.162	41.264
P16	32.877	44.005	76.882
P17	56.253	58.296	114.549
P18	53.624	64.640	118.265
P19	74.498	82.657	157.155
P20	39.908	45.262	85.171
```

#### 2026-01-11 per-case runtime totals (inc+full, 3 deltas, apply_delta_graph)
Run: 2026-01-11, full ruleset, det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
```tsv
Case	IncTotal_s	FullTotal_s	IncPlusFull_s
P1	0.125	0.102	0.227
P3	0.129	0.085	0.214
P4	0.057	0.055	0.112
P5	0.050	0.052	0.103
P6	0.091	0.078	0.169
P7	0.130	0.117	0.247
P8	0.163	0.166	0.328
P9	0.168	0.134	0.301
P10	0.784	0.713	1.497
P11	0.667	0.698	1.365
P12	1.669	1.122	2.791
P13	5.204	4.141	9.345
P14	8.938	7.967	16.906
P15	25.979	24.005	49.984
P16	54.451	41.606	96.056
P17	95.713	53.185	148.899
P18	85.396	60.688	146.085
P19	130.112	81.363	211.475
P20	180.029	65.387	245.416
```

#### 2026-01-11 per-case runtime totals (inc+full, 3 deltas, apply_delta_graph, trimmed ruleset)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_trimmed_eval.
```tsv
Case	IncTotal_s	FullTotal_s	IncPlusFull_s
P1	0.096	0.091	0.187
P3	0.140	0.098	0.238
P4	0.054	0.050	0.103
P5	0.057	0.058	0.115
P6	0.069	0.071	0.140
P7	0.099	0.108	0.206
P8	0.128	0.121	0.249
P9	0.130	0.114	0.245
P10	0.586	0.583	1.169
P11	0.631	0.656	1.286
P12	1.375	0.922	2.298
P13	5.046	3.711	8.757
P14	7.559	7.230	14.789
P15	25.554	25.861	51.415
P16	54.908	41.375	96.283
P17	87.674	53.639	141.313
P18	92.079	62.225	154.304
P19	137.702	79.897	217.599
P20	178.937	76.835	255.772
```

Legacy per-case totals below are pre apply_delta_graph (kept for audit only).
#### 2026-01-11 per-case runtime totals (inc+full, 3 deltas)
Legacy run: 2026-01-11, full ruleset, det-opt, pre apply_delta_graph.
```tsv
Case	IncTotal_s	FullTotal_s	IncPlusFull_s
P1	0.081	0.090	0.171
P3	0.104	0.080	0.184
P4	0.054	0.048	0.102
P5	0.066	0.047	0.114
P6	0.115	0.078	0.193
P7	0.135	0.114	0.249
P8	0.179	0.141	0.321
P9	0.195	0.129	0.324
P10	0.738	0.728	1.466
P11	0.759	0.754	1.513
P12	1.500	1.208	2.709
P13	5.486	4.448	9.934
P14	8.858	7.658	16.516
P15	26.354	24.619	50.973
P16	52.099	41.405	93.503
P17	102.268	55.679	157.946
P18	120.367	67.185	187.552
P19	156.909	85.795	242.704
P20	208.456	68.918	277.374
```

Correctness (det-opt, apply_delta_graph): all P1,P3,P4-P20 OK (P2 missing in source).
Correctness (trimmed, det-opt, apply_delta_graph): all P1,P3,P4-P20 OK (P2 missing in source).
Correctness (det-opt, legacy run): mismatches in P7 (inc1/inc3/inc5), P8 (inc3/inc5), P9 (inc5), P12 (inc1/inc3/inc5), P13 (inc3/inc5), P16 (inc1/inc3/inc5), P17 (inc5), P18 (inc5), P19 (inc1/inc3/inc5), P20 (inc1/inc3/inc5).

#### 2026-01-11 (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_trimmed_eval_small.
```tsv
Case	Delta	Turn	Full_SEM_s	Inc_SEM_s	Speedup	DeltaRuleApps	GraphEdges	DeltaEdgeRatio
P1	inc0p1	del	0.004462	0.001337	3.337	0	58	0.000000
P1	inc0p1	ins	0.004459	0.000877	5.083	0	58	0.000000
P1	inc0p3	del	0.004789	0.001294	3.702	0	58	0.000000
P1	inc0p3	ins	0.004914	0.001761	2.790	0	58	0.000000
P1	inc0p5	del	0.004895	0.001850	2.646	0	58	0.000000
P1	inc0p5	ins	0.004703	0.002201	2.137	0	58	0.000000
P3	inc0p1	del	0.004446	0.000877	5.071	0	58	0.000000
P3	inc0p1	ins	0.004410	0.000889	4.961	0	58	0.000000
P3	inc0p3	del	0.004418	0.001602	2.758	0	58	0.000000
P3	inc0p3	ins	0.004350	0.000746	5.833	0	58	0.000000
P3	inc0p5	del	0.004419	0.000926	4.773	0	58	0.000000
P3	inc0p5	ins	0.004451	0.000925	4.811	0	58	0.000000
P4	inc0p1	del	0.000266	0.000909	0.293	0	19	0.000000
P4	inc0p1	ins	0.000301	0.001761	0.171	0	19	0.000000
P4	inc0p3	del	0.000248	0.000792	0.313	0	19	0.000000
P4	inc0p3	ins	0.000236	0.001131	0.208	0	19	0.000000
P4	inc0p5	del	0.000253	0.002054	0.123	0	19	0.000000
P4	inc0p5	ins	0.000229	0.000441	0.518	0	19	0.000000
P5	inc0p1	del	0.000268	0.001177	0.228	15	18	0.833333
P5	inc0p1	ins	0.000318	0.001045	0.304	15	33	0.454545
P5	inc0p3	del	0.000242	0.000621	0.390	15	18	0.833333
P5	inc0p3	ins	0.000304	0.002492	0.122	15	33	0.454545
P5	inc0p5	del	0.000253	0.000803	0.315	15	18	0.833333
P5	inc0p5	ins	0.000318	0.002410	0.132	15	33	0.454545
P6	inc0p1	del	0.000451	0.001169	0.386	1	60	0.016667
P6	inc0p1	ins	0.000409	0.001046	0.391	1	61	0.016393
P6	inc0p3	del	0.000465	0.002456	0.189	1	60	0.016667
P6	inc0p3	ins	0.000420	0.002647	0.159	1	61	0.016393
P6	inc0p5	del	0.000468	0.001437	0.326	1	60	0.016667
P6	inc0p5	ins	0.000529	0.002191	0.241	1	61	0.016393
P7	inc0p1	del	0.001030	0.001997	0.516	4	167	0.023952
P7	inc0p1	ins	0.000996	0.001919	0.519	4	171	0.023392
P7	inc0p3	del	0.001064	0.002429	0.438	4	167	0.023952
P7	inc0p3	ins	0.001063	0.001320	0.805	4	171	0.023392
P7	inc0p5	del	0.001088	0.001371	0.794	36	135	0.266667
P7	inc0p5	ins	0.001249	0.001967	0.635	36	171	0.210526
P8	inc0p1	del	0.001715	0.001457	1.177	0	287	0.000000
P8	inc0p1	ins	0.001491	0.001040	1.433	0	287	0.000000
P8	inc0p3	del	0.001634	0.001795	0.910	0	287	0.000000
P8	inc0p3	ins	0.001525	0.001229	1.242	0	287	0.000000
P8	inc0p5	del	0.001611	0.002366	0.681	15	272	0.055147
P8	inc0p5	ins	0.001529	0.001306	1.171	15	287	0.052265
P9	inc0p1	del	0.001257	0.001286	0.978	7	202	0.034653
P9	inc0p1	ins	0.001407	0.001156	1.217	7	209	0.033493
P9	inc0p3	del	0.001222	0.002341	0.522	7	202	0.034653
P9	inc0p3	ins	0.001107	0.001924	0.575	7	209	0.033493
P9	inc0p5	del	0.001198	0.001289	0.929	10	199	0.050251
P9	inc0p5	ins	0.001103	0.002088	0.529	10	209	0.047847
P10	inc0p1	del	0.017382	0.003102	5.603	143	2188	0.065356
P10	inc0p1	ins	0.016505	0.008052	2.050	143	2331	0.061347
P10	inc0p3	del	0.014270	0.004797	2.975	494	1837	0.268917
P10	inc0p3	ins	0.016878	0.006660	2.534	494	2331	0.211926
P10	inc0p5	del	0.013707	0.006738	2.034	710	1621	0.438001
P10	inc0p5	ins	0.016625	0.005310	3.131	710	2331	0.304590
P11	inc0p1	del	0.014945	0.009417	1.587	315	2003	0.157264
P11	inc0p1	ins	0.016825	0.003209	5.244	315	2318	0.135893
P11	inc0p3	del	0.013123	0.004124	3.182	559	1759	0.317794
P11	inc0p3	ins	0.015848	0.006339	2.500	559	2318	0.241156
P11	inc0p5	del	0.014515	0.005800	2.503	1082	1236	0.875405
P11	inc0p5	ins	0.019501	0.005728	3.405	1082	2318	0.466782
P12	inc0p1	del	0.014929	0.003384	4.411	65	2250	0.028889
P12	inc0p1	ins	0.012602	0.003251	3.876	65	2315	0.028078
P12	inc0p3	del	0.013814	0.003490	3.958	111	2204	0.050363
P12	inc0p3	ins	0.013780	0.003236	4.258	111	2315	0.047948
P12	inc0p5	del	0.013045	0.003690	3.535	153	2162	0.070768
P12	inc0p5	ins	0.013609	0.002218	6.136	153	2315	0.066091
P13	inc0p1	del	0.029434	0.004762	6.182	113	4417	0.025583
P13	inc0p1	ins	0.024921	0.004537	5.493	113	4530	0.024945
P13	inc0p3	del	0.026197	0.004828	5.427	261	4269	0.061138
P13	inc0p3	ins	0.028873	0.005028	5.742	261	4530	0.057616
P13	inc0p5	del	0.021999	0.005810	3.786	579	3951	0.146545
P13	inc0p5	ins	0.027698	0.005834	4.748	579	4530	0.127815
P14	inc0p1	del	0.040013	0.004783	8.366	168	6577	0.025544
P14	inc0p1	ins	0.044723	0.004882	9.161	168	6745	0.024907
P14	inc0p3	del	0.040072	0.005703	7.026	419	6326	0.066235
P14	inc0p3	ins	0.041167	0.005614	7.332	419	6745	0.062120
P14	inc0p5	del	0.041958	0.007684	5.460	719	6026	0.119316
P14	inc0p5	ins	0.042660	0.007388	5.775	719	6745	0.106597
P15	inc0p1	del	0.108737	0.018486	5.882	887	13211	0.067141
P15	inc0p1	ins	0.110210	0.034138	3.228	887	14098	0.062917
P15	inc0p3	del	0.095775	0.028815	3.324	2460	11638	0.211377
P15	inc0p3	ins	0.111738	0.025520	4.379	2460	14098	0.174493
P15	inc0p5	del	0.075237	0.034535	2.179	3027	11071	0.273417
P15	inc0p5	ins	0.097734	0.027915	3.501	3027	14098	0.214711
P16	inc0p1	del	0.163779	0.030880	5.304	1968	22505	0.087447
P16	inc0p1	ins	0.196405	0.037250	5.273	1968	24473	0.080415
P16	inc0p3	del	0.162215	0.060967	2.661	6200	18273	0.339298
P16	inc0p3	ins	0.201602	0.065006	3.101	6200	24473	0.253340
P16	inc0p5	del	0.137573	0.059488	2.313	9479	14994	0.632186
P16	inc0p5	ins	0.181818	0.059098	3.077	9479	24473	0.387325
P17	inc0p1	del	0.281173	0.049873	5.638	3415	29922	0.114130
P17	inc0p1	ins	0.327089	0.064152	5.099	3415	33337	0.102439
P17	inc0p3	del	0.237279	0.062241	3.812	7415	25922	0.286050
P17	inc0p3	ins	0.328813	0.067742	4.854	7415	33337	0.222426
P17	inc0p5	del	0.206403	0.085284	2.420	11290	22047	0.512088
P17	inc0p5	ins	0.325402	0.080864	4.024	11290	33337	0.338663
P18	inc0p1	del	0.359826	0.075882	4.742	5321	36876	0.144294
P18	inc0p1	ins	0.396444	0.078139	5.074	5321	42197	0.126099
P18	inc0p3	del	0.317769	0.112652	2.821	15504	26693	0.580826
P18	inc0p3	ins	0.379968	0.123031	3.088	15504	42197	0.367419
P18	inc0p5	del	0.272955	0.127900	2.134	20508	21689	0.945548
P18	inc0p5	ins	0.392006	0.131476	2.982	20508	42197	0.486006
P19	inc0p1	del	0.458766	0.140386	3.268	9357	43923	0.213032
P19	inc0p1	ins	0.567848	0.162099	3.503	9357	53280	0.175619
P19	inc0p3	del	0.394764	0.162682	2.427	21022	32258	0.651683
P19	inc0p3	ins	0.507724	0.178437	2.845	21022	53280	0.394557
P19	inc0p5	del	0.362572	0.205365	1.766	26094	27186	0.959832
P19	inc0p5	ins	0.527415	0.177267	2.975	26094	53280	0.489752
P20	inc0p1	del	0.445935	0.084684	5.266	842	43488	0.019362
P20	inc0p1	ins	0.522146	0.064647	8.077	842	44330	0.018994
P20	inc0p3	del	0.471718	0.073470	6.421	2437	41893	0.058172
P20	inc0p3	ins	0.457180	0.070169	6.515	2437	44330	0.054974
P20	inc0p5	del	0.391553	0.075693	5.173	4486	39844	0.112589
P20	inc0p5	ins	0.464711	0.081904	5.674	4486	44330	0.101196
```

#### 2026-01-11 (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_trimmed_eval.
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc1	0.028174	0.031133	1.105	1
P1	inc3	0.020168	0.030031	1.489	1
P1	inc5	0.047576	0.029866	0.628	1
P3	inc1	0.049642	0.034214	0.689	1
P3	inc3	0.043860	0.030722	0.700	1
P3	inc5	0.046448	0.033480	0.721	1
P4	inc1	0.020769	0.015718	0.757	1
P4	inc3	0.015630	0.016476	1.054	1
P4	inc5	0.017387	0.017342	0.997	1
P5	inc1	0.020434	0.018205	0.891	1
P5	inc3	0.016775	0.019649	1.171	1
P5	inc5	0.019417	0.020247	1.043	1
P6	inc1	0.020088	0.019407	0.966	1
P6	inc3	0.023222	0.026678	1.149	1
P6	inc5	0.025284	0.025121	0.994	1
P7	inc1	0.023944	0.025257	1.055	1
P7	inc3	0.033048	0.041928	1.269	1
P7	inc5	0.041658	0.040398	0.970	1
P8	inc1	0.029692	0.034018	1.146	1
P8	inc3	0.042743	0.040157	0.939	1
P8	inc5	0.055685	0.046510	0.835	1
P9	inc1	0.035904	0.033638	0.937	1
P9	inc3	0.045508	0.037518	0.824	1
P9	inc5	0.049045	0.043165	0.880	1
P10	inc1	0.099667	0.113593	1.140	1
P10	inc3	0.196394	0.189252	0.964	1
P10	inc5	0.289605	0.280547	0.969	1
P11	inc1	0.110692	0.131738	1.190	1
P11	inc3	0.202375	0.218438	1.079	1
P11	inc5	0.317574	0.305413	0.962	1
P12	inc1	0.300770	0.210064	0.698	1
P12	inc3	0.578522	0.311286	0.538	1
P12	inc5	0.496008	0.400971	0.808	1
P13	inc1	1.756376	1.154963	0.658	1
P13	inc3	1.610216	1.193499	0.741	1
P13	inc5	1.679541	1.362420	0.811	1
P14	inc1	2.220252	2.469873	1.112	1
P14	inc3	2.477329	2.289784	0.924	1
P14	inc5	2.861612	2.470252	0.863	1
P15	inc1	7.437080	9.243480	1.243	1
P15	inc3	8.322846	8.095021	0.973	1
P15	inc5	9.794030	8.522862	0.870	1
P16	inc1	15.693304	14.205796	0.905	1
P16	inc3	18.439533	13.233052	0.718	1
P16	inc5	20.775026	13.935862	0.671	1
P17	inc1	26.658874	18.140791	0.680	1
P17	inc3	30.111198	17.358775	0.576	1
P17	inc5	30.903680	18.139282	0.587	1
P18	inc1	26.021528	20.945713	0.805	1
P18	inc3	33.110626	20.492151	0.619	1
P18	inc5	32.946639	20.787343	0.631	1
P19	inc1	38.554325	27.160667	0.704	1
P19	inc3	53.219256	24.804331	0.466	1
P19	inc5	45.928423	27.931835	0.608	1
P20	inc1	22.254613	20.504406	0.921	1
P20	inc3	79.235345	26.239887	0.331	1
P20	inc5	77.446699	30.090553	0.389	1
```

#### 2026-01-10 (trimmed ruleset, no equal_assign)
Legacy run: 2026-01-10, trimmed ruleset (no equal_assign), inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_trimmed_eval.
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc1	0.337566	1.286437	3.811	1
P1	inc3	0.370647	1.028006	2.774	1
P1	inc5	1.236234	0.674332	0.545	1
P3	inc1	1.162363	0.737374	0.634	1
P3	inc3	1.104880	0.732703	0.663	1
P3	inc5	1.189762	0.726775	0.611	1
P4	inc1	0.015281	0.013889	0.909	1
P4	inc3	0.018728	0.015583	0.832	1
P4	inc5	0.015677	0.014013	0.894	1
P5	inc1	0.016249	0.015899	0.978	1
P5	inc3	0.014509	0.020673	1.425	1
P5	inc5	0.017839	0.015902	0.891	1
P6	inc1	0.017493	0.018616	1.064	1
P6	inc3	0.021958	0.022397	1.020	1
P6	inc5	0.023372	0.022702	0.971	1
P7	inc1	0.027244	0.026758	0.982	1
P7	inc3	0.031928	0.033618	1.053	1
P7	inc5	0.039772	0.037617	0.946	1
P8	inc1	0.033662	0.034403	1.022	1
P8	inc3	0.040625	0.038162	0.939	1
P8	inc5	0.047059	0.046882	0.996	1
P9	inc1	0.033846	0.033542	0.991	1
P9	inc3	0.041368	0.041887	1.013	1
P9	inc5	0.048792	0.048052	0.985	1
P10	inc1	0.119009	0.137689	1.157	1
P10	inc3	0.192643	0.212458	1.103	1
P10	inc5	0.307846	0.284290	0.923	1
P11	inc1	0.102617	0.117096	1.141	1
P11	inc3	0.190447	0.197392	1.036	1
P11	inc5	0.269119	0.274488	1.020	1
P12	inc1	0.303927	0.251685	0.828	1
P12	inc3	0.451209	0.304003	0.674	1
P12	inc5	0.486506	0.381906	0.785	1
P13	inc1	1.539222	1.033123	0.671	1
P13	inc3	1.475342	1.123304	0.761	1
P13	inc5	1.528772	1.300920	0.851	1
P14	inc1	2.375514	2.317190	0.975	1
P14	inc3	2.644482	2.163876	0.818	1
P14	inc5	2.774842	2.387992	0.861	1
P15	inc1	6.447595	8.072398	1.252	1
P15	inc3	7.621881	7.134407	0.936	1
P15	inc5	8.958616	7.720081	0.862	1
P16	inc1	17.786523	14.867957	0.836	1
P16	inc3	17.096539	11.900136	0.696	1
P16	inc5	17.910979	13.288718	0.742	1
P17	inc1	22.690092	15.751412	0.694	1
P17	inc3	25.109208	14.663478	0.584	1
P17	inc5	25.437655	15.934298	0.626	1
P18	inc1	23.024591	17.099180	0.743	1
P18	inc3	27.333167	17.007375	0.622	1
P18	inc5	31.320941	18.466959	0.590	1
P19	inc1	36.558594	23.779726	0.650	1
P19	inc3	42.888465	23.127642	0.539	1
P19	inc5	44.262132	25.203704	0.569	1
P20	inc1	15.179235	16.208378	1.068	1
P20	inc3	24.246785	17.372865	0.717	1
P20	inc5	32.598067	23.049153	0.707	1
```

#### 2026-01-10 (full ruleset, equal_assign)
Legacy run: 2026-01-10, full ruleset (equal_assign), inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc1	0.362890	1.059420	2.919	1
P1	inc3	0.819279	0.785610	0.959	1
P1	inc5	1.096182	0.677412	0.618	1
P3	inc1	0.999544	0.715619	0.716	1
P3	inc3	0.894973	0.726914	0.812	1
P3	inc5	1.143280	0.635498	0.556	1
P4	inc1	0.014374	0.017848	1.242	1
P4	inc3	0.012436	0.015791	1.270	1
P4	inc5	0.013547	0.017240	1.273	1
P5	inc1	0.014296	0.013665	0.956	1
P5	inc3	0.012366	0.014834	1.200	1
P5	inc5	0.015192	0.014295	0.941	1
P6	inc1	0.034988	0.052611	1.504	1
P6	inc3	0.040112	0.057169	1.425	1
P6	inc5	0.039761	0.056812	1.429	1
P7	inc1	0.117489	0.152707	1.300	1
P7	inc3	0.115539	0.151226	1.309	1
P7	inc5	0.125125	0.170238	1.361	1
P8	inc1	0.282287	0.542966	1.923	1
P8	inc3	1.943589	0.455571	0.234	1
P8	inc5	1.761781	0.435605	0.247	1
P9	inc1	0.150998	0.253229	1.677	1
P9	inc3	0.155044	0.251046	1.619	1
P9	inc5	0.208411	0.258356	1.240	1
P10	inc1	0.451426	0.846413	1.875	1
P10	inc3	0.543028	0.911424	1.678	1
P10	inc5	0.644178	1.014514	1.575	1
P11	inc1	0.464874	0.936150	2.014	1
P11	inc3	0.564085	0.957755	1.698	1
P11	inc5	0.672483	1.007881	1.499	1
P12	inc1	3.479153	6.693862	1.924	1
P12	inc3	4.361903	4.824892	1.106	1
P12	inc5	6.551012	4.576573	0.699	1
P13	inc1	5.613193	10.835839	1.930	1
P13	inc3	8.159739	9.770847	1.197	1
P13	inc5	11.229486	8.569525	0.763	1
P14	inc1	9.994663	14.984015	1.499	1
P14	inc3	12.756585	13.678577	1.072	1
P14	inc5	16.270389	13.148420	0.808	1
P15	inc1	22.344087	48.208019	2.158	1
P15	inc3	28.321870	33.805121	1.194	1
P15	inc5	33.105076	32.611190	0.985	1
P16	inc1	36.696796	55.719785	1.518	1
P16	inc3	48.252403	53.903611	1.117	1
P16	inc5	56.076900	54.604068	0.974	1
P17	inc1	57.178675	82.859408	1.449	1
P17	inc3	69.888304	76.640111	1.097	1
P17	inc5	80.079526	74.580926	0.931	1
P18	inc1	71.371920	102.837910	1.441	1
P18	inc3	93.919276	98.663864	1.051	1
P18	inc5	105.858887	95.173303	0.899	1
P19	inc1	101.545516	141.899737	1.397	1
P19	inc3	141.806616	140.623205	0.992	1
P19	inc5	162.479248	130.699115	0.804	1
P20	inc1	57.374558	96.934050	1.689	1
P20	inc3	155.466635	97.076936	0.624	1
P20	inc5	174.268695	95.279086	0.547	1
```

#### 2026-01-10 (full ruleset, equal_assign)
Legacy run: 2026-01-10, full ruleset (equal_assign), inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	Turn	Stage	Full_s	Inc_s	Speedup
P1	inc1	del	SEM	0.308619	0.003080	100.193
P1	inc1	del	PRN	0.008713	0.008452	1.031
P1	inc1	del	FC	0.000479	0.000323	1.484
P1	inc1	del	WMC	0.000034	0.000032	1.075
P1	inc1	ins	SEM	0.374344	0.001686	221.972
P1	inc1	ins	PRN	0.010486	0.006082	1.724
P1	inc1	ins	FC	0.000447	0.000236	1.898
P1	inc1	ins	WMC	0.000026	0.000043	0.613
P1	inc3	del	SEM	0.163037	0.152906	1.066
P1	inc3	del	PRN	0.004526	0.119975	0.038
P1	inc3	del	FC	0.000473	0.000442	1.068
P1	inc3	del	WMC	0.000026	0.000040	0.655
P1	inc3	ins	SEM	0.271973	0.048598	5.596
P1	inc3	ins	PRN	0.005834	0.146659	0.040
P1	inc3	ins	FC	0.000448	0.000277	1.618
P1	inc3	ins	WMC	0.000025	0.000032	0.793
P1	inc5	del	SEM	0.053398	0.143623	0.372
P1	inc5	del	PRN	0.002010	0.225980	0.009
P1	inc5	del	FC	0.000433	0.000323	1.337
P1	inc5	del	WMC	0.000029	0.000034	0.833
P1	inc5	ins	SEM	0.281798	0.120375	2.341
P1	inc5	ins	PRN	0.008966	0.296032	0.030
P1	inc5	ins	FC	0.000497	0.000339	1.465
P1	inc5	ins	WMC	0.000027	0.000038	0.697
P3	inc1	del	SEM	0.104790	0.133366	0.786
P3	inc1	del	PRN	0.002907	0.192417	0.015
P3	inc1	del	FC	0.000460	0.000529	0.869
P3	inc1	del	WMC	0.000026	0.000099	0.260
P3	inc1	ins	SEM	0.278659	0.104623	2.663
P3	inc1	ins	PRN	0.006933	0.248965	0.028
P3	inc1	ins	FC	0.000566	0.000254	2.233
P3	inc1	ins	WMC	0.000027	0.000030	0.874
P3	inc3	del	SEM	0.105291	0.125322	0.840
P3	inc3	del	PRN	0.002452	0.163500	0.015
P3	inc3	del	FC	0.000311	0.000674	0.461
P3	inc3	del	WMC	0.000029	0.000028	1.038
P3	inc3	ins	SEM	0.289040	0.075171	3.845
P3	inc3	ins	PRN	0.009154	0.217284	0.042
P3	inc3	ins	FC	0.000490	0.001369	0.358
P3	inc3	ins	WMC	0.000027	0.000027	0.996
P3	inc5	del	SEM	0.030692	0.109813	0.279
P3	inc5	del	PRN	0.000981	0.261855	0.004
P3	inc5	del	FC	0.000276	0.000485	0.569
P3	inc5	del	WMC	0.000024	0.000026	0.922
P3	inc5	ins	SEM	0.275340	0.121540	2.265
P3	inc5	ins	PRN	0.007160	0.325831	0.022
P3	inc5	ins	FC	0.000468	0.001921	0.244
P3	inc5	ins	WMC	0.000026	0.000033	0.789
P4	inc1	del	SEM	0.001471	0.001209	1.216
P4	inc1	del	PRN	0.000211	0.000245	0.861
P4	inc1	del	FC	0.000115	0.000100	1.151
P4	inc1	del	WMC	0.000022	0.000114	0.196
P4	inc1	ins	SEM	0.001402	0.000723	1.938
P4	inc1	ins	PRN	0.000124	0.000130	0.950
P4	inc1	ins	FC	0.000091	0.000034	2.677
P4	inc1	ins	WMC	0.000025	0.000022	1.121
P4	inc3	del	SEM	0.001379	0.000575	2.400
P4	inc3	del	PRN	0.000149	0.000149	1.003
P4	inc3	del	FC	0.000090	0.000053	1.699
P4	inc3	del	WMC	0.000022	0.000024	0.932
P4	inc3	ins	SEM	0.001398	0.000647	2.159
P4	inc3	ins	PRN	0.000117	0.000182	0.644
P4	inc3	ins	FC	0.000088	0.000044	2.018
P4	inc3	ins	WMC	0.000022	0.000028	0.762
P4	inc5	del	SEM	0.001357	0.000545	2.489
P4	inc5	del	PRN	0.000160	0.000169	0.944
P4	inc5	del	FC	0.000088	0.000044	2.026
P4	inc5	del	WMC	0.000022	0.000028	0.783
P4	inc5	ins	SEM	0.001390	0.000679	2.047
P4	inc5	ins	PRN	0.000124	0.000133	0.933
P4	inc5	ins	FC	0.000089	0.000035	2.537
P4	inc5	ins	WMC	0.000022	0.000023	0.960
P5	inc1	del	SEM	0.000403	0.001691	0.238
P5	inc1	del	PRN	0.000425	0.000281	1.512
P5	inc1	del	FC	0.000248	0.000156	1.594
P5	inc1	del	WMC	0.000029	0.000060	0.492
P5	inc1	ins	SEM	0.000450	0.001123	0.401
P5	inc1	ins	PRN	0.000263	0.000269	0.978
P5	inc1	ins	FC	0.000208	0.000092	2.253
P5	inc1	ins	WMC	0.000025	0.000028	0.891
P5	inc3	del	SEM	0.000307	0.000851	0.361
P5	inc3	del	PRN	0.000281	0.000256	1.096
P5	inc3	del	FC	0.000232	0.000092	2.520
P5	inc3	del	WMC	0.000024	0.000029	0.826
P5	inc3	ins	SEM	0.000341	0.000675	0.505
P5	inc3	ins	PRN	0.000276	0.000252	1.099
P5	inc3	ins	FC	0.000198	0.000090	2.192
P5	inc3	ins	WMC	0.000024	0.000027	0.880
P5	inc5	del	SEM	0.000230	0.000833	0.276
P5	inc5	del	PRN	0.000112	0.000285	0.392
P5	inc5	del	FC	0.000088	0.001140	0.077
P5	inc5	del	WMC	0.000022	0.000028	0.785
P5	inc5	ins	SEM	0.000339	0.000823	0.412
P5	inc5	ins	PRN	0.000283	0.000474	0.598
P5	inc5	ins	FC	0.000214	0.000316	0.679
P5	inc5	ins	WMC	0.000025	0.000024	1.007
P6	inc1	del	SEM	0.006450	0.002067	3.121
P6	inc1	del	PRN	0.003381	0.001733	1.950
P6	inc1	del	FC	0.002041	0.001138	1.793
P6	inc1	del	WMC	0.000032	0.000074	0.430
P6	inc1	ins	SEM	0.005883	0.001593	3.694
P6	inc1	ins	PRN	0.003164	0.001666	1.899
P6	inc1	ins	FC	0.001962	0.001397	1.405
P6	inc1	ins	WMC	0.000031	0.000073	0.424
P6	inc3	del	SEM	0.006460	0.002108	3.064
P6	inc3	del	PRN	0.003346	0.001631	2.051
P6	inc3	del	FC	0.001907	0.001868	1.021
P6	inc3	del	WMC	0.000042	0.000032	1.314
P6	inc3	ins	SEM	0.007524	0.001430	5.261
P6	inc3	ins	PRN	0.003759	0.002218	1.694
P6	inc3	ins	FC	0.002393	0.001615	1.482
P6	inc3	ins	WMC	0.000032	0.000057	0.566
P6	inc5	del	SEM	0.006187	0.001707	3.625
P6	inc5	del	PRN	0.003338	0.001783	1.872
P6	inc5	del	FC	0.001922	0.001886	1.019
P6	inc5	del	WMC	0.000029	0.000044	0.668
P6	inc5	ins	SEM	0.006507	0.001423	4.572
P6	inc5	ins	PRN	0.003489	0.002400	1.454
P6	inc5	ins	FC	0.002291	0.001893	1.210
P6	inc5	ins	WMC	0.000032	0.000037	0.858
P7	inc1	del	SEM	0.024047	0.006067	3.963
P7	inc1	del	PRN	0.010151	0.006224	1.631
P7	inc1	del	FC	0.006508	0.005233	1.244
P7	inc1	del	WMC	0.000046	0.000070	0.655
P7	inc1	ins	SEM	0.022083	0.002191	10.077
P7	inc1	ins	PRN	0.009546	0.013914	0.686
P7	inc1	ins	FC	0.006353	0.007038	0.903
P7	inc1	ins	WMC	0.000044	0.000039	1.149
P7	inc3	del	SEM	0.022926	0.005189	4.418
P7	inc3	del	PRN	0.008672	0.006023	1.440
P7	inc3	del	FC	0.006070	0.005470	1.110
P7	inc3	del	WMC	0.000040	0.000039	1.032
P7	inc3	ins	SEM	0.021937	0.002552	8.594
P7	inc3	ins	PRN	0.008866	0.013840	0.641
P7	inc3	ins	FC	0.006802	0.008289	0.821
P7	inc3	ins	WMC	0.000043	0.000043	0.995
P7	inc5	del	SEM	0.032670	0.006266	5.214
P7	inc5	del	PRN	0.009022	0.006953	1.297
P7	inc5	del	FC	0.005219	0.004924	1.060
P7	inc5	del	WMC	0.000043	0.000037	1.164
P7	inc5	ins	SEM	0.024209	0.004370	5.540
P7	inc5	ins	PRN	0.010238	0.016295	0.628
P7	inc5	ins	FC	0.007000	0.008660	0.808
P7	inc5	ins	WMC	0.000063	0.000042	1.492
P8	inc1	del	SEM	0.086423	0.005588	15.465
P8	inc1	del	PRN	0.036377	0.017754	2.049
P8	inc1	del	FC	0.033644	0.012662	2.657
P8	inc1	del	WMC	0.000061	0.000048	1.272
P8	inc1	ins	SEM	0.092403	0.002195	42.089
P8	inc1	ins	PRN	0.034440	0.015319	2.248
P8	inc1	ins	FC	0.035232	0.011321	3.112
P8	inc1	ins	WMC	0.000070	0.000050	1.390
P8	inc3	del	SEM	0.079150	0.022903	3.456
P8	inc3	del	PRN	0.002166	0.011262	0.192
P8	inc3	del	FC	0.000653	0.005262	0.124
P8	inc3	del	WMC	0.000034	0.000045	0.764
P8	inc3	ins	SEM	0.084835	0.005852	14.497
P8	inc3	ins	PRN	0.032645	1.349876	0.024
P8	inc3	ins	FC	0.029292	0.201032	0.146
P8	inc3	ins	WMC	0.000053	0.000113	0.469
P8	inc5	del	SEM	0.067322	0.024602	2.736
P8	inc5	del	PRN	0.001892	0.017816	0.106
P8	inc5	del	FC	0.000440	0.011237	0.039
P8	inc5	del	WMC	0.000031	0.000033	0.937
P8	inc5	ins	SEM	0.083994	0.011488	7.311
P8	inc5	ins	PRN	0.031767	1.134333	0.028
P8	inc5	ins	FC	0.029967	0.232550	0.129
P8	inc5	ins	WMC	0.000051	0.000106	0.483
P9	inc1	del	SEM	0.041433	0.006424	6.449
P9	inc1	del	PRN	0.015278	0.010302	1.483
P9	inc1	del	FC	0.010696	0.005838	1.832
P9	inc1	del	WMC	0.000054	0.000045	1.215
P9	inc1	ins	SEM	0.044510	0.003908	11.391
P9	inc1	ins	PRN	0.015098	0.011114	1.358
P9	inc1	ins	FC	0.012268	0.009541	1.286
P9	inc1	ins	WMC	0.000056	0.000107	0.521
P9	inc3	del	SEM	0.040597	0.004028	10.078
P9	inc3	del	PRN	0.013716	0.009415	1.457
P9	inc3	del	FC	0.009920	0.005517	1.798
P9	inc3	del	WMC	0.000075	0.000043	1.756
P9	inc3	ins	SEM	0.045158	0.003945	11.448
P9	inc3	ins	PRN	0.015575	0.012647	1.232
P9	inc3	ins	FC	0.011197	0.010040	1.115
P9	inc3	ins	WMC	0.000057	0.000085	0.668
P9	inc5	del	SEM	0.035834	0.017907	2.001
P9	inc5	del	PRN	0.013001	0.014960	0.869
P9	inc5	del	FC	0.009201	0.007456	1.234
P9	inc5	del	WMC	0.000043	0.000056	0.777
P9	inc5	ins	SEM	0.046493	0.006311	7.367
P9	inc5	ins	PRN	0.017017	0.022561	0.754
P9	inc5	ins	FC	0.011746	0.014078	0.834
P9	inc5	ins	WMC	0.000060	0.000059	1.010
P10	inc1	del	SEM	0.226347	0.042013	5.388
P10	inc1	del	PRN	0.009693	0.019801	0.490
P10	inc1	del	FC	0.000909	0.001648	0.552
P10	inc1	del	WMC	0.000054	0.000069	0.787
P10	inc1	ins	SEM	0.259165	0.012684	20.432
P10	inc1	ins	PRN	0.009990	0.022739	0.439
P10	inc1	ins	FC	0.000835	0.000902	0.926
P10	inc1	ins	WMC	0.000040	0.000035	1.131
P10	inc3	del	SEM	0.227994	0.036504	6.246
P10	inc3	del	PRN	0.008211	0.035428	0.232
P10	inc3	del	FC	0.000774	0.001998	0.387
P10	inc3	del	WMC	0.000038	0.000059	0.645
P10	inc3	ins	SEM	0.229410	0.020437	11.225
P10	inc3	ins	PRN	0.007498	0.042481	0.176
P10	inc3	ins	FC	0.000840	0.007402	0.113
P10	inc3	ins	WMC	0.000039	0.000037	1.036
P10	inc5	del	SEM	0.234801	0.041479	5.661
P10	inc5	del	PRN	0.011625	0.046627	0.249
P10	inc5	del	FC	0.000666	0.002024	0.329
P10	inc5	del	WMC	0.000045	0.000041	1.087
P10	inc5	ins	SEM	0.267687	0.028447	9.410
P10	inc5	ins	PRN	0.013313	0.053993	0.247
P10	inc5	ins	FC	0.000879	0.005184	0.170
P10	inc5	ins	WMC	0.000039	0.000037	1.043
P11	inc1	del	SEM	0.259653	0.044417	5.846
P11	inc1	del	PRN	0.011093	0.022213	0.499
P11	inc1	del	FC	0.000271	0.000196	1.381
P11	inc1	del	WMC	0.000037	0.000117	0.316
P11	inc1	ins	SEM	0.277955	0.012636	21.997
P11	inc1	ins	PRN	0.010921	0.022538	0.485
P11	inc1	ins	FC	0.000267	0.000095	2.798
P11	inc1	ins	WMC	0.000037	0.000031	1.199
P11	inc3	del	SEM	0.227570	0.040740	5.586
P11	inc3	del	PRN	0.005617	0.040382	0.139
P11	inc3	del	FC	0.000207	0.001280	0.161
P11	inc3	del	WMC	0.000030	0.000037	0.812
P11	inc3	ins	SEM	0.260369	0.021466	12.129
P11	inc3	ins	PRN	0.010782	0.042803	0.252
P11	inc3	ins	FC	0.000216	0.000195	1.109
P11	inc3	ins	WMC	0.000031	0.000030	1.038
P11	inc5	del	SEM	0.228618	0.044790	5.104
P11	inc5	del	PRN	0.008111	0.046381	0.175
P11	inc5	del	FC	0.000209	0.001356	0.154
P11	inc5	del	WMC	0.000031	0.000032	0.941
P11	inc5	ins	SEM	0.272081	0.027646	9.842
P11	inc5	ins	PRN	0.010518	0.053473	0.197
P11	inc5	ins	FC	0.000221	0.000256	0.865
P11	inc5	ins	WMC	0.000031	0.000049	0.634
P12	inc1	del	SEM	1.586246	0.060281	26.314
P12	inc1	del	PRN	0.233258	0.171542	1.360
P12	inc1	del	FC	0.111663	0.073062	1.528
P12	inc1	del	WMC	0.000289	0.000262	1.105
P12	inc1	ins	SEM	2.419714	0.036487	66.317
P12	inc1	ins	PRN	0.266102	0.870140	0.306
P12	inc1	ins	FC	0.147388	0.378183	0.390
P12	inc1	ins	WMC	0.000407	0.000648	0.628
P12	inc3	del	SEM	0.951421	0.235974	4.032
P12	inc3	del	PRN	0.148015	0.362137	0.409
P12	inc3	del	FC	0.102064	0.073425	1.390
P12	inc3	del	WMC	0.000208	0.000392	0.531
P12	inc3	ins	SEM	1.351382	0.152247	8.876
P12	inc3	ins	PRN	0.183038	1.064545	0.172
P12	inc3	ins	FC	0.126617	0.367002	0.345
P12	inc3	ins	WMC	0.000275	0.000698	0.393
P12	inc5	del	SEM	0.863031	0.347674	2.482
P12	inc5	del	PRN	0.107613	0.495391	0.217
P12	inc5	del	FC	0.062526	0.067092	0.932
P12	inc5	del	WMC	0.000129	0.000195	0.662
P12	inc5	ins	SEM	1.192526	0.285558	4.176
P12	inc5	ins	PRN	0.200007	2.447524	0.082
P12	inc5	ins	FC	0.139459	0.609793	0.229
P12	inc5	ins	WMC	0.000366	0.000785	0.465
P13	inc1	del	SEM	2.245982	0.212703	10.559
P13	inc1	del	PRN	0.272804	0.329367	0.828
P13	inc1	del	FC	0.504304	0.080996	6.226
P13	inc1	del	WMC	0.001080	0.000753	1.433
P13	inc1	ins	SEM	3.584528	0.079152	45.287
P13	inc1	ins	PRN	0.301611	0.336286	0.897
P13	inc1	ins	FC	0.364472	0.797329	0.457
P13	inc1	ins	WMC	0.001364	0.001437	0.949
P13	inc3	del	SEM	1.896529	0.547251	3.466
P13	inc3	del	PRN	0.165096	0.620762	0.266
P13	inc3	del	FC	0.080300	0.081778	0.982
P13	inc3	del	WMC	0.000432	0.000674	0.642
P13	inc3	ins	SEM	3.199152	0.297707	10.746
P13	inc3	ins	PRN	0.303825	1.950126	0.156
P13	inc3	ins	FC	0.367900	0.852630	0.431
P13	inc3	ins	WMC	0.001495	0.001487	1.006
P13	inc5	del	SEM	1.621187	0.939059	1.726
P13	inc5	del	PRN	0.112993	0.982350	0.115
P13	inc5	del	FC	0.042560	0.088578	0.480
P13	inc5	del	WMC	0.000178	0.000463	0.385
P13	inc5	ins	SEM	2.341568	0.702775	3.332
P13	inc5	ins	PRN	0.265843	3.418196	0.078
P13	inc5	ins	FC	0.367378	1.015384	0.362
P13	inc5	ins	WMC	0.001213	0.001529	0.793
P14	inc1	del	SEM	3.481304	0.393562	8.846
P14	inc1	del	PRN	0.217349	0.407049	0.534
P14	inc1	del	FC	0.369324	0.085072	4.341
P14	inc1	del	WMC	0.001265	0.001165	1.086
P14	inc1	ins	SEM	4.301843	0.130845	32.877
P14	inc1	ins	PRN	0.390635	2.225182	0.176
P14	inc1	ins	FC	0.791078	1.117945	0.708
P14	inc1	ins	WMC	0.002023	0.002305	0.878
P14	inc3	del	SEM	2.940271	1.053372	2.791
P14	inc3	del	PRN	0.169813	0.844853	0.201
P14	inc3	del	FC	0.051080	0.082687	0.618
P14	inc3	del	WMC	0.000327	0.001039	0.314
P14	inc3	ins	SEM	3.676304	0.528349	6.958
P14	inc3	ins	PRN	0.367677	3.100239	0.119
P14	inc3	ins	FC	0.764514	1.079006	0.709
P14	inc3	ins	WMC	0.001845	0.002416	0.764
P14	inc5	del	SEM	2.453899	1.572643	1.560
P14	inc5	del	PRN	0.122754	1.526917	0.080
P14	inc5	del	FC	0.023777	0.064193	0.370
P14	inc5	del	WMC	0.000254	0.000465	0.546
P14	inc5	ins	SEM	3.550889	0.933557	3.804
P14	inc5	ins	PRN	0.322068	4.712544	0.068
P14	inc5	ins	FC	0.699351	1.175785	0.595
P14	inc5	ins	WMC	0.001710	0.002805	0.610
P15	inc1	del	SEM	13.138471	1.559447	8.425
P15	inc1	del	PRN	0.534187	1.047293	0.510
P15	inc1	del	FC	0.906495	0.195040	4.648
P15	inc1	del	WMC	0.001866	0.002438	0.765
P15	inc1	ins	SEM	9.890460	0.409398	24.159
P15	inc1	ins	PRN	0.639593	2.599321	0.246
P15	inc1	ins	FC	2.353103	2.859521	0.823
P15	inc1	ins	WMC	0.004151	0.030181	0.138
P15	inc3	del	SEM	7.293411	2.931849	2.488
P15	inc3	del	PRN	0.304655	2.342736	0.130
P15	inc3	del	FC	0.047370	0.154897	0.306
P15	inc3	del	WMC	0.000356	0.000558	0.637
P15	inc3	ins	SEM	9.217068	1.363925	6.758
P15	inc3	ins	PRN	0.647018	5.468991	0.118
P15	inc3	ins	FC	2.378854	1.987011	1.197
P15	inc3	ins	WMC	0.004914	0.006305	0.779
P15	inc5	del	SEM	6.093589	3.564849	1.709
P15	inc5	del	PRN	0.277368	3.495054	0.079
P15	inc5	del	FC	0.045143	0.149400	0.302
P15	inc5	del	WMC	0.000343	0.000479	0.715
P15	inc5	ins	SEM	9.350145	2.188749	4.272
P15	inc5	ins	PRN	0.637157	6.922778	0.092
P15	inc5	ins	FC	2.298954	2.736336	0.840
P15	inc5	ins	WMC	0.004544	0.006188	0.734
P16	inc1	del	SEM	13.325187	4.104828	3.246
P16	inc1	del	PRN	0.662569	1.763570	0.376
P16	inc1	del	FC	0.919023	0.377773	2.433
P16	inc1	del	WMC	0.001866	0.002871	0.650
P16	inc1	ins	SEM	14.419324	0.867676	16.618
P16	inc1	ins	PRN	0.947317	4.293456	0.221
P16	inc1	ins	FC	3.975530	4.014646	0.990
P16	inc1	ins	WMC	0.008170	0.010143	0.805
P16	inc3	del	SEM	11.907698	5.847362	2.036
P16	inc3	del	PRN	0.494774	3.826674	0.129
P16	inc3	del	FC	0.067508	0.327374	0.206
P16	inc3	del	WMC	0.000665	0.001097	0.606
P16	inc3	ins	SEM	14.285393	2.244387	6.365
P16	inc3	ins	PRN	1.045106	9.372541	0.112
P16	inc3	ins	FC	4.053383	4.159863	0.974
P16	inc3	ins	WMC	0.007792	0.010956	0.711
P16	inc5	del	SEM	10.726381	6.828901	1.571
P16	inc5	del	PRN	0.430824	6.057943	0.071
P16	inc5	del	FC	0.051208	0.319971	0.160
P16	inc5	del	WMC	0.000532	0.000767	0.694
P16	inc5	ins	SEM	15.019261	3.635544	4.131
P16	inc5	ins	PRN	0.985178	11.984416	0.082
P16	inc5	ins	FC	4.067878	4.111485	0.989
P16	inc5	ins	WMC	0.008594	0.013061	0.658
P17	inc1	del	SEM	19.783993	7.508286	2.635
P17	inc1	del	PRN	0.839804	2.503426	0.335
P17	inc1	del	FC	1.552361	0.602068	2.578
P17	inc1	del	WMC	0.002740	0.004601	0.596
P17	inc1	ins	SEM	21.167731	1.049936	20.161
P17	inc1	ins	PRN	1.362337	6.957222	0.196
P17	inc1	ins	FC	4.991568	8.152477	0.612
P17	inc1	ins	WMC	0.011607	0.096650	0.120
P17	inc3	del	SEM	17.842647	9.361490	1.906
P17	inc3	del	PRN	0.697264	5.484214	0.127
P17	inc3	del	FC	0.093109	0.490114	0.190
P17	inc3	del	WMC	0.000911	0.001385	0.658
P17	inc3	ins	SEM	20.845412	3.045086	6.846
P17	inc3	ins	PRN	1.454554	13.624641	0.107
P17	inc3	ins	FC	5.014703	6.862392	0.731
P17	inc3	ins	WMC	0.012859	0.017283	0.744
P17	inc5	del	SEM	15.255336	10.990110	1.388
P17	inc5	del	PRN	0.577210	7.568639	0.076
P17	inc5	del	FC	0.053819	0.464140	0.116
P17	inc5	del	WMC	0.000752	0.001017	0.740
P17	inc5	ins	SEM	21.058234	4.829687	4.360
P17	inc5	ins	PRN	1.359670	17.087250	0.080
P17	inc5	ins	FC	4.857787	7.111825	0.683
P17	inc5	ins	WMC	0.012072	0.019221	0.628
P18	inc1	del	SEM	24.164998	12.008071	2.012
P18	inc1	del	PRN	1.134603	3.076206	0.369
P18	inc1	del	FC	1.573439	0.809237	1.944
P18	inc1	del	WMC	0.003326	0.003575	0.930
P18	inc1	ins	SEM	27.236640	1.371948	19.853
P18	inc1	ins	PRN	1.700431	10.797843	0.157
P18	inc1	ins	FC	6.572554	5.205213	1.263
P18	inc1	ins	WMC	0.019608	0.035457	0.553
P18	inc3	del	SEM	21.498865	13.843759	1.553
P18	inc3	del	PRN	0.834838	6.875334	0.121
P18	inc3	del	FC	0.079235	0.717810	0.110
P18	inc3	del	WMC	0.001021	0.001642	0.622
P18	inc3	ins	SEM	26.043408	4.583560	5.682
P18	inc3	ins	PRN	1.776499	18.890649	0.094
P18	inc3	ins	FC	7.311716	5.434741	1.345
P18	inc3	ins	WMC	0.020517	0.052841	0.388
P18	inc5	del	SEM	19.051526	15.362085	1.240
P18	inc5	del	PRN	0.710640	10.439993	0.068
P18	inc5	del	FC	0.063873	0.679185	0.094
P18	inc5	del	WMC	0.000886	0.001140	0.777
P18	inc5	ins	SEM	25.993145	7.121243	3.650
P18	inc5	ins	PRN	1.755904	23.552281	0.075
P18	inc5	ins	FC	6.425092	5.025301	1.279
P18	inc5	ins	WMC	0.019003	0.025807	0.736
P19	inc1	del	SEM	31.182333	18.830237	1.656
P19	inc1	del	PRN	1.372927	4.249811	0.323
P19	inc1	del	FC	1.801392	1.116198	1.614
P19	inc1	del	WMC	0.003188	0.003591	0.888
P19	inc1	ins	SEM	40.772985	2.230873	18.277
P19	inc1	ins	PRN	2.546373	18.559545	0.137
P19	inc1	ins	FC	10.859598	6.709046	1.619
P19	inc1	ins	WMC	0.034235	0.032230	1.062
P19	inc3	del	SEM	27.125491	20.662074	1.313
P19	inc3	del	PRN	0.992715	9.386476	0.106
P19	inc3	del	FC	0.041516	1.038808	0.040
P19	inc3	del	WMC	0.001227	0.001793	0.684
P19	inc3	ins	SEM	37.194128	7.197824	5.167
P19	inc3	ins	PRN	4.097560	31.038380	0.132
P19	inc3	ins	FC	12.022034	10.946678	1.098
P19	inc3	ins	WMC	0.037440	0.043085	0.869
P19	inc5	del	SEM	24.128818	21.502240	1.122
P19	inc5	del	PRN	0.862235	14.674533	0.059
P19	inc5	del	FC	0.034657	1.030844	0.034
P19	inc5	del	WMC	0.001173	0.001587	0.739
P19	inc5	ins	SEM	36.095263	10.776478	3.349
P19	inc5	ins	PRN	2.376186	38.649447	0.061
P19	inc5	ins	FC	10.177870	12.067559	0.843
P19	inc5	ins	WMC	0.028672	0.040588	0.706
P20	inc1	del	SEM	23.230283	7.224893	3.215
P20	inc1	del	PRN	2.026520	3.214817	0.630
P20	inc1	del	FC	2.644726	1.976857	1.338
P20	inc1	del	WMC	0.012806	0.014388	0.890
P20	inc1	ins	SEM	23.458028	0.921280	25.462
P20	inc1	ins	PRN	2.510219	5.125113	0.490
P20	inc1	ins	FC	6.341444	3.898823	1.627
P20	inc1	ins	WMC	0.017832	0.016678	1.069
P20	inc3	del	SEM	20.872690	15.019120	1.390
P20	inc3	del	PRN	1.329142	5.287875	0.251
P20	inc3	del	FC	1.146838	1.988981	0.577
P20	inc3	del	WMC	0.007616	0.011856	0.642
P20	inc3	ins	SEM	24.262939	2.927604	8.288
P20	inc3	ins	PRN	3.221762	35.416933	0.091
P20	inc3	ins	FC	4.929698	35.012930	0.141
P20	inc3	ins	WMC	0.019240	0.067875	0.283
P20	inc5	del	SEM	18.968011	18.150697	1.045
P20	inc5	del	PRN	0.989770	7.826350	0.126
P20	inc5	del	FC	0.473860	1.941499	0.244
P20	inc5	del	WMC	0.003357	0.006276	0.535
P20	inc5	ins	SEM	23.712349	4.724020	5.020
P20	inc5	ins	PRN	2.280601	38.246878	0.060
P20	inc5	ins	FC	6.594122	33.019388	0.200
P20	inc5	ins	WMC	0.021465	0.041386	0.519
```

#### 2026-01-09 (post-fix rerun)
Legacy run: 2026-01-09 post-fix, inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc1	0.194069	0.519645	2.678	1
P1	inc3	0.229571	0.513915	2.239	1
P1	inc5	0.594321	0.441185	0.742	1
P3	inc1	0.647965	0.405309	0.626	1
P3	inc3	0.666429	0.449008	0.674	1
P3	inc5	0.754287	0.413788	0.549	1
P4	inc1	0.015575	0.015974	1.026	1
P4	inc3	0.014534	0.016409	1.129	1
P4	inc5	0.018372	0.017835	0.971	1
P5	inc1	0.017280	0.015438	0.893	1
P5	inc3	0.017071	0.016748	0.981	1
P5	inc5	0.015393	0.014487	0.941	1
P6	inc1	0.050608	0.048368	0.956	1
P6	inc3	0.058748	0.048011	0.817	1
P6	inc5	0.056322	0.051558	0.915	1
P7	inc1	0.109587	0.120688	1.101	1
P7	inc3	0.137401	0.131418	0.956	1
P7	inc5	0.137810	0.128632	0.933	1
P8	inc1	0.267559	0.430122	1.608	1
P8	inc3	0.467368	0.404941	0.866	1
P8	inc5	1.633401	0.349616	0.214	1
P9	inc1	0.124938	0.199879	1.600	1
P9	inc3	0.210857	0.202309	0.959	1
P9	inc5	0.221700	0.207476	0.936	1
P10	inc1	0.412860	0.645395	1.563	1
P10	inc3	0.463995	0.646879	1.394	1
P10	inc5	0.604941	0.745003	1.232	1
P11	inc1	0.342649	0.547737	1.599	1
P11	inc3	0.429868	0.661546	1.539	1
P11	inc5	0.529117	0.677693	1.281	1
P12	inc1	3.214876	3.671281	1.142	1
P12	inc3	7.173889	3.518608	0.490	1
P12	inc5	7.746429	3.283018	0.424	1
P13	inc1	4.535383	7.599825	1.676	1
P13	inc3	6.864388	6.955791	1.013	1
P13	inc5	7.831079	6.874586	0.878	1
P14	inc1	8.530714	11.769947	1.380	1
P14	inc3	10.822894	11.146831	1.030	1
P14	inc5	13.294106	11.402932	0.858	1
P15	inc1	19.302159	30.179378	1.564	1
P15	inc3	27.430529	28.559028	1.041	1
P15	inc5	31.070705	28.031695	0.902	1
P16	inc1	33.837666	48.993638	1.448	1
P16	inc3	44.546503	45.624610	1.024	1
P16	inc5	51.444408	45.489276	0.884	1
P17	inc1	53.039856	68.598614	1.293	1
P17	inc3	67.651573	65.426974	0.967	1
P17	inc5	77.143617	64.360135	0.834	1
P18	inc1	70.452234	84.913686	1.205	1
P18	inc3	88.677936	83.868373	0.946	1
P18	inc5	101.166053	81.792562	0.808	1
P19	inc1	96.114424	112.246172	1.168	1
P19	inc3	122.202203	110.635033	0.905	1
P19	inc5	137.009689	110.528643	0.807	1
P20	inc1	56.208667	79.573999	1.416	1
P20	inc3	86.931920	83.349990	0.959	1
P20	inc5	172.173196	81.444416	0.473	1
```

#### 2026-01-09 (pre-fix run)
Legacy run: 2026-01-09 pre-fix, inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc1	0.604551	0.373050	0.617	1
P1	inc3	0.790896	0.396658	0.502	1
P1	inc5	0.681234	0.409831	0.602	1
P3	inc1	0.653207	0.387377	0.593	1
P3	inc3	0.529795	0.386704	0.730	1
P3	inc5	0.676294	0.397174	0.587	1
P4	inc1	0.012401	0.014256	1.150	1
P4	inc3	0.012129	0.015080	1.243	1
P4	inc5	0.013769	0.015167	1.102	1
P5	inc1	0.014648	0.014195	0.969	1
P5	inc3	0.013719	0.013065	0.952	1
P5	inc5	0.017921	0.014458	0.807	1
P6	inc1	0.035919	0.043847	1.221	1
P6	inc3	0.060882	0.040107	0.659	1
P6	inc5	0.058952	0.043452	0.737	1
P7	inc1	0.074652	0.112049	1.501	0
P7	inc3	0.079892	0.113963	1.426	0
P7	inc5	0.085110	0.115735	1.360	0
P8	inc1	0.211576	0.385113	1.820	1
P8	inc3	0.250679	0.399446	1.593	1
P8	inc5	0.394993	0.374243	0.947	1
P9	inc1	0.127537	0.181056	1.420	1
P9	inc3	0.143274	0.188003	1.312	1
P9	inc5	0.154803	0.204918	1.324	1
P10	inc1	0.350396	0.520656	1.486	1
P10	inc3	0.392633	0.606679	1.545	1
P10	inc5	0.545664	0.664452	1.218	1
P11	inc1	0.312864	0.580883	1.857	1
P11	inc3	0.392436	0.570571	1.454	1
P11	inc5	0.474962	0.634948	1.337	1
P12	inc1	1.946632	3.240192	1.665	1
P12	inc3	3.482785	3.211639	0.922	1
P12	inc5	4.074973	3.221125	0.790	1
P13	inc1	4.143678	6.442994	1.555	1
P13	inc3	5.153035	6.808034	1.321	1
P13	inc5	5.836867	6.328045	1.084	1
P14	inc1	7.559217	10.818736	1.431	1
P14	inc3	9.485047	10.214262	1.077	1
P14	inc5	11.899553	10.071940	0.846	1
P15	inc1	17.405719	27.654654	1.589	1
P15	inc3	23.352126	26.472463	1.134	1
P15	inc5	26.944416	25.539730	0.948	1
P16	inc1	32.918036	44.385915	1.348	1
P16	inc3	44.361851	42.919150	0.967	1
P16	inc5	49.746649	42.566471	0.856	1
P17	inc1	52.646017	65.098837	1.237	1
P17	inc3	64.572775	61.473317	0.952	1
P17	inc5	71.500162	62.564490	0.875	1
P18	inc1	63.116400	81.246020	1.287	1
P18	inc3	80.687500	78.303712	0.970	1
P18	inc5	92.060303	76.914889	0.835	1
P19	inc1	91.542006	106.882494	1.168	1
P19	inc3	116.656206	103.822104	0.890	1
P19	inc5	128.814061	103.063635	0.800	1
P20	inc1	48.325213	79.132816	1.638	1
P20	inc3	70.973095	75.755336	1.067	1
P20	inc5	85.374623	76.569061	0.897	0
```

### Legacy quick snapshot (inc10 sample=1, 2025-12-21)
Run: 2025-12-21, inc10, sample=1, legacy snapshot.
- P4-P9: inc ~=0.10-0.20s, full ~=0.09-0.17s (roughly parity; prune dominates inc).
- P10-P11: inc ~=0.68/0.66s vs full ~=0.94/0.63s.
- P12: inc ~=2.80s vs full ~=2.18s (prune+fwd heavy).
- P13: inc ~=8.27s vs full ~=5.92s (prune+fwd heavy).

### Approximate per-case evaluation time (inc1/inc3/inc5, 2026-01-09 post-fix)
Run: 2026-01-09 post-fix, inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
DeltaEval_s sums inc+full wall time for the three deltas; add baseline full+inc
overhead (not timed in the table) and compile time (~30-70s per case) when planning
end-to-end runs. Full suite delta runs sum to ~45.7 min on this machine.
```tsv
Case	DeltaEval_s	DeltaEval_min
P1	2.493	0.04
P3	3.337	0.06
P4	0.099	0.00
P5	0.096	0.00
P6	0.314	0.01
P7	0.766	0.01
P8	3.553	0.06
P9	1.167	0.02
P10	3.519	0.06
P11	3.189	0.05
P12	28.608	0.48
P13	40.661	0.68
P14	66.967	1.12
P15	164.573	2.74
P16	269.936	4.50
P17	396.221	6.60
P18	510.871	8.51
P19	688.736	11.48
P20	559.682	9.33
```

### Post-prune delta node/edge counts (inc logs, 2026-01-09 post-fix)
Run: 2026-01-09 post-fix, inc logs (view/prune deltas), base-dir experiments/side_channel_inc_eval.
Counts are from `[inc-naive] delta counts: ...` in `FORWARD_COMPILATION_INC` logs; del counts
are from turn2 (delete) and ins counts from turn3 (insert) for each delta. These are
view/prune deltas and can be asymmetric; use `apply_delta_ops` for SEM delta proxies.
```tsv
Case	Delta	DelNodes	DelEdges	InsNodes	InsEdges	NodeRatio	EdgeRatio
P1	inc1	22	97	27	102	1.227	1.052
P1	inc3	37	161	42	166	1.135	1.031
P1	inc5	37	161	42	166	1.135	1.031
P3	inc1	0	0	0	0	0.000	0.000
P3	inc3	22	97	27	102	1.227	1.052
P3	inc5	44	194	54	204	1.227	1.052
P4	inc1	0	0	0	0	0.000	0.000
P4	inc3	0	0	0	0	0.000	0.000
P4	inc5	0	0	0	0	0.000	0.000
P5	inc1	10	9	49	24	4.900	2.667
P5	inc3	10	9	49	24	4.900	2.667
P5	inc5	11	9	49	24	4.455	2.667
P6	inc1	24	240	40	248	1.667	1.033
P6	inc3	26	241	42	249	1.615	1.033
P6	inc5	34	247	115	284	3.382	1.150
P7	inc1	42	370	83	386	1.976	1.043
P7	inc3	69	726	125	752	1.812	1.036
P7	inc5	69	726	125	752	1.812	1.036
P8	inc1	23	22	100	61	4.348	2.773
P8	inc3	100	1831	172	1867	1.720	1.020
P8	inc5	184	1915	1043	10793	5.668	5.636
P9	inc1	53	50	187	102	3.528	2.040
P9	inc3	151	772	421	875	2.788	1.133
P9	inc5	168	783	439	886	2.613	1.132
P10	inc1	12	16	26	33	2.167	2.062
P10	inc3	17	27	31	44	1.824	1.630
P10	inc5	31	52	86	227	2.774	4.365
P11	inc1	0	0	0	0	0.000	0.000
P11	inc3	2	1	4	2	2.000	2.000
P11	inc5	2	1	4	2	2.000	2.000
P12	inc1	504	4973	1764	8453	3.500	1.700
P12	inc3	1376	19805	3808	31184	2.767	1.575
P12	inc5	1641	20037	4694	31697	2.860	1.582
P13	inc1	723	684	3001	1697	4.151	2.481
P13	inc3	1975	6343	7347	9797	3.720	1.545
P13	inc5	2626	8325	8786	11785	3.346	1.416
P14	inc1	1723	3714	7036	13150	4.084	3.541
P14	inc3	3206	5480	11897	17761	3.711	3.241
P14	inc5	3770	5930	13507	23746	3.583	4.004
P15	inc1	4608	4495	17334	18122	3.762	4.032
P15	inc3	8237	9012	29263	38780	3.553	4.303
P15	inc5	9363	9918	29807	39087	3.183	3.941
P16	inc1	9367	12226	32825	21962	3.504	1.796
P16	inc3	15366	17810	47905	30688	3.118	1.723
P16	inc5	16850	18877	49909	41863	2.962	2.218
P17	inc1	15592	20501	53020	44176	3.400	2.155
P17	inc3	21318	26793	66418	51900	3.116	1.937
P17	inc5	23314	28986	68161	53745	2.924	1.854
P18	inc1	21180	25403	70609	45796	3.334	1.803
P18	inc3	27885	31381	85036	54923	3.050	1.750
P18	inc5	29978	35773	87204	65309	2.909	1.826
P19	inc1	28080	27607	92977	53429	3.311	1.935
P19	inc3	34738	33327	106692	63168	3.071	1.895
P19	inc5	37339	34978	108749	64349	2.912	1.840
P20	inc1	8066	15948	34194	36828	4.239	2.309
P20	inc3	19644	34445	77738	60641	3.957	1.761
P20	inc5	27631	80451	98066	116734	3.549	1.451
```

### Pre-prune applyDelta op counts (P17 inc1, 2026-01-09 post-fix)
Run: 2026-01-09 post-fix, P17 inc1, apply_delta_ops, base-dir experiments/side_channel_inc_eval.
Counts are captured from the `PRUNING_INC` stage log line
`[inc-iter N] mode=INC_NAIVE apply_delta_ops: ...` (after applyDelta, before prune). These
are symmetric and should be used as SEM delta proxies.
In this dataset, deleted facts with surviving derivations do not occur, so applyDelta
op counts are symmetric across delete/insert turns.
```tsv
Turn	ApplyDelTuples	ApplyDelRuleApps	ApplyDelFacts	ApplyInsTuples	ApplyInsRuleApps	ApplyInsFacts
del	46521	221157	829	0	0	0
ins	0	0	0	46521	221157	829
```

### Pre-prune applyDelta view-update counts (P17 inc1, 2026-01-09 post-fix)
Run: 2026-01-09 post-fix, P17 inc1, apply_delta_view, base-dir experiments/side_channel_inc_eval.
Counts are captured from the `PRUNING_INC` stage log line
`[inc-iter N] mode=INC_NAIVE apply_delta_view: ...` (after applyDelta, before prune). These
are asymmetric because the view/prune bookkeeping reintroduces nodes/edges that remain derivable.
```tsv
Turn	InsNodes	InsEdges	DelNodes	DelEdges
del	0	0	28471	221157
ins	46912	221157	0	0
```

### Per-stage breakdown (turn2=delete, turn3=insert)
Speedup = Full_s / Inc_s (>1.0 means inc faster).
#### 2026-01-14 (full ruleset, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, P12-P20 only, no fc-profile)
Run: 2026-01-14, full ruleset, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, sample=1, run timeout=600s, compile timeout=600s, base-dir experiments/side_channel_inc_eval_p12_p20_nofcprofile.
```tsv
Case	Delta	Turn	Full_FC_s	Inc_FC_s	Speedup
P12	inc0p1	del	0.010974	0.006553	1.675
P12	inc0p1	ins	0.010721	0.008781	1.221
P12	inc0p3	del	0.009362	0.007240	1.293
P12	inc0p3	ins	0.010905	0.009412	1.159
P12	inc0p5	del	0.008740	0.006398	1.366
P12	inc0p5	ins	0.011117	0.010834	1.026
P13	inc0p1	del	0.119868	0.020279	5.911
P13	inc0p1	ins	0.122558	0.194095	0.631
P13	inc0p3	del	0.120468	0.015153	7.950
P13	inc0p3	ins	0.117971	0.225429	0.523
P13	inc0p5	del	0.122414	0.018268	6.701
P13	inc0p5	ins	0.119400	0.202273	0.590
P14	inc0p1	del	0.270253	0.041149	6.568
P14	inc0p1	ins	0.250988	0.323350	0.776
P14	inc0p3	del	0.260793	0.027491	9.487
P14	inc0p3	ins	0.257288	0.270270	0.952
P14	inc0p5	del	0.268954	0.024817	10.838
P14	inc0p5	ins	0.261207	0.278162	0.939
P15	inc0p1	del	0.623979	0.063471	9.831
P15	inc0p1	ins	0.606106	0.773111	0.784
P15	inc0p3	del	0.602573	0.047922	12.574
P15	inc0p3	ins	0.581755	0.575849	1.010
P15	inc0p5	del	0.265133	0.036079	7.349
P15	inc0p5	ins	0.554832	0.533848	1.039
P16	inc0p1	del	1.043616	0.090812	11.492
P16	inc0p1	ins	1.949656	0.217665	8.957
P16	inc0p3	del	1.112335	0.084298	13.195
P16	inc0p3	ins	1.913632	0.286140	6.688
P16	inc0p5	del	0.502494	0.065957	7.619
P16	inc0p5	ins	1.886797	0.361555	5.219
P17	inc0p1	del	2.950080	0.152423	19.355
P17	inc0p1	ins	2.876821	0.282871	10.170
P17	inc0p3	del	1.611664	0.138742	11.616
P17	inc0p3	ins	2.922361	0.478214	6.111
P17	inc0p5	del	0.727965	0.088974	8.182
P17	inc0p5	ins	2.891903	3.614899	0.800
P18	inc0p1	del	3.506628	0.193211	18.149
P18	inc0p1	ins	3.426745	6.126412	0.559
P18	inc0p3	del	2.128058	0.131529	16.179
P18	inc0p3	ins	3.406902	6.005507	0.567
P18	inc0p5	del	0.986998	0.146111	6.755
P18	inc0p5	ins	3.507023	6.386191	0.549
P19	inc0p1	del	5.087865	0.295742	17.204
P19	inc0p1	ins	8.946519	0.728925	12.274
P19	inc0p3	del	3.178468	0.174705	18.193
P19	inc0p3	ins	9.153917	0.974576	9.393
P19	inc0p5	del	1.419814	0.176985	8.022
P19	inc0p5	ins	9.189030	1.207633	7.609
P20	inc0p1	del	2.728552	0.393514	6.934
P20	inc0p1	ins	2.787628	0.582046	4.789
P20	inc0p3	del	2.648662	0.308617	8.582
P20	inc0p3	ins	2.737045	0.601279	4.552
P20	inc0p5	del	2.588641	0.349981	7.397
P20	inc0p5	ins	2.757853	0.617845	4.464
```

##### Delta live_nodes ratio (FC workload proxy, 2026-01-14 run)
From `FORWARD_COMPILATION_INC` stage info: `changed_node_count / live_nodes` per turn. This captures how much of the
live node set is updated in FC for each delta.
```tsv
Case	Delta	Turn	DeltaLiveNodes	LiveNodes	DeltaLiveRatio
P12	inc0p1	del	9	1720	0.005233
P12	inc0p1	ins	26	1744	0.014908
P12	inc0p3	del	97	1577	0.061509
P12	inc0p3	ins	290	1762	0.164586
P12	inc0p5	del	132	1502	0.087883
P12	inc0p5	ins	398	1773	0.224478
P13	inc0p1	del	21	4178	0.005026
P13	inc0p1	ins	60	3824	0.015690
P13	inc0p3	del	191	3912	0.048824
P13	inc0p3	ins	585	4044	0.144659
P13	inc0p5	del	269	3709	0.072526
P13	inc0p5	ins	829	4173	0.198658
P14	inc0p1	del	254	7069	0.035932
P14	inc0p1	ins	735	7160	0.102654
P14	inc0p3	del	389	6684	0.058199
P14	inc0p3	ins	1136	6945	0.163571
P14	inc0p5	del	585	6195	0.094431
P14	inc0p5	ins	1696	7162	0.236805
P15	inc0p1	del	933	22069	0.042276
P15	inc0p1	ins	2620	26356	0.099408
P15	inc0p3	del	2286	16133	0.141697
P15	inc0p3	ins	6214	30967	0.200665
P15	inc0p5	del	3129	13308	0.235122
P15	inc0p5	ins	8622	31855	0.270664
P16	inc0p1	del	2356	53243	0.044250
P16	inc0p1	ins	6409	76198	0.084110
P16	inc0p3	del	5570	32649	0.170602
P16	inc0p3	ins	15399	82499	0.186657
P16	inc0p5	del	8087	20601	0.392554
P16	inc0p5	ins	21734	87986	0.247017
P17	inc0p1	del	2400	114421	0.020975
P17	inc0p1	ins	6367	145095	0.043882
P17	inc0p3	del	7760	62835	0.123498
P17	inc0p3	ins	20914	152425	0.137208
P17	inc0p5	del	11517	37471	0.307358
P17	inc0p5	ins	30641	160497	0.190913
P18	inc0p1	del	5329	165064	0.032284
P18	inc0p1	ins	14413	251820	0.057235
P18	inc0p3	del	12276	82607	0.148607
P18	inc0p3	ins	32605	258446	0.126158
P18	inc0p5	del	16095	47978	0.335466
P18	inc0p5	ins	42105	260226	0.161802
P19	inc0p1	del	8096	247005	0.032777
P19	inc0p1	ins	21658	396653	0.054602
P19	inc0p3	del	17105	114196	0.149786
P19	inc0p3	ins	45737	399671	0.114437
P19	inc0p5	del	21131	71174	0.296892
P19	inc0p5	ins	54924	418120	0.131359
P20	inc0p1	del	754	82283	0.009163
P20	inc0p1	ins	2631	84441	0.031158
P20	inc0p3	del	2989	74211	0.040277
P20	inc0p3	ins	10415	84796	0.122824
P20	inc0p5	del	4498	69929	0.064322
P20	inc0p5	ins	15898	85362	0.186242
```

##### FC speedup vs 1/DeltaLiveRatio (2026-01-14 run)
Plot: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114/fc_speedup_vs_inv_delta_live_ratio.png`
Data: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114/fc_inv_delta_live_ratio.tsv`
Script: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114/plot_fc_speedup_vs_inv_delta_live_ratio.py`
- Expectation: FC speedup should roughly follow 1/DeltaLiveRatio if the workload scales linearly with the number of
  updated live nodes.
- Observed outliers:
  - P18 insert: speedup ~0.55-0.57 even with moderate 1/DeltaLiveRatio (~6-17). The gap is dominated by dynamic
    reordering (inc reorder ~5.2-5.5s vs full ~2.9s), which overwhelms the theoretical delta savings.
  - P13/P14/P15 and P17 inc0p5 insert: speedup <1 despite high 1/DeltaLiveRatio. PreConfig deltas are tiny (ms), so the
    mismatch is explained by higher insertion time plus smaller-but-nontrivial reordering overhead; inc rounds are
    fewer than full, so the cost is per-round BDD work rather than iteration count.

##### FC speedup vs 1/DeltaLiveRatio (2026-01-14 reuse-var-index enabled run)
Plot: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114_reuse/fc_speedup_vs_inv_delta_live_ratio.png`
Data: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114_reuse/fc_inv_delta_live_ratio.tsv`
Script: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114_reuse/plot_fc_speedup_vs_inv_delta_live_ratio.py`
- This run uses reuse-var-index (enabled by default; disable with `--no-reuse-var-index`); all insert points shift upward (FC speedups >1).
- The prior outliers (P13/P14/P15/P17/P18 insert) are resolved; see
  `experiments/side_channel_inc_eval_p12_p20_nofcprofile/fc-insert-speedup-reuse-compare.tsv`.

##### FC speedup vs DeltaLiveRatio (2026-01-14 reuse-var-index enabled run)
Plot: `img/fc_speedup_vs_delta_live_ratio_p12_p20_20260114_reuse/fc_speedup_vs_delta_live_ratio.png`
Data: `img/fc_speedup_vs_delta_live_ratio_p12_p20_20260114_reuse/fc_delta_live_ratio.tsv`
Script: `img/fc_speedup_vs_delta_live_ratio_p12_p20_20260114_reuse/plot_fc_speedup_vs_delta_live_ratio.py`
- x-axis is `DeltaLiveRatio = DeltaLiveNodes / LiveNodes`; ideal linear scaling implies speedup ~ `1/DeltaLiveRatio`.
- Plot overlays the theoretical `1/x` curve, clamps x to `[0,1]`, and caps y at the max observed speedup.
- Outliers match the 1/DeltaLiveRatio view: P18 insert still has the largest gap (dynamic reordering dominates), while
  the remaining insert points sit below the theoretical line due to BDD insertion overhead and reordering noise.
- The near-1x outliers are all P12: its FC times are ~0.01s, so fixed overhead dominates and incremental runs only reach
  ~1.3-1.6x; consider excluding P12 from plots/tables when comparing theoretical speedups.
- The variance in the plot is expected: delete runs are cheaper (conditioning + caching) and do not trigger reordering,
  while insert/delete both reuse the variable order, which avoids repeating CUDD’s expensive reordering work that guards
  against exponential blowups.

##### Insert FC slower-than-full analysis (2026-01-14 run)
Source: per-turn FC logs under `experiments/side_channel_inc_eval_p12_p20_nofcprofile/P*/output/log_P*_*_inc_*.json`
and `log_P*_*_full_*.json` (turn3 insert).
```tsv
Case	Delta	Speedup	Full_FC_s	Inc_FC_s	Full_pre_ms	Inc_pre_ms	Full_ins_ms	Inc_ins_ms	Full_rounds	Inc_rounds	Full_reorder_s	Inc_reorder_s	Full_cache_hit	Inc_cache_hit
P13	inc0p1	0.631	0.122558	0.194095	0.541187	3.0	108.0	175.0	3038	25	0.090	0.170	0.129116%	0.064236%
P13	inc0p3	0.523	0.117971	0.225429	0.494902	4.0	105.0	201.0	3038	248	0.090	0.180	0.126863%	0.086755%
P13	inc0p5	0.590	0.119400	0.202273	0.53134	3.0	104.0	178.0	3038	351	0.090	0.160	0.133222%	0.081555%
P14	inc0p1	0.776	0.250988	0.323350	0.639949	10.0	230.0	283.0	4475	307	0.200	0.260	0.000000%	0.000000%
P14	inc0p3	0.952	0.257288	0.270270	0.630933	5.0	235.0	233.0	4475	476	0.200	0.220	0.008706%	0.000000%
P14	inc0p5	0.939	0.261207	0.278162	0.635342	5.0	240.0	245.0	4475	710	0.210	0.220	0.000000%	0.000000%
P15	inc0p1	0.784	0.606106	0.773111	1.226166	19.0	548.0	667.0	9045	1110	0.470	0.610	0.383169%	0.398827%
P17	inc0p5	0.800	2.891903	3.614899	3.348509	43.0	2715.0	3327.0	21719	13012	2.480	2.910	0.000000%	0.050320%
P18	inc0p1	0.559	3.426745	6.126412	3.604545	78.0	3212.0	5805.0	27477	6109	2.910	5.470	0.000000%	0.026252%
P18	inc0p3	0.567	3.406902	6.005507	3.400634	54.0	3190.0	5722.0	27477	13844	2.890	5.180	0.000000%	0.036515%
P18	inc0p5	0.549	3.507023	6.386191	3.786028	63.0	3259.0	6026.0	27477	17903	2.940	5.360	0.000000%	0.032655%
```
- In these cases, insert slowdown correlates with higher inc insertion time (Inc_ins_ms > Full_ins_ms) and, for P18,
  substantially higher reordering time (inc ~5.2-5.5s vs full ~2.9s). PreConfig deltas are small (3-78ms) and do not
  explain multi-second FC gaps. Inc rounds are lower than full rounds, so the per-round cost is higher for inc in
  these cases (likely due to BDD operation cost / reordering overhead), not iteration count.
#### 2026-01-11 (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_trimmed_eval_small.
```tsv
Case	Delta	Turn	Stage	Full_s	Inc_s	Speedup
P1	inc0p1	del	FC	0.000258	0.000236	1.096
P1	inc0p1	del	PRN	0.000512	0.000491	1.041
P1	inc0p1	del	SEM	0.004462	0.001337	3.337
P1	inc0p1	del	WMC	0.000027	0.000048	0.563
P1	inc0p1	ins	FC	0.000258	0.000156	1.658
P1	inc0p1	ins	PRN	0.000342	0.000252	1.356
P1	inc0p1	ins	SEM	0.004459	0.000877	5.083
P1	inc0p1	ins	WMC	0.000026	0.000048	0.546
P1	inc0p3	del	FC	0.000260	0.000118	2.196
P1	inc0p3	del	PRN	0.000338	0.000243	1.393
P1	inc0p3	del	SEM	0.004789	0.001294	3.702
P1	inc0p3	del	WMC	0.000026	0.000032	0.837
P1	inc0p3	ins	FC	0.000292	0.000106	2.759
P1	inc0p3	ins	PRN	0.000347	0.000212	1.633
P1	inc0p3	ins	SEM	0.004914	0.001761	2.790
P1	inc0p3	ins	WMC	0.000086	0.000043	1.988
P1	inc0p5	del	FC	0.000257	0.000142	1.806
P1	inc0p5	del	PRN	0.000388	0.000276	1.406
P1	inc0p5	del	SEM	0.004895	0.001850	2.646
P1	inc0p5	del	WMC	0.000037	0.000038	0.977
P1	inc0p5	ins	FC	0.000255	0.000240	1.063
P1	inc0p5	ins	PRN	0.000313	0.000262	1.197
P1	inc0p5	ins	SEM	0.004703	0.002201	2.137
P1	inc0p5	ins	WMC	0.000026	0.000044	0.592
P3	inc0p1	del	FC	0.000256	0.000172	1.493
P3	inc0p1	del	PRN	0.000425	0.000386	1.100
P3	inc0p1	del	SEM	0.004446	0.000877	5.071
P3	inc0p1	del	WMC	0.000026	0.000125	0.209
P3	inc0p1	ins	FC	0.000268	0.000123	2.182
P3	inc0p1	ins	PRN	0.000307	0.000214	1.433
P3	inc0p1	ins	SEM	0.004410	0.000889	4.961
P3	inc0p1	ins	WMC	0.000026	0.000030	0.889
P3	inc0p3	del	FC	0.000256	0.000115	2.230
P3	inc0p3	del	PRN	0.000324	0.000262	1.238
P3	inc0p3	del	SEM	0.004418	0.001602	2.758
P3	inc0p3	del	WMC	0.000026	0.000032	0.815
P3	inc0p3	ins	FC	0.000258	0.000107	2.417
P3	inc0p3	ins	PRN	0.000355	0.000233	1.525
P3	inc0p3	ins	SEM	0.004350	0.000746	5.833
P3	inc0p3	ins	WMC	0.000026	0.000030	0.876
P3	inc0p5	del	FC	0.000258	0.000110	2.350
P3	inc0p5	del	PRN	0.000321	0.000242	1.325
P3	inc0p5	del	SEM	0.004419	0.000926	4.773
P3	inc0p5	del	WMC	0.000027	0.000032	0.826
P3	inc0p5	ins	FC	0.000256	0.000105	2.430
P3	inc0p5	ins	PRN	0.000308	0.000245	1.257
P3	inc0p5	ins	SEM	0.004451	0.000925	4.811
P3	inc0p5	ins	WMC	0.000026	0.000029	0.896
P4	inc0p1	del	FC	0.000080	0.000071	1.130
P4	inc0p1	del	PRN	0.000191	0.000209	0.911
P4	inc0p1	del	SEM	0.000266	0.000909	0.293
P4	inc0p1	del	WMC	0.000024	0.000053	0.453
P4	inc0p1	ins	FC	0.000083	0.000041	2.011
P4	inc0p1	ins	PRN	0.000117	0.000109	1.069
P4	inc0p1	ins	SEM	0.000301	0.001761	0.171
P4	inc0p1	ins	WMC	0.000028	0.000025	1.081
P4	inc0p3	del	FC	0.000095	0.000038	2.507
P4	inc0p3	del	PRN	0.000123	0.000254	0.483
P4	inc0p3	del	SEM	0.000248	0.000792	0.313
P4	inc0p3	del	WMC	0.000024	0.000025	0.952
P4	inc0p3	ins	FC	0.000094	0.000040	2.326
P4	inc0p3	ins	PRN	0.000116	0.000095	1.223
P4	inc0p3	ins	SEM	0.000236	0.001131	0.208
P4	inc0p3	ins	WMC	0.000024	0.000025	0.959
P4	inc0p5	del	FC	0.000078	0.000042	1.848
P4	inc0p5	del	PRN	0.000103	0.000118	0.877
P4	inc0p5	del	SEM	0.000253	0.002054	0.123
P4	inc0p5	del	WMC	0.000024	0.000026	0.935
P4	inc0p5	ins	FC	0.000093	0.000038	2.458
P4	inc0p5	ins	PRN	0.000097	0.000098	0.996
P4	inc0p5	ins	SEM	0.000229	0.000441	0.518
P4	inc0p5	ins	WMC	0.000024	0.000025	0.967
P5	inc0p1	del	FC	0.000096	0.000943	0.102
P5	inc0p1	del	PRN	0.000170	0.000277	0.612
P5	inc0p1	del	SEM	0.000268	0.001177	0.228
P5	inc0p1	del	WMC	0.000025	0.000052	0.480
P5	inc0p1	ins	FC	0.000204	0.000327	0.626
P5	inc0p1	ins	PRN	0.000249	0.000448	0.555
P5	inc0p1	ins	SEM	0.000318	0.001045	0.304
P5	inc0p1	ins	WMC	0.000026	0.000034	0.774
P5	inc0p3	del	FC	0.000079	0.001087	0.072
P5	inc0p3	del	PRN	0.000124	0.000201	0.618
P5	inc0p3	del	SEM	0.000242	0.000621	0.390
P5	inc0p3	del	WMC	0.000024	0.000025	0.978
P5	inc0p3	ins	FC	0.000192	0.000277	0.695
P5	inc0p3	ins	PRN	0.000247	0.000443	0.557
P5	inc0p3	ins	SEM	0.000304	0.002492	0.122
P5	inc0p3	ins	WMC	0.000026	0.000027	0.977
P5	inc0p5	del	FC	0.000078	0.000975	0.080
P5	inc0p5	del	PRN	0.000143	0.000253	0.563
P5	inc0p5	del	SEM	0.000253	0.000803	0.315
P5	inc0p5	del	WMC	0.000024	0.000025	0.977
P5	inc0p5	ins	FC	0.000239	0.000256	0.936
P5	inc0p5	ins	PRN	0.000246	0.000448	0.550
P5	inc0p5	ins	SEM	0.000318	0.002410	0.132
P5	inc0p5	ins	WMC	0.000027	0.000027	0.985
P6	inc0p1	del	FC	0.000462	0.000352	1.314
P6	inc0p1	del	PRN	0.000616	0.000461	1.336
P6	inc0p1	del	SEM	0.000451	0.001169	0.386
P6	inc0p1	del	WMC	0.000031	0.000032	0.965
P6	inc0p1	ins	FC	0.000453	0.000349	1.298
P6	inc0p1	ins	PRN	0.000580	0.000414	1.401
P6	inc0p1	ins	SEM	0.000409	0.001046	0.391
P6	inc0p1	ins	WMC	0.000030	0.000031	0.996
P6	inc0p3	del	FC	0.000506	0.000380	1.332
P6	inc0p3	del	PRN	0.000621	0.000499	1.245
P6	inc0p3	del	SEM	0.000465	0.002456	0.189
P6	inc0p3	del	WMC	0.000031	0.000038	0.810
P6	inc0p3	ins	FC	0.000480	0.000370	1.295
P6	inc0p3	ins	PRN	0.000629	0.000410	1.535
P6	inc0p3	ins	SEM	0.000420	0.002647	0.159
P6	inc0p3	ins	WMC	0.000031	0.000031	0.997
P6	inc0p5	del	FC	0.000475	0.000303	1.567
P6	inc0p5	del	PRN	0.000622	0.000472	1.320
P6	inc0p5	del	SEM	0.000468	0.001437	0.326
P6	inc0p5	del	WMC	0.000031	0.000030	1.031
P6	inc0p5	ins	FC	0.000602	0.000347	1.737
P6	inc0p5	ins	PRN	0.000782	0.000419	1.866
P6	inc0p5	ins	SEM	0.000529	0.002191	0.241
P6	inc0p5	ins	WMC	0.000040	0.000031	1.269
P7	inc0p1	del	FC	0.000541	0.001122	0.482
P7	inc0p1	del	PRN	0.000716	0.000574	1.247
P7	inc0p1	del	SEM	0.001030	0.001997	0.516
P7	inc0p1	del	WMC	0.000037	0.000055	0.671
P7	inc0p1	ins	FC	0.000558	0.000458	1.219
P7	inc0p1	ins	PRN	0.000773	0.000556	1.390
P7	inc0p1	ins	SEM	0.000996	0.001919	0.519
P7	inc0p1	ins	WMC	0.000034	0.000034	1.005
P7	inc0p3	del	FC	0.000607	0.001162	0.522
P7	inc0p3	del	PRN	0.000847	0.000635	1.334
P7	inc0p3	del	SEM	0.001064	0.002429	0.438
P7	inc0p3	del	WMC	0.000034	0.000033	1.014
P7	inc0p3	ins	FC	0.000555	0.000483	1.151
P7	inc0p3	ins	PRN	0.000732	0.000575	1.274
P7	inc0p3	ins	SEM	0.001063	0.001320	0.805
P7	inc0p3	ins	WMC	0.000034	0.000034	1.002
P7	inc0p5	del	FC	0.000467	0.001135	0.412
P7	inc0p5	del	PRN	0.000704	0.000575	1.226
P7	inc0p5	del	SEM	0.001088	0.001371	0.794
P7	inc0p5	del	WMC	0.000035	0.000031	1.102
P7	inc0p5	ins	FC	0.000660	0.000759	0.869
P7	inc0p5	ins	PRN	0.000919	0.001396	0.658
P7	inc0p5	ins	SEM	0.001249	0.001967	0.635
P7	inc0p5	ins	WMC	0.000040	0.000036	1.099
P8	inc0p1	del	FC	0.000881	0.000572	1.540
P8	inc0p1	del	PRN	0.001283	0.001216	1.055
P8	inc0p1	del	SEM	0.001715	0.001457	1.177
P8	inc0p1	del	WMC	0.000039	0.000064	0.609
P8	inc0p1	ins	FC	0.001143	0.000368	3.101
P8	inc0p1	ins	PRN	0.001203	0.000733	1.641
P8	inc0p1	ins	SEM	0.001491	0.001040	1.433
P8	inc0p1	ins	WMC	0.000051	0.000056	0.913
P8	inc0p3	del	FC	0.000862	0.000379	2.273
P8	inc0p3	del	PRN	0.001144	0.000849	1.349
P8	inc0p3	del	SEM	0.001634	0.001795	0.910
P8	inc0p3	del	WMC	0.000040	0.000061	0.654
P8	inc0p3	ins	FC	0.000848	0.000504	1.684
P8	inc0p3	ins	PRN	0.001206	0.000749	1.609
P8	inc0p3	ins	SEM	0.001525	0.001229	1.242
P8	inc0p3	ins	WMC	0.000047	0.000084	0.558
P8	inc0p5	del	FC	0.000803	0.000397	2.020
P8	inc0p5	del	PRN	0.001196	0.001097	1.091
P8	inc0p5	del	SEM	0.001611	0.002366	0.681
P8	inc0p5	del	WMC	0.000038	0.000057	0.677
P8	inc0p5	ins	FC	0.000913	0.000393	2.324
P8	inc0p5	ins	PRN	0.001299	0.000852	1.523
P8	inc0p5	ins	SEM	0.001529	0.001306	1.171
P8	inc0p5	ins	WMC	0.000040	0.000059	0.680
P9	inc0p1	del	FC	0.001251	0.000589	2.125
P9	inc0p1	del	PRN	0.001650	0.001210	1.363
P9	inc0p1	del	SEM	0.001257	0.001286	0.978
P9	inc0p1	del	WMC	0.000050	0.000081	0.618
P9	inc0p1	ins	FC	0.001257	0.000731	1.718
P9	inc0p1	ins	PRN	0.001714	0.001100	1.558
P9	inc0p1	ins	SEM	0.001407	0.001156	1.217
P9	inc0p1	ins	WMC	0.000054	0.000094	0.582
P9	inc0p3	del	FC	0.001282	0.000632	2.029
P9	inc0p3	del	PRN	0.001618	0.001214	1.333
P9	inc0p3	del	SEM	0.001222	0.002341	0.522
P9	inc0p3	del	WMC	0.000052	0.000082	0.640
P9	inc0p3	ins	FC	0.001259	0.000550	2.287
P9	inc0p3	ins	PRN	0.001653	0.001062	1.557
P9	inc0p3	ins	SEM	0.001107	0.001924	0.575
P9	inc0p3	ins	WMC	0.000052	0.000105	0.494
P9	inc0p5	del	FC	0.001216	0.001953	0.622
P9	inc0p5	del	PRN	0.001667	0.001166	1.430
P9	inc0p5	del	SEM	0.001198	0.001289	0.929
P9	inc0p5	del	WMC	0.000050	0.000057	0.882
P9	inc0p5	ins	FC	0.001280	0.004224	0.303
P9	inc0p5	ins	PRN	0.001563	0.001313	1.190
P9	inc0p5	ins	SEM	0.001103	0.002088	0.529
P9	inc0p5	ins	WMC	0.000051	0.000048	1.068
P10	inc0p1	del	FC	0.000320	0.000119	2.696
P10	inc0p1	del	PRN	0.001427	0.001401	1.019
P10	inc0p1	del	SEM	0.017382	0.003102	5.603
P10	inc0p1	del	WMC	0.000047	0.000038	1.248
P10	inc0p1	ins	FC	0.000243	0.000112	2.169
P10	inc0p1	ins	PRN	0.001102	0.001690	0.652
P10	inc0p1	ins	SEM	0.016505	0.008052	2.050
P10	inc0p1	ins	WMC	0.000032	0.000035	0.927
P10	inc0p3	del	FC	0.000251	0.000152	1.653
P10	inc0p3	del	PRN	0.000963	0.003378	0.285
P10	inc0p3	del	SEM	0.014270	0.004797	2.975
P10	inc0p3	del	WMC	0.000033	0.000061	0.539
P10	inc0p3	ins	FC	0.000258	0.000111	2.330
P10	inc0p3	ins	PRN	0.001157	0.003153	0.367
P10	inc0p3	ins	SEM	0.016878	0.006660	2.534
P10	inc0p3	ins	WMC	0.000031	0.000035	0.872
P10	inc0p5	del	FC	0.000240	0.000125	1.931
P10	inc0p5	del	PRN	0.000929	0.005337	0.174
P10	inc0p5	del	SEM	0.013707	0.006738	2.034
P10	inc0p5	del	WMC	0.000031	0.000039	0.798
P10	inc0p5	ins	FC	0.000240	0.000115	2.086
P10	inc0p5	ins	PRN	0.001050	0.004901	0.214
P10	inc0p5	ins	SEM	0.016625	0.005310	3.131
P10	inc0p5	ins	WMC	0.000031	0.000036	0.856
P11	inc0p1	del	FC	0.000187	0.000108	1.726
P11	inc0p1	del	PRN	0.000997	0.002639	0.378
P11	inc0p1	del	SEM	0.014945	0.009417	1.587
P11	inc0p1	del	WMC	0.000029	0.000039	0.748
P11	inc0p1	ins	FC	0.000186	0.000081	2.296
P11	inc0p1	ins	PRN	0.001003	0.002232	0.449
P11	inc0p1	ins	SEM	0.016825	0.003209	5.244
P11	inc0p1	ins	WMC	0.000030	0.000032	0.933
P11	inc0p3	del	FC	0.000202	0.000148	1.360
P11	inc0p3	del	PRN	0.000867	0.003044	0.285
P11	inc0p3	del	SEM	0.013123	0.004124	3.182
P11	inc0p3	del	WMC	0.000030	0.000032	0.927
P11	inc0p3	ins	FC	0.000188	0.000151	1.249
P11	inc0p3	ins	PRN	0.000900	0.003366	0.267
P11	inc0p3	ins	SEM	0.015848	0.006339	2.500
P11	inc0p3	ins	WMC	0.000030	0.000031	0.966
P11	inc0p5	del	FC	0.000184	0.001525	0.121
P11	inc0p5	del	PRN	0.000613	0.007134	0.086
P11	inc0p5	del	SEM	0.014515	0.005800	2.503
P11	inc0p5	del	WMC	0.000031	0.000044	0.702
P11	inc0p5	ins	FC	0.000194	0.000156	1.243
P11	inc0p5	ins	PRN	0.000893	0.006299	0.142
P11	inc0p5	ins	SEM	0.019501	0.005728	3.405
P11	inc0p5	ins	WMC	0.000030	0.000031	0.975
P12	inc0p1	del	FC	0.009612	0.009810	0.980
P12	inc0p1	del	PRN	0.012056	0.009327	1.293
P12	inc0p1	del	SEM	0.014929	0.003384	4.411
P12	inc0p1	del	WMC	0.000261	0.000214	1.222
P12	inc0p1	ins	FC	0.010453	0.097336	0.107
P12	inc0p1	ins	PRN	0.011797	0.007532	1.566
P12	inc0p1	ins	SEM	0.012602	0.003251	3.876
P12	inc0p1	ins	WMC	0.000331	0.000434	0.762
P12	inc0p3	del	FC	0.009901	0.009904	1.000
P12	inc0p3	del	PRN	0.010550	0.009569	1.103
P12	inc0p3	del	SEM	0.013814	0.003490	3.958
P12	inc0p3	del	WMC	0.000242	0.000211	1.148
P12	inc0p3	ins	FC	0.009964	0.049742	0.200
P12	inc0p3	ins	PRN	0.012155	0.008185	1.485
P12	inc0p3	ins	SEM	0.013780	0.003236	4.258
P12	inc0p3	ins	WMC	0.000239	0.000369	0.649
P12	inc0p5	del	FC	0.008863	0.009996	0.887
P12	inc0p5	del	PRN	0.010569	0.008934	1.183
P12	inc0p5	del	SEM	0.013045	0.003690	3.535
P12	inc0p5	del	WMC	0.000233	0.000210	1.113
P12	inc0p5	ins	FC	0.013224	0.049239	0.269
P12	inc0p5	ins	PRN	0.015128	0.008893	1.701
P12	inc0p5	ins	SEM	0.013609	0.002218	6.136
P12	inc0p5	ins	WMC	0.000311	0.000461	0.674
P13	inc0p1	del	FC	0.310675	0.032938	9.432
P13	inc0p1	del	PRN	0.031745	0.026835	1.183
P13	inc0p1	del	SEM	0.029434	0.004762	6.182
P13	inc0p1	del	WMC	0.001004	0.000545	1.844
P13	inc0p1	ins	FC	0.273196	1.004080	0.272
P13	inc0p1	ins	PRN	0.030429	0.022599	1.346
P13	inc0p1	ins	SEM	0.024921	0.004537	5.493
P13	inc0p1	ins	WMC	0.001089	0.000930	1.172
P13	inc0p3	del	FC	0.350770	0.029537	11.876
P13	inc0p3	del	PRN	0.025118	0.021430	1.172
P13	inc0p3	del	SEM	0.026197	0.004828	5.427
P13	inc0p3	del	WMC	0.001202	0.000521	2.305
P13	inc0p3	ins	FC	0.303749	0.797630	0.381
P13	inc0p3	ins	PRN	0.028108	0.023290	1.207
P13	inc0p3	ins	SEM	0.028873	0.005028	5.742
P13	inc0p3	ins	WMC	0.000949	0.001031	0.920
P13	inc0p5	del	FC	0.381976	0.034151	11.185
P13	inc0p5	del	PRN	0.021424	0.028385	0.755
P13	inc0p5	del	SEM	0.021999	0.005810	3.786
P13	inc0p5	del	WMC	0.000999	0.000611	1.636
P13	inc0p5	ins	FC	0.269438	0.768142	0.351
P13	inc0p5	ins	PRN	0.027580	0.032775	0.841
P13	inc0p5	ins	SEM	0.027698	0.005834	4.748
P13	inc0p5	ins	WMC	0.000993	0.001270	0.782
P14	inc0p1	del	FC	0.724219	0.046865	15.453
P14	inc0p1	del	PRN	0.037953	0.033957	1.118
P14	inc0p1	del	SEM	0.040013	0.004783	8.366
P14	inc0p1	del	WMC	0.001676	0.001239	1.353
P14	inc0p1	ins	FC	0.691813	1.168568	0.592
P14	inc0p1	ins	PRN	0.049794	0.041861	1.190
P14	inc0p1	ins	SEM	0.044723	0.004882	9.161
P14	inc0p1	ins	WMC	0.001643	0.001618	1.015
P14	inc0p3	del	FC	0.718997	0.050780	14.159
P14	inc0p3	del	PRN	0.044596	0.039213	1.137
P14	inc0p3	del	SEM	0.040072	0.005703	7.026
P14	inc0p3	del	WMC	0.001618	0.001169	1.384
P14	inc0p3	ins	FC	0.752254	1.262636	0.596
P14	inc0p3	ins	PRN	0.041176	0.042677	0.965
P14	inc0p3	ins	SEM	0.041167	0.005614	7.332
P14	inc0p3	ins	WMC	0.001973	0.001890	1.044
P14	inc0p5	del	FC	0.786499	0.048087	16.356
P14	inc0p5	del	PRN	0.037939	0.038148	0.995
P14	inc0p5	del	SEM	0.041958	0.007684	5.460
P14	inc0p5	del	WMC	0.001393	0.001121	1.243
P14	inc0p5	ins	FC	0.681929	1.034966	0.659
P14	inc0p5	ins	PRN	0.039478	0.052824	0.747
P14	inc0p5	ins	SEM	0.042660	0.007388	5.775
P14	inc0p5	ins	WMC	0.001761	0.001300	1.354
P15	inc0p1	del	FC	3.047640	0.180820	16.855
P15	inc0p1	del	PRN	0.102469	0.098074	1.045
P15	inc0p1	del	SEM	0.108737	0.018486	5.882
P15	inc0p1	del	WMC	0.004515	0.003399	1.329
P15	inc0p1	ins	FC	2.540522	0.923416	2.751
P15	inc0p1	ins	PRN	0.112419	0.144588	0.778
P15	inc0p1	ins	SEM	0.110210	0.034138	3.228
P15	inc0p1	ins	WMC	0.004209	0.002916	1.443
P15	inc0p3	del	FC	1.650958	0.140283	11.769
P15	inc0p3	del	PRN	0.069786	0.097094	0.719
P15	inc0p3	del	SEM	0.095775	0.028815	3.324
P15	inc0p3	del	WMC	0.002783	0.002668	1.043
P15	inc0p3	ins	FC	2.611321	1.904509	1.371
P15	inc0p3	ins	PRN	0.110225	0.269336	0.409
P15	inc0p3	ins	SEM	0.111738	0.025520	4.379
P15	inc0p3	ins	WMC	0.005495	0.004374	1.256
P15	inc0p5	del	FC	1.531727	0.151539	10.108
P15	inc0p5	del	PRN	0.052210	0.087989	0.593
P15	inc0p5	del	SEM	0.075237	0.034535	2.179
P15	inc0p5	del	WMC	0.003030	0.002009	1.509
P15	inc0p5	ins	FC	2.547238	1.855658	1.373
P15	inc0p5	ins	PRN	0.108278	0.270908	0.400
P15	inc0p5	ins	SEM	0.097734	0.027915	3.501
P15	inc0p5	ins	WMC	0.003659	0.004239	0.863
P16	inc0p1	del	FC	4.289082	0.426019	10.068
P16	inc0p1	del	PRN	0.199490	0.166236	1.200
P16	inc0p1	del	SEM	0.163779	0.030880	5.304
P16	inc0p1	del	WMC	0.006711	0.005078	1.321
P16	inc0p1	ins	FC	4.351065	1.816880	2.395
P16	inc0p1	ins	PRN	0.251434	0.329554	0.763
P16	inc0p1	ins	SEM	0.196405	0.037250	5.273
P16	inc0p1	ins	WMC	0.008108	0.006803	1.192
P16	inc0p3	del	FC	2.704384	0.402855	6.713
P16	inc0p3	del	PRN	0.129060	0.250505	0.515
P16	inc0p3	del	SEM	0.162215	0.060967	2.661
P16	inc0p3	del	WMC	0.004618	0.003980	1.160
P16	inc0p3	ins	FC	4.494153	4.703978	0.955
P16	inc0p3	ins	PRN	0.239124	0.906170	0.264
P16	inc0p3	ins	SEM	0.201602	0.065006	3.101
P16	inc0p3	ins	WMC	0.009494	0.009184	1.034
P16	inc0p5	del	FC	3.678109	0.325992	11.283
P16	inc0p5	del	PRN	0.081496	0.188459	0.432
P16	inc0p5	del	SEM	0.137573	0.059488	2.313
P16	inc0p5	del	WMC	0.003557	0.003011	1.181
P16	inc0p5	ins	FC	4.224442	4.484049	0.942
P16	inc0p5	ins	PRN	0.223250	1.371037	0.163
P16	inc0p5	ins	SEM	0.181818	0.059098	3.077
P16	inc0p5	ins	WMC	0.008333	0.009149	0.911
P17	inc0p1	del	FC	4.917857	0.813809	6.043
P17	inc0p1	del	PRN	0.263594	0.270804	0.973
P17	inc0p1	del	SEM	0.281173	0.049873	5.638
P17	inc0p1	del	WMC	0.010827	0.006708	1.614
P17	inc0p1	ins	FC	5.926456	8.831407	0.671
P17	inc0p1	ins	PRN	0.483917	0.906958	0.534
P17	inc0p1	ins	SEM	0.327089	0.064152	5.099
P17	inc0p1	ins	WMC	0.015325	0.012483	1.228
P17	inc0p3	del	FC	4.448754	0.685930	6.486
P17	inc0p3	del	PRN	0.201638	0.294751	0.684
P17	inc0p3	del	SEM	0.237279	0.062241	3.812
P17	inc0p3	del	WMC	0.007594	0.005276	1.439
P17	inc0p3	ins	FC	6.146907	8.422660	0.730
P17	inc0p3	ins	PRN	0.470924	1.349119	0.349
P17	inc0p3	ins	SEM	0.328813	0.067742	4.854
P17	inc0p3	ins	WMC	0.016458	0.014957	1.100
P17	inc0p5	del	FC	4.680297	0.603922	7.750
P17	inc0p5	del	PRN	0.141013	0.317554	0.444
P17	inc0p5	del	SEM	0.206403	0.085284	2.420
P17	inc0p5	del	WMC	0.006857	0.003837	1.787
P17	inc0p5	ins	FC	5.737835	9.183012	0.625
P17	inc0p5	ins	PRN	0.444637	1.946739	0.228
P17	inc0p5	ins	SEM	0.325402	0.080864	4.024
P17	inc0p5	ins	WMC	0.015360	0.017094	0.899
P18	inc0p1	del	FC	5.801961	1.295361	4.479
P18	inc0p1	del	PRN	0.336396	0.400026	0.841
P18	inc0p1	del	SEM	0.359826	0.075882	4.742
P18	inc0p1	del	WMC	0.014832	0.008139	1.822
P18	inc0p1	ins	FC	6.907898	4.032478	1.713
P18	inc0p1	ins	PRN	0.494874	1.212912	0.408
P18	inc0p1	ins	SEM	0.396444	0.078139	5.074
P18	inc0p1	ins	WMC	0.026203	0.021273	1.232
P18	inc0p3	del	FC	4.841083	1.042358	4.644
P18	inc0p3	del	PRN	0.188187	0.327559	0.575
P18	inc0p3	del	SEM	0.317769	0.112652	2.821
P18	inc0p3	del	WMC	0.008045	0.006356	1.266
P18	inc0p3	ins	FC	6.833086	4.462466	1.531
P18	inc0p3	ins	PRN	0.517305	3.813643	0.136
P18	inc0p3	ins	SEM	0.379968	0.123031	3.088
P18	inc0p3	ins	WMC	0.025578	0.020834	1.228
P18	inc0p5	del	FC	3.102575	0.849826	3.651
P18	inc0p5	del	PRN	0.097733	0.384196	0.254
P18	inc0p5	del	SEM	0.272955	0.127900	2.134
P18	inc0p5	del	WMC	0.004528	0.003582	1.264
P18	inc0p5	ins	FC	6.530702	4.541250	1.438
P18	inc0p5	ins	PRN	0.489285	4.848070	0.101
P18	inc0p5	ins	SEM	0.392006	0.131476	2.982
P18	inc0p5	ins	WMC	0.019506	0.021242	0.918
P19	inc0p1	del	FC	6.124874	2.366321	2.588
P19	inc0p1	del	PRN	0.556373	0.548439	1.014
P19	inc0p1	del	SEM	0.458766	0.140386	3.268
P19	inc0p1	del	WMC	0.019514	0.010442	1.869
P19	inc0p1	ins	FC	8.484889	4.410780	1.924
P19	inc0p1	ins	PRN	0.755852	2.820433	0.268
P19	inc0p1	ins	SEM	0.567848	0.162099	3.503
P19	inc0p1	ins	WMC	0.030750	0.021762	1.413
P19	inc0p3	del	FC	4.693566	1.599100	2.935
P19	inc0p3	del	PRN	0.241065	0.491717	0.490
P19	inc0p3	del	SEM	0.394764	0.162682	2.427
P19	inc0p3	del	WMC	0.007865	0.006035	1.303
P19	inc0p3	ins	FC	8.426217	5.113266	1.648
P19	inc0p3	ins	PRN	0.641318	6.367033	0.101
P19	inc0p3	ins	SEM	0.507724	0.178437	2.845
P19	inc0p3	ins	WMC	0.027683	0.029159	0.949
P19	inc0p5	del	FC	5.279360	1.305664	4.043
P19	inc0p5	del	PRN	0.147898	0.490438	0.302
P19	inc0p5	del	SEM	0.362572	0.205365	1.766
P19	inc0p5	del	WMC	0.005035	0.003823	1.317
P19	inc0p5	ins	FC	8.347401	5.213631	1.601
P19	inc0p5	ins	PRN	0.578485	7.927107	0.073
P19	inc0p5	ins	SEM	0.527415	0.177267	2.975
P19	inc0p5	ins	WMC	0.027893	0.031542	0.884
P20	inc0p1	del	FC	3.062824	1.939695	1.579
P20	inc0p1	del	PRN	0.782812	0.872839	0.897
P20	inc0p1	del	SEM	0.445935	0.084684	5.266
P20	inc0p1	del	WMC	0.017877	0.013151	1.359
P20	inc0p1	ins	FC	3.251189	3.457220	0.940
P20	inc0p1	ins	PRN	0.850621	0.692266	1.229
P20	inc0p1	ins	SEM	0.522146	0.064647	8.077
P20	inc0p1	ins	WMC	0.023656	0.015379	1.538
P20	inc0p3	del	FC	2.430943	1.267965	1.917
P20	inc0p3	del	PRN	0.580567	0.604886	0.960
P20	inc0p3	del	SEM	0.471718	0.073470	6.421
P20	inc0p3	del	WMC	0.015942	0.013168	1.211
P20	inc0p3	ins	FC	3.347979	3.105850	1.078
P20	inc0p3	ins	PRN	0.758168	0.718624	1.055
P20	inc0p3	ins	SEM	0.457180	0.070169	6.515
P20	inc0p3	ins	WMC	0.014610	0.020075	0.728
P20	inc0p5	del	FC	2.222264	1.308789	1.698
P20	inc0p5	del	PRN	0.632361	0.586975	1.077
P20	inc0p5	del	SEM	0.391553	0.075693	5.173
P20	inc0p5	del	WMC	0.014776	0.013193	1.120
P20	inc0p5	ins	FC	3.488609	3.562673	0.979
P20	inc0p5	ins	PRN	0.795613	0.862600	0.922
P20	inc0p5	ins	SEM	0.464711	0.081904	5.674
P20	inc0p5	ins	WMC	0.018231	0.027487	0.663
```
#### 2026-01-11 (full ruleset, det-opt, apply_delta_graph)
Run: 2026-01-11, full ruleset, det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	Turn	Stage	Full_s	Inc_s	Speedup
P1	inc1	del	SEM	0.005949	0.005426	1.096
P1	inc1	del	PRN	0.000428	0.000323	1.325
P1	inc1	del	FC	0.000296	0.000136	2.185
P1	inc1	del	WMC	0.000030	0.000040	0.771
P1	inc1	ins	SEM	0.005568	0.001506	3.698
P1	inc1	ins	PRN	0.000364	0.000228	1.600
P1	inc1	ins	FC	0.000289	0.000105	2.759
P1	inc1	ins	WMC	0.000027	0.000029	0.922
P1	inc3	del	SEM	0.004767	0.019603	0.243
P1	inc3	del	PRN	0.000388	0.000256	1.512
P1	inc3	del	FC	0.000320	0.000106	3.025
P1	inc3	del	WMC	0.000033	0.000034	0.970
P1	inc3	ins	SEM	0.004770	0.003737	1.277
P1	inc3	ins	PRN	0.000319	0.000259	1.235
P1	inc3	ins	FC	0.000254	0.000103	2.455
P1	inc3	ins	WMC	0.000027	0.000030	0.883
P1	inc5	del	SEM	0.001490	0.018827	0.079
P1	inc5	del	PRN	0.000316	0.000234	1.348
P1	inc5	del	FC	0.000258	0.000106	2.430
P1	inc5	del	WMC	0.000026	0.000032	0.838
P1	inc5	ins	SEM	0.004413	0.013487	0.327
P1	inc5	ins	PRN	0.000307	0.000286	1.071
P1	inc5	ins	FC	0.000256	0.000136	1.883
P1	inc5	ins	WMC	0.000026	0.000037	0.702
P3	inc1	del	SEM	0.002377	0.018805	0.126
P3	inc1	del	PRN	0.000372	0.000322	1.154
P3	inc1	del	FC	0.000255	0.000109	2.339
P3	inc1	del	WMC	0.000027	0.000031	0.869
P3	inc1	ins	SEM	0.004394	0.006261	0.702
P3	inc1	ins	PRN	0.000328	0.000228	1.441
P3	inc1	ins	FC	0.000256	0.000107	2.383
P3	inc1	ins	WMC	0.000026	0.000030	0.869
P3	inc3	del	SEM	0.002250	0.019016	0.118
P3	inc3	del	PRN	0.000220	0.000382	0.577
P3	inc3	del	FC	0.000175	0.000187	0.939
P3	inc3	del	WMC	0.000026	0.000026	0.976
P3	inc3	ins	SEM	0.004686	0.006041	0.776
P3	inc3	ins	PRN	0.000363	0.000447	0.813
P3	inc3	ins	FC	0.000253	0.000280	0.905
P3	inc3	ins	WMC	0.000026	0.000027	0.963
P3	inc5	del	SEM	0.001073	0.014696	0.073
P3	inc5	del	PRN	0.000225	0.000292	0.769
P3	inc5	del	FC	0.000180	0.000183	0.982
P3	inc5	del	WMC	0.000026	0.000027	0.968
P3	inc5	ins	SEM	0.004469	0.010542	0.424
P3	inc5	ins	PRN	0.000348	0.000477	0.730
P3	inc5	ins	FC	0.000255	0.000275	0.928
P3	inc5	ins	WMC	0.000027	0.000028	0.956
P4	inc1	del	SEM	0.000342	0.001842	0.186
P4	inc1	del	PRN	0.000127	0.000222	0.573
P4	inc1	del	FC	0.000103	0.000056	1.853
P4	inc1	del	WMC	0.000040	0.000032	1.267
P4	inc1	ins	SEM	0.000308	0.002716	0.113
P4	inc1	ins	PRN	0.000118	0.000176	0.672
P4	inc1	ins	FC	0.000094	0.000045	2.081
P4	inc1	ins	WMC	0.000024	0.000031	0.775
P4	inc3	del	SEM	0.000335	0.002448	0.137
P4	inc3	del	PRN	0.000166	0.000157	1.053
P4	inc3	del	FC	0.000119	0.000056	2.125
P4	inc3	del	WMC	0.000031	0.000031	0.999
P4	inc3	ins	SEM	0.000406	0.002428	0.167
P4	inc3	ins	PRN	0.000116	0.000103	1.124
P4	inc3	ins	FC	0.000119	0.000036	3.280
P4	inc3	ins	WMC	0.000030	0.000025	1.209
P4	inc5	del	SEM	0.000382	0.002118	0.181
P4	inc5	del	PRN	0.000135	0.000136	0.993
P4	inc5	del	FC	0.000105	0.000051	2.037
P4	inc5	del	WMC	0.000027	0.000026	1.068
P4	inc5	ins	SEM	0.000315	0.001917	0.164
P4	inc5	ins	PRN	0.000115	0.000123	0.940
P4	inc5	ins	FC	0.000096	0.000037	2.610
P4	inc5	ins	WMC	0.000024	0.000025	0.975
P5	inc1	del	SEM	0.000265	0.001689	0.157
P5	inc1	del	PRN	0.000278	0.000408	0.681
P5	inc1	del	FC	0.000192	0.000112	1.715
P5	inc1	del	WMC	0.000026	0.000038	0.676
P5	inc1	ins	SEM	0.000306	0.001391	0.220
P5	inc1	ins	PRN	0.000275	0.000266	1.033
P5	inc1	ins	FC	0.000194	0.000083	2.347
P5	inc1	ins	WMC	0.000026	0.000028	0.928
P5	inc3	del	SEM	0.000281	0.001198	0.234
P5	inc3	del	PRN	0.000249	0.000283	0.880
P5	inc3	del	FC	0.000195	0.000091	2.136
P5	inc3	del	WMC	0.000026	0.000030	0.861
P5	inc3	ins	SEM	0.000388	0.001429	0.271
P5	inc3	ins	PRN	0.000261	0.000265	0.986
P5	inc3	ins	FC	0.000198	0.000084	2.362
P5	inc3	ins	WMC	0.000030	0.000028	1.071
P5	inc5	del	SEM	0.000184	0.000921	0.200
P5	inc5	del	PRN	0.000078	0.000229	0.342
P5	inc5	del	FC	0.000056	0.000907	0.062
P5	inc5	del	WMC	0.000030	0.000025	1.177
P5	inc5	ins	SEM	0.000319	0.001943	0.164
P5	inc5	ins	PRN	0.000266	0.000466	0.571
P5	inc5	ins	FC	0.000200	0.000261	0.765
P5	inc5	ins	WMC	0.000026	0.000027	0.965
P6	inc1	del	SEM	0.000745	0.006285	0.119
P6	inc1	del	PRN	0.000642	0.000696	0.923
P6	inc1	del	FC	0.000535	0.000427	1.252
P6	inc1	del	WMC	0.000031	0.000039	0.806
P6	inc1	ins	SEM	0.000752	0.003410	0.221
P6	inc1	ins	PRN	0.000624	0.000537	1.162
P6	inc1	ins	FC	0.000482	0.000422	1.141
P6	inc1	ins	WMC	0.000038	0.000039	0.978
P6	inc3	del	SEM	0.000887	0.004428	0.200
P6	inc3	del	PRN	0.000572	0.000457	1.252
P6	inc3	del	FC	0.000447	0.001087	0.411
P6	inc3	del	WMC	0.000037	0.000030	1.210
P6	inc3	ins	SEM	0.000731	0.002372	0.308
P6	inc3	ins	PRN	0.000680	0.000749	0.907
P6	inc3	ins	FC	0.000501	0.000617	0.813
P6	inc3	ins	WMC	0.000032	0.000033	0.955
P6	inc5	del	SEM	0.000648	0.003109	0.208
P6	inc5	del	PRN	0.000403	0.000443	0.911
P6	inc5	del	FC	0.000298	0.001093	0.273
P6	inc5	del	WMC	0.000030	0.000031	0.961
P6	inc5	ins	SEM	0.000744	0.003838	0.194
P6	inc5	ins	PRN	0.000654	0.000808	0.810
P6	inc5	ins	FC	0.000570	0.000717	0.795
P6	inc5	ins	WMC	0.000032	0.000033	0.974
P7	inc1	del	SEM	0.001875	0.004001	0.469
P7	inc1	del	PRN	0.000901	0.000837	1.076
P7	inc1	del	FC	0.000675	0.001445	0.467
P7	inc1	del	WMC	0.000041	0.000096	0.425
P7	inc1	ins	SEM	0.001939	0.008006	0.242
P7	inc1	ins	PRN	0.000926	0.000667	1.389
P7	inc1	ins	FC	0.000660	0.000477	1.385
P7	inc1	ins	WMC	0.000040	0.000034	1.186
P7	inc3	del	SEM	0.001825	0.003866	0.472
P7	inc3	del	PRN	0.000865	0.000710	1.219
P7	inc3	del	FC	0.000589	0.001276	0.462
P7	inc3	del	WMC	0.000037	0.000041	0.909
P7	inc3	ins	SEM	0.002089	0.005045	0.414
P7	inc3	ins	PRN	0.000927	0.000935	0.992
P7	inc3	ins	FC	0.000663	0.000748	0.886
P7	inc3	ins	WMC	0.000040	0.000088	0.452
P7	inc5	del	SEM	0.001733	0.004204	0.412
P7	inc5	del	PRN	0.000607	0.000878	0.692
P7	inc5	del	FC	0.000393	0.001373	0.286
P7	inc5	del	WMC	0.000033	0.000039	0.846
P7	inc5	ins	SEM	0.001962	0.003515	0.558
P7	inc5	ins	PRN	0.000910	0.001584	0.575
P7	inc5	ins	FC	0.000673	0.001346	0.500
P7	inc5	ins	WMC	0.000040	0.000059	0.670
P8	inc1	del	SEM	0.005327	0.004240	1.257
P8	inc1	del	PRN	0.001530	0.001655	0.925
P8	inc1	del	FC	0.001115	0.002015	0.554
P8	inc1	del	WMC	0.000053	0.000049	1.075
P8	inc1	ins	SEM	0.004307	0.002971	1.449
P8	inc1	ins	PRN	0.001229	0.001787	0.688
P8	inc1	ins	FC	0.000906	0.000980	0.924
P8	inc1	ins	WMC	0.000043	0.000064	0.673
P8	inc3	del	SEM	0.004764	0.006255	0.762
P8	inc3	del	PRN	0.001133	0.000997	1.137
P8	inc3	del	FC	0.000683	0.001380	0.495
P8	inc3	del	WMC	0.000048	0.000036	1.336
P8	inc3	ins	SEM	0.005635	0.003714	1.517
P8	inc3	ins	PRN	0.001388	0.001830	0.759
P8	inc3	ins	FC	0.001231	0.001475	0.835
P8	inc3	ins	WMC	0.000054	0.000054	0.989
P8	inc5	del	SEM	0.003227	0.007586	0.425
P8	inc5	del	PRN	0.000672	0.001323	0.508
P8	inc5	del	FC	0.000534	0.001991	0.268
P8	inc5	del	WMC	0.000038	0.000037	1.026
P8	inc5	ins	SEM	0.004233	0.006153	0.688
P8	inc5	ins	PRN	0.001731	0.002819	0.614
P8	inc5	ins	FC	0.001230	0.006078	0.202
P8	inc5	ins	WMC	0.000071	0.000070	1.012
P9	inc1	del	SEM	0.002239	0.003866	0.579
P9	inc1	del	PRN	0.001083	0.001014	1.068
P9	inc1	del	FC	0.000873	0.001732	0.504
P9	inc1	del	WMC	0.000042	0.000052	0.811
P9	inc1	ins	SEM	0.002551	0.003623	0.704
P9	inc1	ins	PRN	0.001707	0.002122	0.804
P9	inc1	ins	FC	0.001319	0.005226	0.252
P9	inc1	ins	WMC	0.000053	0.000094	0.565
P9	inc3	del	SEM	0.002331	0.008827	0.264
P9	inc3	del	PRN	0.000943	0.001009	0.935
P9	inc3	del	FC	0.000630	0.001871	0.337
P9	inc3	del	WMC	0.000039	0.000050	0.782
P9	inc3	ins	SEM	0.002559	0.003432	0.746
P9	inc3	ins	PRN	0.001871	0.003015	0.620
P9	inc3	ins	FC	0.001399	0.005312	0.263
P9	inc3	ins	WMC	0.000054	0.000060	0.894
P9	inc5	del	SEM	0.001970	0.006254	0.315
P9	inc5	del	PRN	0.000782	0.001383	0.565
P9	inc5	del	FC	0.000553	0.002480	0.223
P9	inc5	del	WMC	0.000037	0.000060	0.612
P9	inc5	ins	SEM	0.002433	0.003404	0.715
P9	inc5	ins	PRN	0.001639	0.003096	0.530
P9	inc5	ins	FC	0.001401	0.005959	0.235
P9	inc5	ins	WMC	0.000055	0.000059	0.939
P10	inc1	del	SEM	0.022626	0.016401	1.380
P10	inc1	del	PRN	0.001180	0.009050	0.130
P10	inc1	del	FC	0.000367	0.001840	0.199
P10	inc1	del	WMC	0.000037	0.000057	0.641
P10	inc1	ins	SEM	0.029375	0.013756	2.135
P10	inc1	ins	PRN	0.001382	0.008094	0.171
P10	inc1	ins	FC	0.000362	0.000299	1.208
P10	inc1	ins	WMC	0.000037	0.000037	0.995
P10	inc3	del	SEM	0.016200	0.018088	0.896
P10	inc3	del	PRN	0.000995	0.011768	0.085
P10	inc3	del	FC	0.000375	0.001484	0.253
P10	inc3	del	WMC	0.000038	0.000045	0.857
P10	inc3	ins	SEM	0.025992	0.015238	1.706
P10	inc3	ins	PRN	0.001107	0.016486	0.067
P10	inc3	ins	FC	0.000387	0.003901	0.099
P10	inc3	ins	WMC	0.000037	0.000048	0.770
P10	inc5	del	SEM	0.015023	0.018954	0.793
P10	inc5	del	PRN	0.000930	0.014419	0.064
P10	inc5	del	FC	0.000334	0.002229	0.150
P10	inc5	del	WMC	0.000039	0.000049	0.805
P10	inc5	ins	SEM	0.027699	0.015600	1.776
P10	inc5	ins	PRN	0.001224	0.014361	0.085
P10	inc5	ins	FC	0.000392	0.007271	0.054
P10	inc5	ins	WMC	0.000038	0.000038	0.987
P11	inc1	del	SEM	0.019965	0.012252	1.630
P11	inc1	del	PRN	0.000700	0.007094	0.099
P11	inc1	del	FC	0.000192	0.000101	1.901
P11	inc1	del	WMC	0.000031	0.000036	0.845
P11	inc1	ins	SEM	0.026763	0.010427	2.567
P11	inc1	ins	PRN	0.001023	0.007450	0.137
P11	inc1	ins	FC	0.000189	0.000090	2.101
P11	inc1	ins	WMC	0.000030	0.000035	0.854
P11	inc3	del	SEM	0.016677	0.015371	1.085
P11	inc3	del	PRN	0.000771	0.009280	0.083
P11	inc3	del	FC	0.000188	0.001217	0.155
P11	inc3	del	WMC	0.000031	0.000036	0.854
P11	inc3	ins	SEM	0.028387	0.014536	1.953
P11	inc3	ins	PRN	0.001361	0.013932	0.098
P11	inc3	ins	FC	0.000197	0.000168	1.171
P11	inc3	ins	WMC	0.000031	0.000032	0.974
P11	inc5	del	SEM	0.016679	0.017463	0.955
P11	inc5	del	PRN	0.000782	0.010856	0.072
P11	inc5	del	FC	0.000191	0.001375	0.139
P11	inc5	del	WMC	0.000038	0.000033	1.129
P11	inc5	ins	SEM	0.025602	0.013835	1.851
P11	inc5	ins	PRN	0.000728	0.012046	0.060
P11	inc5	ins	FC	0.000215	0.000185	1.160
P11	inc5	ins	WMC	0.000031	0.000032	0.970
P12	inc1	del	SEM	0.042451	0.022958	1.849
P12	inc1	del	PRN	0.010981	0.010724	1.024
P12	inc1	del	FC	0.008370	0.010926	0.766
P12	inc1	del	WMC	0.000326	0.000261	1.253
P12	inc1	ins	SEM	0.045460	0.015938	2.852
P12	inc1	ins	PRN	0.012659	0.015183	0.834
P12	inc1	ins	FC	0.009783	0.227113	0.043
P12	inc1	ins	WMC	0.000247	0.000466	0.530
P12	inc3	del	SEM	0.035452	0.053465	0.663
P12	inc3	del	PRN	0.005460	0.010062	0.543
P12	inc3	del	FC	0.004369	0.010469	0.417
P12	inc3	del	WMC	0.000122	0.000145	0.846
P12	inc3	ins	SEM	0.049778	0.031985	1.556
P12	inc3	ins	PRN	0.011781	0.027200	0.433
P12	inc3	ins	FC	0.009578	0.093751	0.102
P12	inc3	ins	WMC	0.000212	0.000437	0.485
P12	inc5	del	SEM	0.030687	0.079825	0.384
P12	inc5	del	PRN	0.002792	0.010002	0.279
P12	inc5	del	FC	0.002115	0.010329	0.205
P12	inc5	del	WMC	0.000090	0.000092	0.975
P12	inc5	ins	SEM	0.043567	0.032396	1.345
P12	inc5	ins	PRN	0.011396	0.035595	0.320
P12	inc5	ins	FC	0.009396	0.194827	0.048
P12	inc5	ins	WMC	0.000181	0.000504	0.360
P13	inc1	del	SEM	0.073189	0.048741	1.502
P13	inc1	del	PRN	0.018333	0.026157	0.701
P13	inc1	del	FC	0.420103	0.030454	13.795
P13	inc1	del	WMC	0.000783	0.000559	1.402
P13	inc1	ins	SEM	0.085161	0.025728	3.310
P13	inc1	ins	PRN	0.030869	0.053756	0.574
P13	inc1	ins	FC	0.263311	0.786047	0.335
P13	inc1	ins	WMC	0.000984	0.001084	0.907
P13	inc3	del	SEM	0.066443	0.123701	0.537
P13	inc3	del	PRN	0.009030	0.034420	0.262
P13	inc3	del	FC	0.007501	0.046314	0.162
P13	inc3	del	WMC	0.000196	0.000546	0.360
P13	inc3	ins	SEM	0.083246	0.057483	1.448
P13	inc3	ins	PRN	0.030007	0.114716	0.262
P13	inc3	ins	FC	0.250840	0.634293	0.395
P13	inc3	ins	WMC	0.000909	0.001375	0.661
P13	inc5	del	SEM	0.054103	0.156645	0.345
P13	inc5	del	PRN	0.005701	0.025152	0.227
P13	inc5	del	FC	0.004511	0.029408	0.153
P13	inc5	del	WMC	0.000190	0.000301	0.632
P13	inc5	ins	SEM	0.084723	0.060950	1.390
P13	inc5	ins	PRN	0.028773	0.105511	0.273
P13	inc5	ins	FC	0.259445	0.468270	0.554
P13	inc5	ins	WMC	0.001005	0.001191	0.844
P14	inc1	del	SEM	0.131847	0.070215	1.878
P14	inc1	del	PRN	0.027949	0.037071	0.754
P14	inc1	del	FC	0.361304	0.044308	8.154
P14	inc1	del	WMC	0.001248	0.000925	1.348
P14	inc1	ins	SEM	0.133311	0.041328	3.226
P14	inc1	ins	PRN	0.061691	0.107493	0.574
P14	inc1	ins	FC	0.650302	0.979779	0.664
P14	inc1	ins	WMC	0.001532	0.002148	0.714
P14	inc3	del	SEM	0.098265	0.162903	0.603
P14	inc3	del	PRN	0.011622	0.049437	0.235
P14	inc3	del	FC	0.010225	0.046177	0.221
P14	inc3	del	WMC	0.000317	0.000442	0.718
P14	inc3	ins	SEM	0.126148	0.066679	1.892
P14	inc3	ins	PRN	0.043166	0.188329	0.229
P14	inc3	ins	FC	0.674604	0.619036	1.090
P14	inc3	ins	WMC	0.001593	0.002074	0.768
P14	inc5	del	SEM	0.094283	0.247546	0.381
P14	inc5	del	PRN	0.005663	0.036425	0.155
P14	inc5	del	FC	0.003934	0.041300	0.095
P14	inc5	del	WMC	0.000198	0.000272	0.729
P14	inc5	ins	SEM	0.130085	0.089933	1.446
P14	inc5	ins	PRN	0.043543	0.237673	0.183
P14	inc5	ins	FC	0.656256	1.624767	0.404
P14	inc5	ins	WMC	0.001640	0.004742	0.346
P15	inc1	del	SEM	0.265276	0.181820	1.459
P15	inc1	del	PRN	0.042704	0.088364	0.483
P15	inc1	del	FC	1.035894	0.125312	8.266
P15	inc1	del	WMC	0.001907	0.001497	1.273
P15	inc1	ins	SEM	0.299944	0.102930	2.914
P15	inc1	ins	PRN	0.143370	0.528818	0.271
P15	inc1	ins	FC	2.886364	1.881244	1.534
P15	inc1	ins	WMC	0.003837	0.004690	0.818
P15	inc3	del	SEM	0.214627	0.387001	0.555
P15	inc3	del	PRN	0.007920	0.103887	0.076
P15	inc3	del	FC	0.003905	0.125685	0.031
P15	inc3	del	WMC	0.000387	0.000435	0.891
P15	inc3	ins	SEM	0.276546	0.182655	1.514
P15	inc3	ins	PRN	0.103266	1.183926	0.087
P15	inc3	ins	FC	2.479838	1.918898	1.292
P15	inc3	ins	WMC	0.003685	0.005747	0.641
P15	inc5	del	SEM	0.188237	0.504491	0.373
P15	inc5	del	PRN	0.005025	0.106019	0.047
P15	inc5	del	FC	0.001894	0.114260	0.017
P15	inc5	del	WMC	0.000275	0.000397	0.693
P15	inc5	ins	SEM	0.289823	0.240085	1.207
P15	inc5	ins	PRN	0.105463	1.211843	0.087
P15	inc5	ins	FC	2.756859	4.138369	0.666
P15	inc5	ins	WMC	0.004094	0.006378	0.642
P16	inc1	del	SEM	0.448604	0.319361	1.405
P16	inc1	del	PRN	0.043773	0.178591	0.245
P16	inc1	del	FC	0.981630	0.297144	3.304
P16	inc1	del	WMC	0.002219	0.002066	1.074
P16	inc1	ins	SEM	0.516224	0.214506	2.407
P16	inc1	ins	PRN	0.257347	2.148691	0.120
P16	inc1	ins	FC	4.820598	5.651540	0.853
P16	inc1	ins	WMC	0.008122	0.030165	0.269
P16	inc3	del	SEM	0.382780	0.642074	0.596
P16	inc3	del	PRN	0.014822	0.183324	0.081
P16	inc3	del	FC	0.007962	0.275286	0.029
P16	inc3	del	WMC	0.000620	0.000816	0.760
P16	inc3	ins	SEM	0.511636	0.290410	1.762
P16	inc3	ins	PRN	0.226865	2.982534	0.076
P16	inc3	ins	FC	4.481273	6.834669	0.656
P16	inc3	ins	WMC	0.010508	0.027037	0.389
P16	inc5	del	SEM	0.319658	0.820305	0.390
P16	inc5	del	PRN	0.010593	0.192163	0.055
P16	inc5	del	FC	0.003416	0.252758	0.014
P16	inc5	del	WMC	0.000582	0.000621	0.938
P16	inc5	ins	SEM	0.468832	0.406757	1.153
P16	inc5	ins	PRN	0.213918	3.413312	0.063
P16	inc5	ins	FC	4.428294	6.115048	0.724
P16	inc5	ins	WMC	0.007292	0.038237	0.191
P17	inc1	del	SEM	0.626876	0.418753	1.497
P17	inc1	del	PRN	0.067167	0.293134	0.229
P17	inc1	del	FC	1.969948	0.486125	4.052
P17	inc1	del	WMC	0.002486	0.002168	1.146
P17	inc1	ins	SEM	0.710549	0.280982	2.529
P17	inc1	ins	PRN	0.390586	4.041428	0.097
P17	inc1	ins	FC	5.489770	21.332053	0.257
P17	inc1	ins	WMC	0.011268	0.026059	0.432
P17	inc3	del	SEM	0.501872	0.925038	0.543
P17	inc3	del	PRN	0.033942	0.376620	0.090
P17	inc3	del	FC	0.009641	0.517190	0.019
P17	inc3	del	WMC	0.000908	0.001201	0.756
P17	inc3	ins	SEM	0.715911	0.443286	1.615
P17	inc3	ins	PRN	0.391614	6.188496	0.063
P17	inc3	ins	FC	5.939184	10.462206	0.568
P17	inc3	ins	WMC	0.013451	0.019264	0.698
P17	inc5	del	SEM	0.486997	1.099398	0.443
P17	inc5	del	PRN	0.014949	0.332723	0.045
P17	inc5	del	FC	0.006104	0.476938	0.013
P17	inc5	del	WMC	0.000983	0.000888	1.108
P17	inc5	ins	SEM	0.680069	0.499823	1.361
P17	inc5	ins	PRN	0.352535	6.030900	0.058
P17	inc5	ins	FC	5.566679	9.616531	0.579
P17	inc5	ins	WMC	0.013054	0.026102	0.500
P18	inc1	del	SEM	0.733412	0.568449	1.290
P18	inc1	del	PRN	0.087262	0.436956	0.200
P18	inc1	del	FC	1.734039	0.787984	2.201
P18	inc1	del	WMC	0.002796	0.002588	1.080
P18	inc1	ins	SEM	0.909348	0.379410	2.397
P18	inc1	ins	PRN	0.578991	5.848393	0.099
P18	inc1	ins	FC	6.172361	6.064649	1.018
P18	inc1	ins	WMC	0.025725	0.023147	1.111
P18	inc3	del	SEM	0.650327	1.043887	0.623
P18	inc3	del	PRN	0.045977	0.409682	0.112
P18	inc3	del	FC	0.013362	0.668973	0.020
P18	inc3	del	WMC	0.001283	0.001184	1.083
P18	inc3	ins	SEM	0.933957	0.509513	1.833
P18	inc3	ins	PRN	0.445710	9.352399	0.048
P18	inc3	ins	FC	6.311617	5.863587	1.076
P18	inc3	ins	WMC	0.019405	0.029761	0.652
P18	inc5	del	SEM	0.608425	1.499644	0.406
P18	inc5	del	PRN	0.028547	0.460682	0.062
P18	inc5	del	FC	0.004682	0.728560	0.006
P18	inc5	del	WMC	0.001073	0.001310	0.819
P18	inc5	ins	SEM	0.975995	0.655736	1.488
P18	inc5	ins	PRN	0.460848	9.380067	0.049
P18	inc5	ins	FC	6.087577	5.342382	1.139
P18	inc5	ins	WMC	0.024594	0.026350	0.933
P19	inc1	del	SEM	1.026818	0.734954	1.397
P19	inc1	del	PRN	0.084457	0.547215	0.154
P19	inc1	del	FC	1.923837	1.125574	1.709
P19	inc1	del	WMC	0.002656	0.002483	1.070
P19	inc1	ins	SEM	1.299704	0.520638	2.496
P19	inc1	ins	PRN	0.723166	11.505166	0.063
P19	inc1	ins	FC	8.531678	9.921401	0.860
P19	inc1	ins	WMC	0.028808	0.039102	0.737
P19	inc3	del	SEM	0.862460	1.423235	0.606
P19	inc3	del	PRN	0.036294	0.608072	0.060
P19	inc3	del	FC	0.007482	1.099164	0.007
P19	inc3	del	WMC	0.001276	0.001419	0.900
P19	inc3	ins	SEM	1.180612	0.732256	1.612
P19	inc3	ins	PRN	0.706058	16.272904	0.043
P19	inc3	ins	FC	8.218482	8.496291	0.967
P19	inc3	ins	WMC	0.026766	0.040130	0.667
P19	inc5	del	SEM	0.722430	1.994920	0.362
P19	inc5	del	PRN	0.022601	0.624696	0.036
P19	inc5	del	FC	0.002845	1.064087	0.003
P19	inc5	del	WMC	0.001025	0.001119	0.916
P19	inc5	ins	SEM	1.196871	0.891869	1.342
P19	inc5	ins	PRN	0.709198	17.142523	0.041
P19	inc5	ins	FC	7.958318	7.904848	1.007
P19	inc5	ins	WMC	0.027416	0.034485	0.795
P20	inc1	del	SEM	0.912271	0.519754	1.755
P20	inc1	del	PRN	0.556661	0.515498	1.080
P20	inc1	del	FC	1.828145	1.388467	1.317
P20	inc1	del	WMC	0.011049	0.011156	0.990
P20	inc1	ins	SEM	1.005359	0.322497	3.117
P20	inc1	ins	PRN	0.756983	1.002726	0.755
P20	inc1	ins	FC	3.603686	4.176005	0.863
P20	inc1	ins	WMC	0.016144	0.019817	0.815
P20	inc3	del	SEM	0.790884	1.009805	0.783
P20	inc3	del	PRN	0.220571	0.466603	0.473
P20	inc3	del	FC	0.584589	1.667500	0.351
P20	inc3	del	WMC	0.006059	0.006984	0.867
P20	inc3	ins	SEM	1.002361	0.480662	2.085
P20	inc3	ins	PRN	0.809172	1.593747	0.508
P20	inc3	ins	FC	4.899516	45.548539	0.108
P20	inc3	ins	WMC	0.014509	0.084085	0.173
P20	inc5	del	SEM	0.729993	1.524599	0.479
P20	inc5	del	PRN	0.110299	0.509505	0.216
P20	inc5	del	FC	0.350576	1.735462	0.202
P20	inc5	del	WMC	0.003673	0.003947	0.931
P20	inc5	ins	SEM	0.928317	0.636884	1.458
P20	inc5	ins	PRN	0.789117	2.403102	0.328
P20	inc5	ins	FC	4.675583	71.266423	0.066
P20	inc5	ins	WMC	0.024335	0.221421	0.110
```

#### 2026-01-11 (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_trimmed_eval.
```tsv
Case	Delta	Turn	Stage	Full_s	Inc_s	Speedup
P1	inc1	del	SEM	0.004743	0.002514	1.887
P1	inc1	del	PRN	0.000369	0.000337	1.094
P1	inc1	del	FC	0.000299	0.000136	2.205
P1	inc1	del	WMC	0.000028	0.000039	0.712
P1	inc1	ins	SEM	0.004392	0.000987	4.448
P1	inc1	ins	PRN	0.000416	0.000221	1.883
P1	inc1	ins	FC	0.000263	0.000105	2.512
P1	inc1	ins	WMC	0.000026	0.000030	0.882
P1	inc3	del	SEM	0.004344	0.000980	4.431
P1	inc3	del	PRN	0.000315	0.000243	1.295
P1	inc3	del	FC	0.000260	0.000110	2.357
P1	inc3	del	WMC	0.000026	0.000031	0.839
P1	inc3	ins	SEM	0.004562	0.001215	3.755
P1	inc3	ins	PRN	0.000304	0.000209	1.450
P1	inc3	ins	FC	0.000255	0.000102	2.486
P1	inc3	ins	WMC	0.000026	0.000029	0.910
P1	inc5	del	SEM	0.001337	0.019502	0.069
P1	inc5	del	PRN	0.000361	0.000263	1.370
P1	inc5	del	FC	0.000256	0.000104	2.449
P1	inc5	del	WMC	0.000026	0.000030	0.871
P1	inc5	ins	SEM	0.004464	0.008072	0.553
P1	inc5	ins	PRN	0.000356	0.000223	1.599
P1	inc5	ins	FC	0.000262	0.000104	2.512
P1	inc5	ins	WMC	0.000026	0.000029	0.905
P3	inc1	del	SEM	0.002458	0.019236	0.128
P3	inc1	del	PRN	0.000381	0.000232	1.637
P3	inc1	del	FC	0.000289	0.000111	2.598
P3	inc1	del	WMC	0.000030	0.000030	0.989
P3	inc1	ins	SEM	0.004907	0.006778	0.724
P3	inc1	ins	PRN	0.000400	0.000226	1.772
P3	inc1	ins	FC	0.000297	0.000106	2.797
P3	inc1	ins	WMC	0.000032	0.000029	1.079
P3	inc3	del	SEM	0.002005	0.018872	0.106
P3	inc3	del	PRN	0.000357	0.000259	1.380
P3	inc3	del	FC	0.000262	0.000114	2.303
P3	inc3	del	WMC	0.000027	0.000030	0.913
P3	inc3	ins	SEM	0.004519	0.006775	0.667
P3	inc3	ins	PRN	0.000354	0.000220	1.609
P3	inc3	ins	FC	0.000310	0.000104	2.983
P3	inc3	ins	WMC	0.000038	0.000029	1.301
P3	inc5	del	SEM	0.001783	0.019515	0.091
P3	inc5	del	PRN	0.000309	0.000278	1.112
P3	inc5	del	FC	0.000250	0.000106	2.349
P3	inc5	del	WMC	0.000026	0.000031	0.845
P3	inc5	ins	SEM	0.006603	0.007456	0.886
P3	inc5	ins	PRN	0.000431	0.000254	1.701
P3	inc5	ins	FC	0.000317	0.000132	2.404
P3	inc5	ins	WMC	0.000033	0.000030	1.092
P4	inc1	del	SEM	0.000242	0.002511	0.096
P4	inc1	del	PRN	0.000103	0.000213	0.486
P4	inc1	del	FC	0.000095	0.000046	2.067
P4	inc1	del	WMC	0.000024	0.000027	0.901
P4	inc1	ins	SEM	0.000261	0.001367	0.191
P4	inc1	ins	PRN	0.000126	0.000187	0.675
P4	inc1	ins	FC	0.000093	0.000039	2.398
P4	inc1	ins	WMC	0.000024	0.000025	0.970
P4	inc3	del	SEM	0.000228	0.001048	0.218
P4	inc3	del	PRN	0.000123	0.000152	0.810
P4	inc3	del	FC	0.000096	0.000041	2.321
P4	inc3	del	WMC	0.000024	0.000026	0.952
P4	inc3	ins	SEM	0.000344	0.000871	0.395
P4	inc3	ins	PRN	0.000169	0.000157	1.076
P4	inc3	ins	FC	0.000093	0.000044	2.096
P4	inc3	ins	WMC	0.000030	0.000027	1.106
P4	inc5	del	SEM	0.000234	0.000539	0.434
P4	inc5	del	PRN	0.000077	0.000144	0.530
P4	inc5	del	FC	0.000065	0.000108	0.598
P4	inc5	del	WMC	0.000023	0.000029	0.804
P4	inc5	ins	SEM	0.000234	0.002628	0.089
P4	inc5	ins	PRN	0.000132	0.000162	0.813
P4	inc5	ins	FC	0.000076	0.000092	0.824
P4	inc5	ins	WMC	0.000024	0.000026	0.913
P5	inc1	del	SEM	0.000298	0.001770	0.169
P5	inc1	del	PRN	0.000298	0.000330	0.905
P5	inc1	del	FC	0.000192	0.000091	2.095
P5	inc1	del	WMC	0.000026	0.000030	0.886
P5	inc1	ins	SEM	0.000299	0.001114	0.268
P5	inc1	ins	PRN	0.000240	0.000285	0.844
P5	inc1	ins	FC	0.000190	0.000100	1.910
P5	inc1	ins	WMC	0.000026	0.000039	0.675
P5	inc3	del	SEM	0.000295	0.001214	0.243
P5	inc3	del	PRN	0.000258	0.000226	1.141
P5	inc3	del	FC	0.000192	0.000083	2.305
P5	inc3	del	WMC	0.000026	0.000028	0.930
P5	inc3	ins	SEM	0.000395	0.002563	0.154
P5	inc3	ins	PRN	0.000271	0.000253	1.072
P5	inc3	ins	FC	0.000212	0.000086	2.479
P5	inc3	ins	WMC	0.000028	0.000033	0.847
P5	inc5	del	SEM	0.000311	0.002252	0.138
P5	inc5	del	PRN	0.000134	0.000264	0.507
P5	inc5	del	FC	0.000101	0.000922	0.109
P5	inc5	del	WMC	0.000027	0.000026	1.061
P5	inc5	ins	SEM	0.000355	0.001292	0.275
P5	inc5	ins	PRN	0.000280	0.000447	0.626
P5	inc5	ins	FC	0.000227	0.000279	0.813
P5	inc5	ins	WMC	0.000029	0.000027	1.059
P6	inc1	del	SEM	0.000494	0.001690	0.292
P6	inc1	del	PRN	0.000650	0.000606	1.072
P6	inc1	del	FC	0.000509	0.000274	1.859
P6	inc1	del	WMC	0.000031	0.000053	0.581
P6	inc1	ins	SEM	0.000417	0.001266	0.329
P6	inc1	ins	PRN	0.000601	0.000398	1.511
P6	inc1	ins	FC	0.000458	0.000211	2.172
P6	inc1	ins	WMC	0.000031	0.000041	0.758
P6	inc3	del	SEM	0.000387	0.001962	0.197
P6	inc3	del	PRN	0.000268	0.000338	0.792
P6	inc3	del	FC	0.000192	0.001026	0.187
P6	inc3	del	WMC	0.000029	0.000028	1.024
P6	inc3	ins	SEM	0.000516	0.001318	0.392
P6	inc3	ins	PRN	0.000813	0.000954	0.852
P6	inc3	ins	FC	0.000618	0.000912	0.678
P6	inc3	ins	WMC	0.000033	0.000035	0.961
P6	inc5	del	SEM	0.000388	0.001505	0.258
P6	inc5	del	PRN	0.000253	0.000443	0.572
P6	inc5	del	FC	0.000198	0.001191	0.167
P6	inc5	del	WMC	0.000027	0.000031	0.844
P6	inc5	ins	SEM	0.000491	0.001223	0.402
P6	inc5	ins	PRN	0.000643	0.001082	0.595
P6	inc5	ins	FC	0.000472	0.000918	0.514
P6	inc5	ins	WMC	0.000032	0.000033	0.966
P7	inc1	del	SEM	0.000918	0.001241	0.740
P7	inc1	del	PRN	0.000668	0.000675	0.990
P7	inc1	del	FC	0.000464	0.001454	0.319
P7	inc1	del	WMC	0.000032	0.000036	0.897
P7	inc1	ins	SEM	0.000986	0.001470	0.671
P7	inc1	ins	PRN	0.000818	0.000810	1.009
P7	inc1	ins	FC	0.000551	0.000581	0.947
P7	inc1	ins	WMC	0.000034	0.000036	0.960
P7	inc3	del	SEM	0.001008	0.001814	0.556
P7	inc3	del	PRN	0.000744	0.000979	0.760
P7	inc3	del	FC	0.000551	0.001455	0.379
P7	inc3	del	WMC	0.000035	0.000032	1.090
P7	inc3	ins	SEM	0.001194	0.001994	0.599
P7	inc3	ins	PRN	0.000891	0.001001	0.890
P7	inc3	ins	FC	0.000635	0.000866	0.734
P7	inc3	ins	WMC	0.000039	0.000040	0.974
P7	inc5	del	SEM	0.000777	0.004235	0.184
P7	inc5	del	PRN	0.000653	0.000747	0.874
P7	inc5	del	FC	0.000428	0.001643	0.260
P7	inc5	del	WMC	0.000096	0.000037	2.562
P7	inc5	ins	SEM	0.001042	0.002830	0.368
P7	inc5	ins	PRN	0.000894	0.001805	0.495
P7	inc5	ins	FC	0.000567	0.000775	0.731
P7	inc5	ins	WMC	0.000035	0.000037	0.950
P8	inc1	del	SEM	0.001619	0.001447	1.119
P8	inc1	del	PRN	0.001013	0.001118	0.906
P8	inc1	del	FC	0.000711	0.001419	0.501
P8	inc1	del	WMC	0.000036	0.000051	0.709
P8	inc1	ins	SEM	0.002586	0.001378	1.877
P8	inc1	ins	PRN	0.001020	0.001017	1.003
P8	inc1	ins	FC	0.000912	0.000844	1.080
P8	inc1	ins	WMC	0.000039	0.000044	0.890
P8	inc3	del	SEM	0.001140	0.001548	0.736
P8	inc3	del	PRN	0.000422	0.001299	0.325
P8	inc3	del	FC	0.000236	0.001995	0.118
P8	inc3	del	WMC	0.000029	0.000031	0.942
P8	inc3	ins	SEM	0.001791	0.001501	1.193
P8	inc3	ins	PRN	0.001258	0.002871	0.438
P8	inc3	ins	FC	0.000823	0.001603	0.514
P8	inc3	ins	WMC	0.000050	0.000052	0.970
P8	inc5	del	SEM	0.001054	0.002834	0.372
P8	inc5	del	PRN	0.000423	0.000981	0.431
P8	inc5	del	FC	0.000266	0.001346	0.198
P8	inc5	del	WMC	0.000033	0.000033	1.016
P8	inc5	ins	SEM	0.001975	0.002585	0.764
P8	inc5	ins	PRN	0.001412	0.002928	0.482
P8	inc5	ins	FC	0.000991	0.001978	0.501
P8	inc5	ins	WMC	0.000045	0.000046	0.983
P9	inc1	del	SEM	0.001463	0.001394	1.050
P9	inc1	del	PRN	0.001695	0.001171	1.447
P9	inc1	del	FC	0.001394	0.001676	0.831
P9	inc1	del	WMC	0.000048	0.000048	0.990
P9	inc1	ins	SEM	0.001239	0.001204	1.029
P9	inc1	ins	PRN	0.001670	0.001198	1.394
P9	inc1	ins	FC	0.001519	0.006548	0.232
P9	inc1	ins	WMC	0.000053	0.000072	0.745
P9	inc3	del	SEM	0.001117	0.002986	0.374
P9	inc3	del	PRN	0.001317	0.001125	1.171
P9	inc3	del	FC	0.001009	0.001664	0.607
P9	inc3	del	WMC	0.000044	0.000052	0.840
P9	inc3	ins	SEM	0.001157	0.001173	0.986
P9	inc3	ins	PRN	0.001570	0.001349	1.164
P9	inc3	ins	FC	0.001265	0.005920	0.214
P9	inc3	ins	WMC	0.000050	0.000080	0.622
P9	inc5	del	SEM	0.000989	0.001262	0.784
P9	inc5	del	PRN	0.000913	0.000961	0.950
P9	inc5	del	FC	0.000692	0.001559	0.444
P9	inc5	del	WMC	0.000038	0.000040	0.948
P9	inc5	ins	SEM	0.001137	0.001295	0.878
P9	inc5	ins	PRN	0.001351	0.003227	0.419
P9	inc5	ins	FC	0.001310	0.005178	0.253
P9	inc5	ins	WMC	0.000066	0.000069	0.945
P10	inc1	del	SEM	0.010677	0.006068	1.760
P10	inc1	del	PRN	0.000915	0.005915	0.155
P10	inc1	del	FC	0.000237	0.000129	1.838
P10	inc1	del	WMC	0.000030	0.000038	0.796
P10	inc1	ins	SEM	0.017543	0.005938	2.955
P10	inc1	ins	PRN	0.001321	0.006747	0.196
P10	inc1	ins	FC	0.000249	0.000110	2.261
P10	inc1	ins	WMC	0.000031	0.000036	0.872
P10	inc3	del	SEM	0.007039	0.007867	0.895
P10	inc3	del	PRN	0.000744	0.009184	0.081
P10	inc3	del	FC	0.000247	0.000123	2.016
P10	inc3	del	WMC	0.000051	0.000039	1.302
P10	inc3	ins	SEM	0.016966	0.010369	1.636
P10	inc3	ins	PRN	0.001867	0.011707	0.159
P10	inc3	ins	FC	0.000345	0.000114	3.023
P10	inc3	ins	WMC	0.000044	0.000037	1.203
P10	inc5	del	SEM	0.006235	0.008821	0.707
P10	inc5	del	PRN	0.000675	0.010858	0.062
P10	inc5	del	FC	0.000240	0.001264	0.190
P10	inc5	del	WMC	0.000034	0.000038	0.883
P10	inc5	ins	SEM	0.014899	0.009525	1.564
P10	inc5	ins	PRN	0.000941	0.014934	0.063
P10	inc5	ins	FC	0.000267	0.000232	1.154
P10	inc5	ins	WMC	0.000035	0.000034	1.020
P11	inc1	del	SEM	0.011549	0.007544	1.531
P11	inc1	del	PRN	0.000917	0.007937	0.116
P11	inc1	del	FC	0.000257	0.001005	0.256
P11	inc1	del	WMC	0.000040	0.000034	1.177
P11	inc1	ins	SEM	0.021025	0.006399	3.286
P11	inc1	ins	PRN	0.001272	0.009786	0.130
P11	inc1	ins	FC	0.000244	0.000223	1.095
P11	inc1	ins	WMC	0.000040	0.000034	1.182
P11	inc3	del	SEM	0.006676	0.007554	0.884
P11	inc3	del	PRN	0.000779	0.010027	0.078
P11	inc3	del	FC	0.000189	0.001366	0.138
P11	inc3	del	WMC	0.000032	0.000043	0.733
P11	inc3	ins	SEM	0.019375	0.007536	2.571
P11	inc3	ins	PRN	0.001012	0.014279	0.071
P11	inc3	ins	FC	0.000193	0.000168	1.151
P11	inc3	ins	WMC	0.000032	0.000032	1.002
P11	inc5	del	SEM	0.006796	0.008546	0.795
P11	inc5	del	PRN	0.000864	0.012575	0.069
P11	inc5	del	FC	0.000267	0.001161	0.230
P11	inc5	del	WMC	0.000034	0.000034	0.998
P11	inc5	ins	SEM	0.019100	0.008368	2.282
P11	inc5	ins	PRN	0.000743	0.016355	0.045
P11	inc5	ins	FC	0.000190	0.000169	1.122
P11	inc5	ins	WMC	0.000031	0.000048	0.654
P12	inc1	del	SEM	0.013257	0.004136	3.205
P12	inc1	del	PRN	0.009784	0.011335	0.863
P12	inc1	del	FC	0.008031	0.010602	0.758
P12	inc1	del	WMC	0.000214	0.000246	0.870
P12	inc1	ins	SEM	0.012471	0.003676	3.393
P12	inc1	ins	PRN	0.010673	0.015086	0.707
P12	inc1	ins	FC	0.010154	0.114725	0.089
P12	inc1	ins	WMC	0.000202	0.000395	0.511
P12	inc3	del	SEM	0.011190	0.005048	2.217
P12	inc3	del	PRN	0.006257	0.010955	0.571
P12	inc3	del	FC	0.005635	0.011061	0.509
P12	inc3	del	WMC	0.000151	0.000171	0.884
P12	inc3	ins	SEM	0.018049	0.004250	4.247
P12	inc3	ins	PRN	0.012604	0.026539	0.475
P12	inc3	ins	FC	0.010267	0.297234	0.035
P12	inc3	ins	WMC	0.000238	0.000574	0.415
P12	inc5	del	SEM	0.009671	0.006657	1.453
P12	inc5	del	PRN	0.003436	0.010201	0.337
P12	inc5	del	FC	0.003174	0.009762	0.325
P12	inc5	del	WMC	0.000148	0.000175	0.846
P12	inc5	ins	SEM	0.017568	0.005519	3.183
P12	inc5	ins	PRN	0.014044	0.036330	0.387
P12	inc5	ins	FC	0.011382	0.113785	0.100
P12	inc5	ins	WMC	0.000263	0.000440	0.598
P13	inc1	del	SEM	0.033115	0.007896	4.194
P13	inc1	del	PRN	0.022676	0.030783	0.737
P13	inc1	del	FC	0.021519	0.032631	0.659
P13	inc1	del	WMC	0.000543	0.000539	1.006
P13	inc1	ins	SEM	0.039622	0.006199	6.391
P13	inc1	ins	PRN	0.038151	0.049646	0.768
P13	inc1	ins	FC	0.299594	1.002941	0.299
P13	inc1	ins	WMC	0.001103	0.001234	0.894
P13	inc3	del	SEM	0.020484	0.010539	1.944
P13	inc3	del	PRN	0.008394	0.020177	0.416
P13	inc3	del	FC	0.006386	0.032708	0.195
P13	inc3	del	WMC	0.000179	0.000258	0.693
P13	inc3	ins	SEM	0.033269	0.009800	3.395
P13	inc3	ins	PRN	0.030722	0.128930	0.238
P13	inc3	ins	FC	0.313203	0.624284	0.502
P13	inc3	ins	WMC	0.000951	0.001475	0.645
P13	inc5	del	SEM	0.012205	0.012813	0.953
P13	inc5	del	PRN	0.001951	0.027524	0.071
P13	inc5	del	FC	0.001648	0.029974	0.055
P13	inc5	del	WMC	0.000118	0.000187	0.631
P13	inc5	ins	SEM	0.035694	0.010787	3.309
P13	inc5	ins	PRN	0.034870	0.156700	0.223
P13	inc5	ins	FC	0.290969	0.480505	0.606
P13	inc5	ins	WMC	0.001189	0.001317	0.903
P14	inc1	del	SEM	0.038203	0.013184	2.898
P14	inc1	del	PRN	0.027275	0.044648	0.611
P14	inc1	del	FC	0.366106	0.048809	7.501
P14	inc1	del	WMC	0.001006	0.000963	1.044
P14	inc1	ins	SEM	0.062351	0.011808	5.280
P14	inc1	ins	PRN	0.053740	0.094904	0.566
P14	inc1	ins	FC	0.707355	0.893842	0.791
P14	inc1	ins	WMC	0.001722	0.002587	0.666
P14	inc3	del	SEM	0.019084	0.023567	0.810
P14	inc3	del	PRN	0.006416	0.056724	0.113
P14	inc3	del	FC	0.005890	0.051542	0.114
P14	inc3	del	WMC	0.000199	0.000431	0.462
P14	inc3	ins	SEM	0.039272	0.019502	2.014
P14	inc3	ins	PRN	0.053542	0.277527	0.193
P14	inc3	ins	FC	0.752616	0.641910	1.172
P14	inc3	ins	WMC	0.002127	0.002038	1.044
P14	inc5	del	SEM	0.017785	0.017414	1.021
P14	inc5	del	PRN	0.002776	0.042512	0.065
P14	inc5	del	FC	0.001914	0.040729	0.047
P14	inc5	del	WMC	0.000152	0.000248	0.611
P14	inc5	ins	SEM	0.048149	0.016584	2.903
P14	inc5	ins	PRN	0.049662	0.311765	0.159
P14	inc5	ins	FC	0.708168	0.669599	1.058
P14	inc5	ins	WMC	0.001486	0.001821	0.816
P15	inc1	del	SEM	0.083113	0.035372	2.350
P15	inc1	del	PRN	0.066795	0.108728	0.614
P15	inc1	del	FC	2.083023	0.144549	14.410
P15	inc1	del	WMC	0.002508	0.001790	1.401
P15	inc1	ins	SEM	0.119745	0.043150	2.775
P15	inc1	ins	PRN	0.129767	0.575156	0.226
P15	inc1	ins	FC	2.966588	2.471038	1.201
P15	inc1	ins	WMC	0.004415	0.006818	0.647
P15	inc3	del	SEM	0.064004	0.043043	1.487
P15	inc3	del	PRN	0.017535	0.101780	0.172
P15	inc3	del	FC	0.609360	0.137365	4.436
P15	inc3	del	WMC	0.000747	0.000709	1.053
P15	inc3	ins	SEM	0.095203	0.046392	2.052
P15	inc3	ins	PRN	0.098795	1.028131	0.096
P15	inc3	ins	FC	2.709845	2.540056	1.067
P15	inc3	ins	WMC	0.004654	0.012475	0.373
P15	inc5	del	SEM	0.045485	0.045516	0.999
P15	inc5	del	PRN	0.003952	0.102680	0.038
P15	inc5	del	FC	0.001533	0.132330	0.012
P15	inc5	del	WMC	0.000273	0.000457	0.597
P15	inc5	ins	SEM	0.113216	0.051747	2.188
P15	inc5	ins	PRN	0.140155	1.615025	0.087
P15	inc5	ins	FC	2.709029	2.501964	1.083
P15	inc5	ins	WMC	0.004194	0.010008	0.419
P16	inc1	del	SEM	0.147153	0.072585	2.027
P16	inc1	del	PRN	0.055436	0.268839	0.206
P16	inc1	del	FC	1.367697	0.388867	3.517
P16	inc1	del	WMC	0.005298	0.002599	2.039
P16	inc1	ins	SEM	0.259013	0.076654	3.379
P16	inc1	ins	PRN	0.297764	2.447215	0.122
P16	inc1	ins	FC	4.671168	5.687138	0.821
P16	inc1	ins	WMC	0.007662	0.013171	0.582
P16	inc3	del	SEM	0.090289	0.075636	1.194
P16	inc3	del	PRN	0.020576	0.259006	0.079
P16	inc3	del	FC	0.008849	0.317868	0.028
P16	inc3	del	WMC	0.000591	0.001043	0.567
P16	inc3	ins	SEM	0.217355	0.097046	2.240
P16	inc3	ins	PRN	0.332535	3.806804	0.087
P16	inc3	ins	FC	4.995322	5.806584	0.860
P16	inc3	ins	WMC	0.008318	0.026237	0.317
P16	inc5	del	SEM	0.068150	0.083557	0.816
P16	inc5	del	PRN	0.007381	0.260887	0.028
P16	inc5	del	FC	0.003727	0.303080	0.012
P16	inc5	del	WMC	0.000525	0.000744	0.706
P16	inc5	ins	SEM	0.216881	0.101574	2.135
P16	inc5	ins	PRN	0.291024	4.163579	0.070
P16	inc5	ins	FC	4.973518	7.161513	0.694
P16	inc5	ins	WMC	0.010122	0.036475	0.278
P17	inc1	del	SEM	0.175295	0.099465	1.762
P17	inc1	del	PRN	0.047893	0.307436	0.156
P17	inc1	del	FC	2.244418	0.523704	4.286
P17	inc1	del	WMC	0.002430	0.001895	1.282
P17	inc1	ins	SEM	0.350136	0.123004	2.847
P17	inc1	ins	PRN	0.464461	4.269479	0.109
P17	inc1	ins	FC	6.168977	12.331960	0.500
P17	inc1	ins	WMC	0.017274	0.020594	0.839
P17	inc3	del	SEM	0.134602	0.123260	1.092
P17	inc3	del	PRN	0.023759	0.366868	0.065
P17	inc3	del	FC	0.007649	0.506080	0.015
P17	inc3	del	WMC	0.001329	0.001061	1.253
P17	inc3	ins	SEM	0.303583	0.136419	2.225
P17	inc3	ins	PRN	0.401953	6.999991	0.057
P17	inc3	ins	FC	5.941497	11.979178	0.496
P17	inc3	ins	WMC	0.015410	0.021889	0.704
P17	inc5	del	SEM	0.119744	0.133294	0.898
P17	inc5	del	PRN	0.019624	0.331619	0.059
P17	inc5	del	FC	0.003607	0.485432	0.007
P17	inc5	del	WMC	0.000944	0.001444	0.654
P17	inc5	ins	SEM	0.310351	0.137578	2.256
P17	inc5	ins	PRN	0.482732	6.490261	0.074
P17	inc5	ins	FC	6.213848	11.159653	0.557
P17	inc5	ins	WMC	0.016887	0.027611	0.612
P18	inc1	del	SEM	0.271961	0.165667	1.642
P18	inc1	del	PRN	0.089511	0.426342	0.210
P18	inc1	del	FC	2.155408	0.772158	2.791
P18	inc1	del	WMC	0.003407	0.003007	1.133
P18	inc1	ins	SEM	0.389432	0.162323	2.399
P18	inc1	ins	PRN	0.623117	6.726006	0.093
P18	inc1	ins	FC	7.184263	7.566518	0.949
P18	inc1	ins	WMC	0.039519	0.030034	1.316
P18	inc3	del	SEM	0.151291	0.171126	0.884
P18	inc3	del	PRN	0.021694	0.518955	0.042
P18	inc3	del	FC	0.010664	0.798797	0.013
P18	inc3	del	WMC	0.001093	0.001445	0.757
P18	inc3	ins	SEM	0.462724	0.167023	2.770
P18	inc3	ins	PRN	0.695035	9.783647	0.071
P18	inc3	ins	FC	7.386186	6.486962	1.139
P18	inc3	ins	WMC	0.021988	0.034841	0.631
P18	inc5	del	SEM	0.138465	0.163466	0.847
P18	inc5	del	PRN	0.023639	0.595577	0.040
P18	inc5	del	FC	0.004488	0.768733	0.006
P18	inc5	del	WMC	0.001174	0.001243	0.944
P18	inc5	ins	SEM	0.420255	0.183545	2.290
P18	inc5	ins	PRN	0.603728	11.534662	0.052
P18	inc5	ins	FC	7.042728	6.311089	1.116
P18	inc5	ins	WMC	0.025766	0.029918	0.861
P19	inc1	del	SEM	0.355481	0.229725	1.547
P19	inc1	del	PRN	0.107808	0.684836	0.157
P19	inc1	del	FC	2.019279	1.267118	1.594
P19	inc1	del	WMC	0.003721	0.003192	1.166
P19	inc1	ins	SEM	0.548455	0.270420	2.028
P19	inc1	ins	PRN	0.629839	13.296178	0.047
P19	inc1	ins	FC	8.450660	8.836306	0.956
P19	inc1	ins	WMC	0.029647	0.039592	0.749
P19	inc3	del	SEM	0.198553	0.228191	0.870
P19	inc3	del	PRN	0.030561	0.612878	0.050
P19	inc3	del	FC	0.006527	1.111915	0.006
P19	inc3	del	WMC	0.001357	0.001760	0.771
P19	inc3	ins	SEM	0.502165	0.266115	1.887
P19	inc3	ins	PRN	0.625268	19.085240	0.033
P19	inc3	ins	FC	8.534822	13.732038	0.622
P19	inc3	ins	WMC	0.033840	0.049560	0.683
P19	inc5	del	SEM	0.212476	0.193918	1.096
P19	inc5	del	PRN	0.023198	0.590973	0.039
P19	inc5	del	FC	0.003354	1.072419	0.003
P19	inc5	del	WMC	0.001135	0.001137	0.997
P19	inc5	ins	SEM	0.488407	0.294687	1.657
P19	inc5	ins	PRN	0.658232	17.691969	0.037
P19	inc5	ins	FC	9.390675	8.042524	1.168
P19	inc5	ins	WMC	0.044130	0.048403	0.912
P20	inc1	del	SEM	0.408499	0.127433	3.206
P20	inc1	del	PRN	0.484674	0.880933	0.550
P20	inc1	del	FC	2.217454	1.871211	1.185
P20	inc1	del	WMC	0.011718	0.010987	1.067
P20	inc1	ins	SEM	0.446217	0.122291	3.649
P20	inc1	ins	PRN	1.014775	1.209817	0.839
P20	inc1	ins	FC	4.213063	9.741141	0.433
P20	inc1	ins	WMC	0.017206	0.023041	0.747
P20	inc3	del	SEM	0.598049	0.145318	4.115
P20	inc3	del	PRN	0.495207	0.520207	0.952
P20	inc3	del	FC	1.214744	1.895808	0.641
P20	inc3	del	WMC	0.019877	0.008427	2.359
P20	inc3	ins	SEM	0.610870	0.129668	4.711
P20	inc3	ins	PRN	1.045462	2.081397	0.502
P20	inc3	ins	FC	5.619336	57.960980	0.097
P20	inc3	ins	WMC	0.020905	0.094957	0.220
P20	inc5	del	SEM	0.303287	0.198381	1.529
P20	inc5	del	PRN	0.106903	0.626383	0.171
P20	inc5	del	FC	0.338001	1.817147	0.186
P20	inc5	del	WMC	0.004109	0.005564	0.739
P20	inc5	ins	SEM	0.566586	0.191767	2.955
P20	inc5	ins	PRN	1.132399	3.162009	0.358
P20	inc5	ins	FC	5.669214	53.320787	0.106
P20	inc5	ins	WMC	0.018299	0.071998	0.254
```

Legacy per-stage tables below are pre apply_delta_graph (kept for audit only).

#### 2026-01-11 (det-opt run)
Legacy run: 2026-01-11, det-opt, pre apply_delta_graph; stage breakdown from debugger JSON.
```tsv
Case	Delta	Turn	Stage	Full_s	Inc_s	Speedup
P1	inc1	del	SEM	0.004251	0.001509	2.818
P1	inc1	del	PRN	0.001056	0.000327	3.232
P1	inc1	del	FC	0.000284	0.000134	2.122
P1	inc1	del	WMC	0.000026	0.000039	0.661
P1	inc1	ins	SEM	0.004478	0.005950	0.753
P1	inc1	ins	PRN	0.000318	0.000321	0.993
P1	inc1	ins	FC	0.000250	0.000127	1.969
P1	inc1	ins	WMC	0.000025	0.000036	0.698
P1	inc3	del	SEM	0.003155	0.007915	0.399
P1	inc3	del	PRN	0.000298	0.000252	1.186
P1	inc3	del	FC	0.000251	0.000114	2.204
P1	inc3	del	WMC	0.000056	0.000031	1.780
P1	inc3	ins	SEM	0.004452	0.001304	3.413
P1	inc3	ins	PRN	0.001114	0.000235	4.733
P1	inc3	ins	FC	0.000250	0.000105	2.394
P1	inc3	ins	WMC	0.000026	0.000030	0.866
P1	inc5	del	SEM	0.001473	0.010097	0.146
P1	inc5	del	PRN	0.000325	0.000258	1.258
P1	inc5	del	FC	0.000252	0.000104	2.417
P1	inc5	del	WMC	0.000026	0.000030	0.861
P1	inc5	ins	SEM	0.004766	0.002672	1.784
P1	inc5	ins	PRN	0.000318	0.000313	1.015
P1	inc5	ins	FC	0.000250	0.000127	1.978
P1	inc5	ins	WMC	0.000026	0.000037	0.698
P3	inc1	del	SEM	0.002297	0.015203	0.151
P3	inc1	del	PRN	0.001401	0.000571	2.453
P3	inc1	del	FC	0.000299	0.000889	0.337
P3	inc1	del	WMC	0.000026	0.000269	0.098
P3	inc1	ins	SEM	0.004700	0.003420	1.374
P3	inc1	ins	PRN	0.000312	0.000281	1.110
P3	inc1	ins	FC	0.000259	0.000133	1.950
P3	inc1	ins	WMC	0.000028	0.000038	0.724
P3	inc3	del	SEM	0.002324	0.011148	0.208
P3	inc3	del	PRN	0.000222	0.000728	0.305
P3	inc3	del	FC	0.000175	0.000348	0.502
P3	inc3	del	WMC	0.000025	0.000033	0.778
P3	inc3	ins	SEM	0.004608	0.001191	3.870
P3	inc3	ins	PRN	0.000333	0.000429	0.776
P3	inc3	ins	FC	0.000250	0.000450	0.556
P3	inc3	ins	WMC	0.000026	0.000027	0.986
P3	inc5	del	SEM	0.001052	0.009461	0.111
P3	inc5	del	PRN	0.000217	0.000449	0.483
P3	inc5	del	FC	0.000177	0.000266	0.663
P3	inc5	del	WMC	0.000025	0.000039	0.642
P3	inc5	ins	SEM	0.004785	0.002811	1.702
P3	inc5	ins	PRN	0.000293	0.000563	0.520
P3	inc5	ins	FC	0.000247	0.000356	0.695
P3	inc5	ins	WMC	0.000026	0.000034	0.771
P4	inc1	del	SEM	0.000302	0.004944	0.061
P4	inc1	del	PRN	0.001040	0.000898	1.158
P4	inc1	del	FC	0.000098	0.000636	0.154
P4	inc1	del	WMC	0.000169	0.000078	2.155
P4	inc1	ins	SEM	0.000442	0.001038	0.426
P4	inc1	ins	PRN	0.000094	0.000136	0.691
P4	inc1	ins	FC	0.000075	0.000038	2.004
P4	inc1	ins	WMC	0.000023	0.000024	0.960
P4	inc3	del	SEM	0.000312	0.002161	0.145
P4	inc3	del	PRN	0.000103	0.000154	0.668
P4	inc3	del	FC	0.000095	0.000054	1.764
P4	inc3	del	WMC	0.000025	0.000031	0.799
P4	inc3	ins	SEM	0.000325	0.002006	0.162
P4	inc3	ins	PRN	0.000097	0.000119	0.812
P4	inc3	ins	FC	0.000046	0.000037	1.243
P4	inc3	ins	WMC	0.000023	0.000025	0.934
P4	inc5	del	SEM	0.000392	0.001742	0.225
P4	inc5	del	PRN	0.000122	0.000161	0.759
P4	inc5	del	FC	0.000095	0.000045	2.098
P4	inc5	del	WMC	0.000024	0.000031	0.792
P4	inc5	ins	SEM	0.000307	0.002253	0.136
P4	inc5	ins	PRN	0.000112	0.000099	1.132
P4	inc5	ins	FC	0.000096	0.000036	2.695
P4	inc5	ins	WMC	0.000024	0.000026	0.925
P5	inc1	del	SEM	0.000242	0.006048	0.040
P5	inc1	del	PRN	0.000529	0.001350	0.392
P5	inc1	del	FC	0.000192	0.000231	0.830
P5	inc1	del	WMC	0.000066	0.000129	0.508
P5	inc1	ins	SEM	0.000302	0.003256	0.093
P5	inc1	ins	PRN	0.000250	0.000255	0.979
P5	inc1	ins	FC	0.000188	0.000081	2.308
P5	inc1	ins	WMC	0.000026	0.000061	0.423
P5	inc3	del	SEM	0.000261	0.001235	0.211
P5	inc3	del	PRN	0.000241	0.000258	0.935
P5	inc3	del	FC	0.000188	0.000091	2.065
P5	inc3	del	WMC	0.000025	0.000030	0.858
P5	inc3	ins	SEM	0.000304	0.002696	0.113
P5	inc3	ins	PRN	0.000234	0.000251	0.935
P5	inc3	ins	FC	0.000186	0.000080	2.328
P5	inc3	ins	WMC	0.000025	0.000029	0.879
P5	inc5	del	SEM	0.000204	0.001952	0.105
P5	inc5	del	PRN	0.000106	0.000218	0.485
P5	inc5	del	FC	0.000078	0.001202	0.065
P5	inc5	del	WMC	0.000024	0.000025	0.955
P5	inc5	ins	SEM	0.000320	0.003192	0.100
P5	inc5	ins	PRN	0.000261	0.000486	0.537
P5	inc5	ins	FC	0.000190	0.000747	0.255
P5	inc5	ins	WMC	0.000026	0.000034	0.781
P6	inc1	del	SEM	0.000831	0.013218	0.063
P6	inc1	del	PRN	0.000972	0.000760	1.277
P6	inc1	del	FC	0.000673	0.001353	0.497
P6	inc1	del	WMC	0.000040	0.000094	0.425
P6	inc1	ins	SEM	0.000879	0.004374	0.201
P6	inc1	ins	PRN	0.000817	0.000517	1.581
P6	inc1	ins	FC	0.000608	0.000442	1.376
P6	inc1	ins	WMC	0.000039	0.000032	1.243
P6	inc3	del	SEM	0.000724	0.012503	0.058
P6	inc3	del	PRN	0.000477	0.000461	1.036
P6	inc3	del	FC	0.000331	0.001264	0.262
P6	inc3	del	WMC	0.000029	0.000035	0.821
P6	inc3	ins	SEM	0.000723	0.008265	0.088
P6	inc3	ins	PRN	0.000652	0.000908	0.718
P6	inc3	ins	FC	0.000475	0.001100	0.432
P6	inc3	ins	WMC	0.000031	0.000036	0.859
P6	inc5	del	SEM	0.000676	0.004099	0.165
P6	inc5	del	PRN	0.000411	0.000416	0.989
P6	inc5	del	FC	0.000294	0.001221	0.241
P6	inc5	del	WMC	0.000028	0.000036	0.778
P6	inc5	ins	SEM	0.000741	0.005760	0.129
P6	inc5	ins	PRN	0.000627	0.001006	0.624
P6	inc5	ins	FC	0.000525	0.000613	0.856
P6	inc5	ins	WMC	0.000031	0.000032	0.970
P7	inc1	del	SEM	0.002124	0.014671	0.145
P7	inc1	del	PRN	0.001384	0.002207	0.627
P7	inc1	del	FC	0.000768	0.003133	0.245
P7	inc1	del	WMC	0.000044	0.000383	0.116
P7	inc1	ins	SEM	0.002021	0.004589	0.440
P7	inc1	ins	PRN	0.000903	0.000794	1.137
P7	inc1	ins	FC	0.000716	0.000606	1.183
P7	inc1	ins	WMC	0.000041	0.000041	1.009
P7	inc3	del	SEM	0.002022	0.004234	0.477
P7	inc3	del	PRN	0.000851	0.000781	1.089
P7	inc3	del	FC	0.000639	0.001256	0.509
P7	inc3	del	WMC	0.000047	0.000032	1.449
P7	inc3	ins	SEM	0.002694	0.007001	0.385
P7	inc3	ins	PRN	0.000908	0.000869	1.044
P7	inc3	ins	FC	0.000648	0.000769	0.842
P7	inc3	ins	WMC	0.000039	0.000038	1.009
P7	inc5	del	SEM	0.001698	0.002656	0.639
P7	inc5	del	PRN	0.000661	0.000664	0.996
P7	inc5	del	FC	0.000389	0.001331	0.292
P7	inc5	del	WMC	0.000032	0.000034	0.954
P7	inc5	ins	SEM	0.002232	0.001980	1.127
P7	inc5	ins	PRN	0.000908	0.001262	0.720
P7	inc5	ins	FC	0.000723	0.001015	0.712
P7	inc5	ins	WMC	0.000053	0.000038	1.401
P8	inc1	del	SEM	0.003797	0.022280	0.170
P8	inc1	del	PRN	0.001693	0.001837	0.922
P8	inc1	del	FC	0.000862	0.004643	0.186
P8	inc1	del	WMC	0.000088	0.000337	0.261
P8	inc1	ins	SEM	0.004016	0.007588	0.529
P8	inc1	ins	PRN	0.001236	0.001073	1.152
P8	inc1	ins	FC	0.000899	0.000816	1.102
P8	inc1	ins	WMC	0.000042	0.000042	1.017
P8	inc3	del	SEM	0.003454	0.007939	0.435
P8	inc3	del	PRN	0.000847	0.001043	0.812
P8	inc3	del	FC	0.000521	0.001385	0.376
P8	inc3	del	WMC	0.000033	0.000037	0.892
P8	inc3	ins	SEM	0.003848	0.006126	0.628
P8	inc3	ins	PRN	0.001239	0.002022	0.613
P8	inc3	ins	FC	0.000854	0.001257	0.680
P8	inc3	ins	WMC	0.000043	0.000049	0.864
P8	inc5	del	SEM	0.004138	0.006612	0.626
P8	inc5	del	PRN	0.000754	0.001019	0.740
P8	inc5	del	FC	0.000470	0.001382	0.340
P8	inc5	del	WMC	0.000038	0.000040	0.944
P8	inc5	ins	SEM	0.004168	0.002592	1.608
P8	inc5	ins	PRN	0.001271	0.002376	0.535
P8	inc5	ins	FC	0.000867	0.001417	0.612
P8	inc5	ins	WMC	0.000061	0.000044	1.363
P9	inc1	del	SEM	0.002218	0.018669	0.119
P9	inc1	del	PRN	0.001618	0.001591	1.017
P9	inc1	del	FC	0.000777	0.001935	0.402
P9	inc1	del	WMC	0.000039	0.001065	0.037
P9	inc1	ins	SEM	0.002604	0.007336	0.355
P9	inc1	ins	PRN	0.001544	0.002033	0.760
P9	inc1	ins	FC	0.001332	0.008972	0.148
P9	inc1	ins	WMC	0.000054	0.000063	0.852
P9	inc3	del	SEM	0.002041	0.010515	0.194
P9	inc3	del	PRN	0.000773	0.000945	0.818
P9	inc3	del	FC	0.000573	0.001605	0.357
P9	inc3	del	WMC	0.000036	0.000042	0.851
P9	inc3	ins	SEM	0.002537	0.005795	0.438
P9	inc3	ins	PRN	0.001760	0.002996	0.587
P9	inc3	ins	FC	0.001443	0.004831	0.299
P9	inc3	ins	WMC	0.000052	0.000058	0.903
P9	inc5	del	SEM	0.002015	0.006511	0.309
P9	inc5	del	PRN	0.000728	0.001227	0.593
P9	inc5	del	FC	0.000533	0.001609	0.331
P9	inc5	del	WMC	0.000036	0.000038	0.939
P9	inc5	ins	SEM	0.002585	0.003643	0.710
P9	inc5	ins	PRN	0.001606	0.002954	0.544
P9	inc5	ins	FC	0.001320	0.005869	0.225
P9	inc5	ins	WMC	0.000052	0.000054	0.963
P10	inc1	del	SEM	0.020981	0.020477	1.025
P10	inc1	del	PRN	0.001569	0.009690	0.162
P10	inc1	del	FC	0.000406	0.002997	0.135
P10	inc1	del	WMC	0.000043	0.000403	0.107
P10	inc1	ins	SEM	0.026745	0.013681	1.955
P10	inc1	ins	PRN	0.001428	0.009592	0.149
P10	inc1	ins	FC	0.000404	0.000355	1.137
P10	inc1	ins	WMC	0.000045	0.000040	1.139
P10	inc3	del	SEM	0.016264	0.013988	1.163
P10	inc3	del	PRN	0.000990	0.012326	0.080
P10	inc3	del	FC	0.000365	0.001422	0.256
P10	inc3	del	WMC	0.000042	0.000040	1.050
P10	inc3	ins	SEM	0.026539	0.017276	1.536
P10	inc3	ins	PRN	0.001277	0.014502	0.088
P10	inc3	ins	FC	0.000369	0.003135	0.118
P10	inc3	ins	WMC	0.000037	0.000042	0.861
P10	inc5	del	SEM	0.016651	0.017872	0.932
P10	inc5	del	PRN	0.001077	0.013596	0.079
P10	inc5	del	FC	0.000834	0.001712	0.487
P10	inc5	del	WMC	0.000049	0.000038	1.277
P10	inc5	ins	SEM	0.027808	0.014015	1.984
P10	inc5	ins	PRN	0.001504	0.013896	0.108
P10	inc5	ins	FC	0.000399	0.003697	0.108
P10	inc5	ins	WMC	0.000040	0.000042	0.964
P11	inc1	del	SEM	0.020579	0.018087	1.138
P11	inc1	del	PRN	0.000971	0.007292	0.133
P11	inc1	del	FC	0.000202	0.000166	1.214
P11	inc1	del	WMC	0.000085	0.000325	0.261
P11	inc1	ins	SEM	0.031059	0.011481	2.705
P11	inc1	ins	PRN	0.001466	0.007574	0.194
P11	inc1	ins	FC	0.000243	0.000084	2.889
P11	inc1	ins	WMC	0.000039	0.000049	0.798
P11	inc3	del	SEM	0.018832	0.016069	1.172
P11	inc3	del	PRN	0.000975	0.013496	0.072
P11	inc3	del	FC	0.000232	0.002427	0.096
P11	inc3	del	WMC	0.000041	0.000036	1.156
P11	inc3	ins	SEM	0.028418	0.014512	1.958
P11	inc3	ins	PRN	0.001272	0.013187	0.096
P11	inc3	ins	FC	0.000212	0.000261	0.812
P11	inc3	ins	WMC	0.000032	0.000032	1.000
P11	inc5	del	SEM	0.015537	0.016590	0.937
P11	inc5	del	PRN	0.000726	0.011315	0.064
P11	inc5	del	FC	0.000183	0.001472	0.124
P11	inc5	del	WMC	0.000032	0.000044	0.731
P11	inc5	ins	SEM	0.026677	0.013775	1.937
P11	inc5	ins	PRN	0.000857	0.015912	0.054
P11	inc5	ins	FC	0.000199	0.000215	0.923
P11	inc5	ins	WMC	0.000031	0.000037	0.849
P12	inc1	del	SEM	0.049613	0.024790	2.001
P12	inc1	del	PRN	0.010173	0.010169	1.000
P12	inc1	del	FC	0.007996	0.010451	0.765
P12	inc1	del	WMC	0.000286	0.000344	0.831
P12	inc1	ins	SEM	0.045153	0.011840	3.814
P12	inc1	ins	PRN	0.012070	0.011903	1.014
P12	inc1	ins	FC	0.009328	0.213302	0.044
P12	inc1	ins	WMC	0.000223	0.000361	0.618
P12	inc3	del	SEM	0.037833	0.029860	1.267
P12	inc3	del	PRN	0.005931	0.012070	0.491
P12	inc3	del	FC	0.004309	0.011259	0.383
P12	inc3	del	WMC	0.000152	0.000151	1.003
P12	inc3	ins	SEM	0.047211	0.013767	3.429
P12	inc3	ins	PRN	0.015579	0.024071	0.647
P12	inc3	ins	FC	0.011451	0.106100	0.108
P12	inc3	ins	WMC	0.000313	0.000564	0.555
P12	inc5	del	SEM	0.032127	0.041353	0.777
P12	inc5	del	PRN	0.002753	0.010860	0.253
P12	inc5	del	FC	0.002381	0.009808	0.243
P12	inc5	del	WMC	0.000099	0.000155	0.634
P12	inc5	ins	SEM	0.050788	0.010736	4.731
P12	inc5	ins	PRN	0.012583	0.043339	0.290
P12	inc5	ins	FC	0.010350	0.116301	0.089
P12	inc5	ins	WMC	0.000263	0.000503	0.523
P13	inc1	del	SEM	0.079615	0.039754	2.003
P13	inc1	del	PRN	0.020977	0.054158	0.387
P13	inc1	del	FC	0.421273	0.032185	13.089
P13	inc1	del	WMC	0.000945	0.000593	1.594
P13	inc1	ins	SEM	0.089094	0.019020	4.684
P13	inc1	ins	PRN	0.035129	0.062184	0.565
P13	inc1	ins	FC	0.276202	0.955628	0.289
P13	inc1	ins	WMC	0.001176	0.001199	0.981
P13	inc3	del	SEM	0.068048	0.056309	1.208
P13	inc3	del	PRN	0.009732	0.024611	0.395
P13	inc3	del	FC	0.007830	0.028654	0.273
P13	inc3	del	WMC	0.000240	0.000244	0.983
P13	inc3	ins	SEM	0.093261	0.019942	4.677
P13	inc3	ins	PRN	0.027182	0.091430	0.297
P13	inc3	ins	FC	0.287935	0.603593	0.477
P13	inc3	ins	WMC	0.000866	0.001140	0.759
P13	inc5	del	SEM	0.056393	0.091148	0.619
P13	inc5	del	PRN	0.005829	0.025948	0.225
P13	inc5	del	FC	0.004368	0.030541	0.143
P13	inc5	del	WMC	0.000199	0.000190	1.046
P13	inc5	ins	SEM	0.082277	0.022039	3.733
P13	inc5	ins	PRN	0.027398	0.105451	0.260
P13	inc5	ins	FC	0.265834	0.887421	0.300
P13	inc5	ins	WMC	0.000880	0.001286	0.684
P14	inc1	del	SEM	0.108704	0.057824	1.880
P14	inc1	del	PRN	0.025550	0.040421	0.632
P14	inc1	del	FC	0.375528	0.044641	8.412
P14	inc1	del	WMC	0.000942	0.000814	1.156
P14	inc1	ins	SEM	0.133728	0.028363	4.715
P14	inc1	ins	PRN	0.058697	0.109892	0.534
P14	inc1	ins	FC	0.656353	1.139232	0.576
P14	inc1	ins	WMC	0.001879	0.002685	0.700
P14	inc3	del	SEM	0.097584	0.094255	1.035
P14	inc3	del	PRN	0.012867	0.042385	0.304
P14	inc3	del	FC	0.009688	0.051242	0.189
P14	inc3	del	WMC	0.000346	0.000420	0.824
P14	inc3	ins	SEM	0.131120	0.031611	4.148
P14	inc3	ins	PRN	0.051809	0.161420	0.321
P14	inc3	ins	FC	0.687895	0.705284	0.975
P14	inc3	ins	WMC	0.001388	0.001999	0.694
P14	inc5	del	SEM	0.084118	0.120522	0.698
P14	inc5	del	PRN	0.006186	0.035363	0.175
P14	inc5	del	FC	0.003821	0.039766	0.096
P14	inc5	del	WMC	0.000175	0.000253	0.692
P14	inc5	ins	SEM	0.126602	0.034240	3.698
P14	inc5	ins	PRN	0.042913	0.241601	0.178
P14	inc5	ins	FC	0.700970	1.533145	0.457
P14	inc5	ins	WMC	0.001658	0.002271	0.730
P15	inc1	del	SEM	0.273208	0.174149	1.569
P15	inc1	del	PRN	0.047457	0.079712	0.595
P15	inc1	del	FC	0.862920	0.119508	7.221
P15	inc1	del	WMC	0.001352	0.001934	0.699
P15	inc1	ins	SEM	0.307830	0.112487	2.737
P15	inc1	ins	PRN	0.119871	0.557280	0.215
P15	inc1	ins	FC	2.664418	2.095397	1.272
P15	inc1	ins	WMC	0.005926	0.004845	1.223
P15	inc3	del	SEM	0.229333	0.222286	1.032
P15	inc3	del	PRN	0.008205	0.090130	0.091
P15	inc3	del	FC	0.003446	0.113279	0.030
P15	inc3	del	WMC	0.000367	0.000376	0.975
P15	inc3	ins	SEM	0.300941	0.083295	3.613
P15	inc3	ins	PRN	0.088873	1.130170	0.079
P15	inc3	ins	FC	2.721291	2.493798	1.091
P15	inc3	ins	WMC	0.004041	0.008009	0.504
P15	inc5	del	SEM	0.216101	0.294825	0.733
P15	inc5	del	PRN	0.005035	0.095717	0.053
P15	inc5	del	FC	0.001859	0.116390	0.016
P15	inc5	del	WMC	0.000237	0.000317	0.748
P15	inc5	ins	SEM	0.372062	0.095902	3.880
P15	inc5	ins	PRN	0.169969	1.240096	0.137
P15	inc5	ins	FC	3.145466	3.806062	0.826
P15	inc5	ins	WMC	0.005404	0.005331	1.014
P16	inc1	del	SEM	0.425021	0.242774	1.751
P16	inc1	del	PRN	0.052913	0.213125	0.248
P16	inc1	del	FC	1.130022	0.314088	3.598
P16	inc1	del	WMC	0.002036	0.001732	1.175
P16	inc1	ins	SEM	0.551677	0.134090	4.114
P16	inc1	ins	PRN	0.264732	2.315092	0.114
P16	inc1	ins	FC	4.979389	5.122901	0.972
P16	inc1	ins	WMC	0.014189	0.010192	1.392
P16	inc3	del	SEM	0.421373	0.323300	1.303
P16	inc3	del	PRN	0.018952	0.171580	0.110
P16	inc3	del	FC	0.012711	0.270314	0.047
P16	inc3	del	WMC	0.000734	0.000912	0.805
P16	inc3	ins	SEM	0.462971	0.152172	3.042
P16	inc3	ins	PRN	0.223009	3.109280	0.072
P16	inc3	ins	FC	4.566333	6.752010	0.676
P16	inc3	ins	WMC	0.008638	0.014085	0.613
P16	inc5	del	SEM	0.349955	0.446609	0.784
P16	inc5	del	PRN	0.014405	0.186543	0.077
P16	inc5	del	FC	0.004417	0.270616	0.016
P16	inc5	del	WMC	0.000938	0.000568	1.651
P16	inc5	ins	SEM	0.508594	0.174257	2.919
P16	inc5	ins	PRN	0.204950	3.235827	0.063
P16	inc5	ins	FC	4.460447	5.459029	0.817
P16	inc5	ins	WMC	0.009863	0.014142	0.697
P17	inc1	del	SEM	0.563367	0.320419	1.758
P17	inc1	del	PRN	0.074548	0.295499	0.252
P17	inc1	del	FC	1.931425	0.485529	3.978
P17	inc1	del	WMC	0.002555	0.002266	1.127
P17	inc1	ins	SEM	0.706945	0.194515	3.634
P17	inc1	ins	PRN	0.347290	3.660934	0.095
P17	inc1	ins	FC	5.563075	19.813338	0.281
P17	inc1	ins	WMC	0.013804	0.058472	0.236
P17	inc3	del	SEM	0.655752	0.483841	1.355
P17	inc3	del	PRN	0.026579	0.301017	0.088
P17	inc3	del	FC	0.011251	0.476863	0.024
P17	inc3	del	WMC	0.001455	0.001223	1.190
P17	inc3	ins	SEM	0.748730	0.228624	3.275
P17	inc3	ins	PRN	0.393068	5.742965	0.068
P17	inc3	ins	FC	5.425321	14.902812	0.364
P17	inc3	ins	WMC	0.011553	0.023136	0.499
P17	inc5	del	SEM	0.486171	0.624983	0.778
P17	inc5	del	PRN	0.018449	0.325221	0.057
P17	inc5	del	FC	0.004870	0.482455	0.010
P17	inc5	del	WMC	0.000717	0.000860	0.833
P17	inc5	ins	SEM	0.721795	0.246373	2.930
P17	inc5	ins	PRN	0.402241	6.181947	0.065
P17	inc5	ins	FC	5.570104	14.949791	0.373
P17	inc5	ins	WMC	0.012801	0.021163	0.605
P18	inc1	del	SEM	0.754565	0.423318	1.783
P18	inc1	del	PRN	0.086756	0.460928	0.188
P18	inc1	del	FC	1.747355	0.834343	2.094
P18	inc1	del	WMC	0.002963	0.002763	1.073
P18	inc1	ins	SEM	0.967544	0.322139	3.003
P18	inc1	ins	PRN	0.465934	6.895333	0.068
P18	inc1	ins	FC	6.214961	13.276929	0.468
P18	inc1	ins	WMC	0.019837	0.029997	0.661
P18	inc3	del	SEM	0.606934	0.598993	1.013
P18	inc3	del	PRN	0.028944	0.433830	0.067
P18	inc3	del	FC	0.014180	0.722139	0.020
P18	inc3	del	WMC	0.001012	0.001229	0.823
P18	inc3	ins	SEM	0.960113	0.318369	3.016
P18	inc3	ins	PRN	0.499270	9.463979	0.053
P18	inc3	ins	FC	6.246413	12.458132	0.501
P18	inc3	ins	WMC	0.024031	0.037025	0.649
P18	inc5	del	SEM	0.623686	0.789984	0.789
P18	inc5	del	PRN	0.025297	0.485870	0.052
P18	inc5	del	FC	0.003978	0.747338	0.005
P18	inc5	del	WMC	0.000955	0.001236	0.772
P18	inc5	ins	SEM	0.945563	0.317627	2.977
P18	inc5	ins	PRN	0.468633	10.946128	0.043
P18	inc5	ins	FC	6.290194	14.004747	0.449
P18	inc5	ins	WMC	0.026689	0.039889	0.669
P19	inc1	del	SEM	1.004522	0.573492	1.752
P19	inc1	del	PRN	0.088137	0.538242	0.164
P19	inc1	del	FC	1.924228	1.130031	1.703
P19	inc1	del	WMC	0.002473	0.002988	0.828
P19	inc1	ins	SEM	1.185190	0.392632	3.019
P19	inc1	ins	PRN	0.624903	12.627464	0.049
P19	inc1	ins	FC	8.482758	14.194293	0.598
P19	inc1	ins	WMC	0.030186	0.048400	0.624
P19	inc3	del	SEM	0.864874	0.824897	1.048
P19	inc3	del	PRN	0.040031	0.626829	0.064
P19	inc3	del	FC	0.007506	1.069686	0.007
P19	inc3	del	WMC	0.001245	0.001535	0.811
P19	inc3	ins	SEM	1.192149	0.389017	3.065
P19	inc3	ins	PRN	0.603274	16.214190	0.037
P19	inc3	ins	FC	9.670271	17.783310	0.544
P19	inc3	ins	WMC	0.036524	0.069072	0.529
P19	inc5	del	SEM	0.687928	1.018640	0.675
P19	inc5	del	PRN	0.017419	0.615711	0.028
P19	inc5	del	FC	0.002628	1.143949	0.002
P19	inc5	del	WMC	0.000931	0.001383	0.673
P19	inc5	ins	SEM	1.196731	0.476608	2.511
P19	inc5	ins	PRN	0.747791	16.738525	0.045
P19	inc5	ins	FC	8.224507	11.234866	0.732
P19	inc5	ins	WMC	0.030390	0.039476	0.770
P20	inc1	del	SEM	0.896109	0.432936	2.070
P20	inc1	del	PRN	0.491827	0.503938	0.976
P20	inc1	del	FC	1.763112	1.419921	1.242
P20	inc1	del	WMC	0.011130	0.012161	0.915
P20	inc1	ins	SEM	1.013685	0.288688	3.511
P20	inc1	ins	PRN	0.858867	0.971965	0.884
P20	inc1	ins	FC	3.556236	4.636635	0.767
P20	inc1	ins	WMC	0.020059	0.024584	0.816
P20	inc3	del	SEM	0.808855	0.636448	1.271
P20	inc3	del	PRN	0.206837	0.502045	0.412
P20	inc3	del	FC	0.599805	1.711148	0.351
P20	inc3	del	WMC	0.006115	0.006315	0.968
P20	inc3	ins	SEM	0.966461	0.311388	3.104
P20	inc3	ins	PRN	0.745503	1.870945	0.398
P20	inc3	ins	FC	4.448927	55.166807	0.081
P20	inc3	ins	WMC	0.014784	0.100308	0.147
P20	inc5	del	SEM	0.806323	0.761576	1.059
P20	inc5	del	PRN	0.170831	0.452100	0.378
P20	inc5	del	FC	0.375276	1.866901	0.201
P20	inc5	del	WMC	0.004248	0.004931	0.861
P20	inc5	ins	SEM	1.050306	0.354827	2.960
P20	inc5	ins	PRN	0.795247	2.332996	0.341
P20	inc5	ins	FC	4.005914	91.531459	0.044
P20	inc5	ins	WMC	0.020315	0.463985	0.044
```

#### 2026-01-09 (post-fix rerun)
Legacy run: 2026-01-09 post-fix, inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	Turn	Stage	Full_s	Inc_s	Speedup
P1	inc1	del	SEM	0.150286	0.001311	114.649
P1	inc1	del	PRN	0.004109	0.004426	0.928
P1	inc1	del	FC	0.000316	0.000485	0.652
P1	inc1	del	WMC	0.000026	0.000026	1.019
P1	inc1	ins	SEM	0.165449	0.003246	50.966
P1	inc1	ins	PRN	0.004916	0.004589	1.071
P1	inc1	ins	FC	0.000477	0.001047	0.456
P1	inc1	ins	WMC	0.000026	0.000026	1.026
P1	inc3	del	SEM	0.152345	0.003865	39.419
P1	inc3	del	PRN	0.003999	0.005571	0.718
P1	inc3	del	FC	0.000230	0.000646	0.356
P1	inc3	del	WMC	0.000026	0.000031	0.837
P1	inc3	ins	SEM	0.155578	0.004302	36.161
P1	inc3	ins	PRN	0.004639	0.005924	0.783
P1	inc3	ins	FC	0.000459	0.001885	0.244
P1	inc3	ins	WMC	0.000027	0.000028	0.968
P1	inc5	del	SEM	0.087959	0.139243	0.632
P1	inc5	del	PRN	0.002451	0.092785	0.026
P1	inc5	del	FC	0.000227	0.000662	0.343
P1	inc5	del	WMC	0.000025	0.000028	0.887
P1	inc5	ins	SEM	0.151069	0.058631	2.577
P1	inc5	ins	PRN	0.003911	0.091307	0.043
P1	inc5	ins	FC	0.000484	0.001864	0.260
P1	inc5	ins	WMC	0.000026	0.000029	0.902
P3	inc1	del	SEM	0.043786	0.119938	0.365
P3	inc1	del	PRN	0.001761	0.120696	0.015
P3	inc1	del	FC	0.000448	0.000336	1.332
P3	inc1	del	WMC	0.000025	0.000035	0.715
P3	inc1	ins	SEM	0.158259	0.086936	1.820
P3	inc1	ins	PRN	0.004857	0.120315	0.040
P3	inc1	ins	FC	0.000484	0.000297	1.632
P3	inc1	ins	WMC	0.000028	0.000036	0.779
P3	inc3	del	SEM	0.048714	0.111859	0.435
P3	inc3	del	PRN	0.002523	0.113811	0.022
P3	inc3	del	FC	0.000322	0.000610	0.527
P3	inc3	del	WMC	0.000027	0.000027	0.972
P3	inc3	ins	SEM	0.178918	0.091577	1.954
P3	inc3	ins	PRN	0.005756	0.158794	0.036
P3	inc3	ins	FC	0.000469	0.001140	0.412
P3	inc3	ins	WMC	0.000027	0.000029	0.944
P3	inc5	del	SEM	0.020018	0.112539	0.178
P3	inc5	del	PRN	0.000865	0.134470	0.006
P3	inc5	del	FC	0.000198	0.000677	0.292
P3	inc5	del	WMC	0.000029	0.000027	1.068
P3	inc5	ins	SEM	0.172734	0.121563	1.421
P3	inc5	ins	PRN	0.005395	0.167900	0.032
P3	inc5	ins	FC	0.000512	0.001833	0.279
P3	inc5	ins	WMC	0.000029	0.000029	1.010
P4	inc1	del	SEM	0.000961	0.000929	1.035
P4	inc1	del	PRN	0.000143	0.000201	0.709
P4	inc1	del	FC	0.000095	0.000052	1.838
P4	inc1	del	WMC	0.000024	0.000031	0.776
P4	inc1	ins	SEM	0.000951	0.001542	0.616
P4	inc1	ins	PRN	0.000133	0.000184	0.724
P4	inc1	ins	FC	0.000076	0.000058	1.309
P4	inc1	ins	WMC	0.000024	0.000032	0.736
P4	inc3	del	SEM	0.001457	0.002614	0.557
P4	inc3	del	PRN	0.000187	0.000216	0.864
P4	inc3	del	FC	0.000116	0.000054	2.154
P4	inc3	del	WMC	0.000030	0.000029	1.014
P4	inc3	ins	SEM	0.000963	0.000760	1.267
P4	inc3	ins	PRN	0.000147	0.000156	0.943
P4	inc3	ins	FC	0.000075	0.000043	1.736
P4	inc3	ins	WMC	0.000022	0.000029	0.766
P4	inc5	del	SEM	0.000834	0.002952	0.282
P4	inc5	del	PRN	0.000132	0.000315	0.420
P4	inc5	del	FC	0.000097	0.000046	2.108
P4	inc5	del	WMC	0.000024	0.000030	0.789
P4	inc5	ins	SEM	0.001669	0.001434	1.164
P4	inc5	ins	PRN	0.000124	0.000257	0.484
P4	inc5	ins	FC	0.000095	0.000037	2.590
P4	inc5	ins	WMC	0.000024	0.000024	1.000
P5	inc1	del	SEM	0.000278	0.000697	0.399
P5	inc1	del	PRN	0.000138	0.000167	0.826
P5	inc1	del	FC	0.000095	0.000850	0.111
P5	inc1	del	WMC	0.000024	0.000025	0.944
P5	inc1	ins	SEM	0.000322	0.001612	0.200
P5	inc1	ins	PRN	0.000343	0.000428	0.801
P5	inc1	ins	FC	0.000220	0.000434	0.507
P5	inc1	ins	WMC	0.000026	0.000033	0.800
P5	inc3	del	SEM	0.000273	0.001671	0.164
P5	inc3	del	PRN	0.000106	0.000145	0.732
P5	inc3	del	FC	0.000078	0.000846	0.092
P5	inc3	del	WMC	0.000024	0.000024	0.988
P5	inc3	ins	SEM	0.000295	0.002238	0.132
P5	inc3	ins	PRN	0.000294	0.000496	0.593
P5	inc3	ins	FC	0.000211	0.000446	0.473
P5	inc3	ins	WMC	0.000026	0.000033	0.773
P5	inc5	del	SEM	0.000273	0.000872	0.313
P5	inc5	del	PRN	0.000123	0.000149	0.828
P5	inc5	del	FC	0.000096	0.000841	0.114
P5	inc5	del	WMC	0.000024	0.000024	0.983
P5	inc5	ins	SEM	0.000296	0.001256	0.236
P5	inc5	ins	PRN	0.000316	0.000444	0.712
P5	inc5	ins	FC	0.000213	0.000361	0.590
P5	inc5	ins	WMC	0.000026	0.000027	0.968
P6	inc1	del	SEM	0.003598	0.005488	0.656
P6	inc1	del	PRN	0.002722	0.001715	1.587
P6	inc1	del	FC	0.001793	0.002520	0.711
P6	inc1	del	WMC	0.000032	0.000031	1.037
P6	inc1	ins	SEM	0.003528	0.003041	1.160
P6	inc1	ins	PRN	0.003410	0.005600	0.609
P6	inc1	ins	FC	0.002244	0.004390	0.511
P6	inc1	ins	WMC	0.000034	0.000032	1.066
P6	inc3	del	SEM	0.003516	0.003968	0.886
P6	inc3	del	PRN	0.002759	0.001720	1.605
P6	inc3	del	FC	0.001610	0.002525	0.637
P6	inc3	del	WMC	0.000032	0.000031	1.041
P6	inc3	ins	SEM	0.003548	0.008508	0.417
P6	inc3	ins	PRN	0.003360	0.005859	0.573
P6	inc3	ins	FC	0.002300	0.005365	0.429
P6	inc3	ins	WMC	0.000034	0.000033	1.015
P6	inc5	del	SEM	0.003803	0.008326	0.457
P6	inc5	del	PRN	0.002550	0.001747	1.459
P6	inc5	del	FC	0.002038	0.002544	0.801
P6	inc5	del	WMC	0.000030	0.000048	0.620
P6	inc5	ins	SEM	0.003853	0.002809	1.372
P6	inc5	ins	PRN	0.003297	0.006148	0.536
P6	inc5	ins	FC	0.002055	0.005048	0.407
P6	inc5	ins	WMC	0.000034	0.000033	1.026
P7	inc1	del	SEM	0.013004	0.009794	1.328
P7	inc1	del	PRN	0.008436	0.006041	1.397
P7	inc1	del	FC	0.005885	0.006510	0.904
P7	inc1	del	WMC	0.000040	0.000045	0.892
P7	inc1	ins	SEM	0.014474	0.004787	3.023
P7	inc1	ins	PRN	0.009775	0.013014	0.751
P7	inc1	ins	FC	0.006315	0.008631	0.732
P7	inc1	ins	WMC	0.000041	0.000040	1.031
P7	inc3	del	SEM	0.012515	0.010388	1.205
P7	inc3	del	PRN	0.007836	0.005986	1.309
P7	inc3	del	FC	0.004927	0.007115	0.692
P7	inc3	del	WMC	0.000040	0.000048	0.835
P7	inc3	ins	SEM	0.013163	0.004461	2.950
P7	inc3	ins	PRN	0.009271	0.023850	0.389
P7	inc3	ins	FC	0.006746	0.016573	0.407
P7	inc3	ins	WMC	0.000044	0.000060	0.739
P7	inc5	del	SEM	0.012111	0.010723	1.129
P7	inc5	del	PRN	0.007512	0.006454	1.164
P7	inc5	del	FC	0.005179	0.006137	0.844
P7	inc5	del	WMC	0.000040	0.000053	0.740
P7	inc5	ins	SEM	0.012441	0.005477	2.272
P7	inc5	ins	PRN	0.010049	0.024449	0.411
P7	inc5	ins	FC	0.007202	0.012829	0.561
P7	inc5	ins	WMC	0.000045	0.000054	0.827
P8	inc1	del	SEM	0.054243	0.007734	7.013
P8	inc1	del	PRN	0.032484	0.018493	1.757
P8	inc1	del	FC	0.029774	0.013045	2.282
P8	inc1	del	WMC	0.000057	0.000053	1.079
P8	inc1	ins	SEM	0.055015	0.007548	7.289
P8	inc1	ins	PRN	0.033381	0.017790	1.876
P8	inc1	ins	FC	0.030508	0.012751	2.393
P8	inc1	ins	WMC	0.000059	0.000053	1.103
P8	inc3	del	SEM	0.044966	0.019697	2.283
P8	inc3	del	PRN	0.027718	0.021805	1.271
P8	inc3	del	FC	0.024921	0.018974	1.313
P8	inc3	del	WMC	0.000056	0.000097	0.577
P8	inc3	ins	SEM	0.054737	0.013348	4.101
P8	inc3	ins	PRN	0.032837	0.138414	0.237
P8	inc3	ins	FC	0.029462	0.054123	0.544
P8	inc3	ins	WMC	0.000057	0.000064	0.888
P8	inc5	del	SEM	0.038962	0.022632	1.722
P8	inc5	del	PRN	0.000915	0.012736	0.072
P8	inc5	del	FC	0.000126	0.008138	0.015
P8	inc5	del	WMC	0.000030	0.000039	0.773
P8	inc5	ins	SEM	0.052885	0.012862	4.112
P8	inc5	ins	PRN	0.033429	1.048929	0.032
P8	inc5	ins	FC	0.030676	0.225543	0.136
P8	inc5	ins	WMC	0.000057	0.000145	0.391
P9	inc1	del	SEM	0.027127	0.004078	6.651
P9	inc1	del	PRN	0.014695	0.006651	2.209
P9	inc1	del	FC	0.010639	0.006289	1.692
P9	inc1	del	WMC	0.000050	0.000058	0.868
P9	inc1	ins	SEM	0.027140	0.002904	9.347
P9	inc1	ins	PRN	0.015002	0.007548	1.988
P9	inc1	ins	FC	0.011692	0.009729	1.202
P9	inc1	ins	WMC	0.000060	0.000123	0.488
P9	inc3	del	SEM	0.022686	0.019069	1.190
P9	inc3	del	PRN	0.014667	0.011068	1.325
P9	inc3	del	FC	0.008449	0.008407	1.005
P9	inc3	del	WMC	0.000043	0.000048	0.891
P9	inc3	ins	SEM	0.027475	0.007379	3.723
P9	inc3	ins	PRN	0.015385	0.041623	0.370
P9	inc3	ins	FC	0.011672	0.019194	0.608
P9	inc3	ins	WMC	0.000059	0.000063	0.939
P9	inc5	del	SEM	0.022454	0.019336	1.161
P9	inc5	del	PRN	0.012539	0.011639	1.077
P9	inc5	del	FC	0.009082	0.008368	1.085
P9	inc5	del	WMC	0.000047	0.000051	0.937
P9	inc5	ins	SEM	0.029369	0.008551	3.435
P9	inc5	ins	PRN	0.015975	0.042263	0.378
P9	inc5	ins	FC	0.011717	0.020832	0.562
P9	inc5	ins	WMC	0.000063	0.000061	1.027
P10	inc1	del	SEM	0.167250	0.057061	2.931
P10	inc1	del	PRN	0.009828	0.026542	0.370
P10	inc1	del	FC	0.000784	0.001781	0.440
P10	inc1	del	WMC	0.000039	0.000043	0.911
P10	inc1	ins	SEM	0.172988	0.015545	11.129
P10	inc1	ins	PRN	0.009987	0.024929	0.401
P10	inc1	ins	FC	0.000888	0.001510	0.588
P10	inc1	ins	WMC	0.000040	0.000052	0.766
P10	inc3	del	SEM	0.145143	0.051725	2.806
P10	inc3	del	PRN	0.006648	0.031369	0.212
P10	inc3	del	FC	0.000839	0.001957	0.429
P10	inc3	del	WMC	0.000040	0.000043	0.918
P10	inc3	ins	SEM	0.152614	0.024758	6.164
P10	inc3	ins	PRN	0.008095	0.032207	0.251
P10	inc3	ins	FC	0.000868	0.005790	0.150
P10	inc3	ins	WMC	0.000039	0.000039	1.003
P10	inc5	del	SEM	0.149307	0.053402	2.796
P10	inc5	del	PRN	0.006935	0.039365	0.176
P10	inc5	del	FC	0.000511	0.001899	0.269
P10	inc5	del	WMC	0.000037	0.000044	0.837
P10	inc5	ins	SEM	0.152724	0.032194	4.744
P10	inc5	ins	PRN	0.007135	0.050453	0.141
P10	inc5	ins	FC	0.000897	0.008425	0.106
P10	inc5	ins	WMC	0.000039	0.000040	0.984
P11	inc1	del	SEM	0.137996	0.042612	3.238
P11	inc1	del	PRN	0.004689	0.020278	0.231
P11	inc1	del	FC	0.000224	0.000127	1.762
P11	inc1	del	WMC	0.000030	0.000034	0.878
P11	inc1	ins	SEM	0.151510	0.015008	10.095
P11	inc1	ins	PRN	0.005978	0.021012	0.285
P11	inc1	ins	FC	0.000239	0.000102	2.344
P11	inc1	ins	WMC	0.000032	0.000056	0.567
P11	inc3	del	SEM	0.140501	0.043888	3.201
P11	inc3	del	PRN	0.004949	0.026099	0.190
P11	inc3	del	FC	0.000216	0.001136	0.190
P11	inc3	del	WMC	0.000030	0.000034	0.904
P11	inc3	ins	SEM	0.151708	0.023333	6.502
P11	inc3	ins	PRN	0.005449	0.035633	0.153
P11	inc3	ins	FC	0.000242	0.000270	0.896
P11	inc3	ins	WMC	0.000031	0.000038	0.819
P11	inc5	del	SEM	0.120800	0.049901	2.421
P11	inc5	del	PRN	0.003378	0.032099	0.105
P11	inc5	del	FC	0.000214	0.001310	0.163
P11	inc5	del	WMC	0.000030	0.000032	0.932
P11	inc5	ins	SEM	0.143529	0.027798	5.163
P11	inc5	ins	PRN	0.005270	0.039387	0.134
P11	inc5	ins	FC	0.000235	0.004960	0.047
P11	inc5	ins	WMC	0.000030	0.000031	0.976
P12	inc1	del	SEM	0.731445	0.135671	5.391
P12	inc1	del	PRN	0.145979	0.196444	0.743
P12	inc1	del	FC	0.096753	0.069386	1.394
P12	inc1	del	WMC	0.000216	0.000360	0.598
P12	inc1	ins	SEM	0.872891	0.058799	14.845
P12	inc1	ins	PRN	0.193853	0.908953	0.213
P12	inc1	ins	FC	0.144939	0.400012	0.362
P12	inc1	ins	WMC	0.000437	0.000688	0.635
P12	inc3	del	SEM	0.676968	0.273221	2.478
P12	inc3	del	PRN	0.056159	0.261970	0.214
P12	inc3	del	FC	0.015634	0.090387	0.173
P12	inc3	del	WMC	0.000145	0.000392	0.371
P12	inc3	ins	SEM	0.830800	0.160351	5.181
P12	inc3	ins	PRN	0.200795	3.676396	0.055
P12	inc3	ins	FC	0.133020	0.893183	0.149
P12	inc3	ins	WMC	0.000330	0.000608	0.543
P12	inc5	del	SEM	0.597505	0.353981	1.688
P12	inc5	del	PRN	0.050865	0.358512	0.142
P12	inc5	del	FC	0.015226	0.095501	0.159
P12	inc5	del	WMC	0.000144	0.000228	0.632
P12	inc5	ins	SEM	0.753761	0.255912	2.945
P12	inc5	ins	PRN	0.190889	3.765238	0.051
P12	inc5	ins	FC	0.134192	0.897322	0.150
P12	inc5	ins	WMC	0.000297	0.000772	0.385
P13	inc1	del	SEM	1.655599	0.211352	7.833
P13	inc1	del	PRN	0.226398	0.274869	0.824
P13	inc1	del	FC	0.496145	0.094021	5.277
P13	inc1	del	WMC	0.001061	0.000901	1.177
P13	inc1	ins	SEM	1.903420	0.085241	22.330
P13	inc1	ins	PRN	0.253572	0.287357	0.882
P13	inc1	ins	FC	0.351663	0.827515	0.425
P13	inc1	ins	WMC	0.001327	0.001476	0.899
P13	inc3	del	SEM	1.398513	0.708246	1.975
P13	inc3	del	PRN	0.164440	0.617094	0.266
P13	inc3	del	FC	0.081689	0.100588	0.812
P13	inc3	del	WMC	0.000352	0.000780	0.451
P13	inc3	ins	SEM	1.877691	0.298560	6.289
P13	inc3	ins	PRN	0.241931	1.240569	0.195
P13	inc3	ins	FC	0.352025	0.814578	0.432
P13	inc3	ins	WMC	0.001197	0.001398	0.856
P13	inc5	del	SEM	1.145930	0.836851	1.369
P13	inc5	del	PRN	0.140600	0.794288	0.177
P13	inc5	del	FC	0.070941	0.110608	0.641
P13	inc5	del	WMC	0.000183	0.000492	0.373
P13	inc5	ins	SEM	1.817206	0.569569	3.190
P13	inc5	ins	PRN	0.247157	1.662686	0.149
P13	inc5	ins	FC	0.357407	0.782582	0.457
P13	inc5	ins	WMC	0.001356	0.001535	0.883
P14	inc1	del	SEM	2.649057	0.463945	5.710
P14	inc1	del	PRN	0.229747	0.413588	0.555
P14	inc1	del	FC	0.438554	0.101842	4.306
P14	inc1	del	WMC	0.001145	0.001468	0.780
P14	inc1	ins	SEM	2.972744	0.193334	15.376
P14	inc1	ins	PRN	0.334559	1.853697	0.180
P14	inc1	ins	FC	0.713965	1.089166	0.656
P14	inc1	ins	WMC	0.001994	0.002255	0.884
P14	inc3	del	SEM	2.188151	0.999486	2.189
P14	inc3	del	PRN	0.190253	0.889417	0.214
P14	inc3	del	FC	0.064195	0.125973	0.510
P14	inc3	del	WMC	0.000274	0.000943	0.290
P14	inc3	ins	SEM	2.991978	0.618763	4.835
P14	inc3	ins	PRN	0.342124	2.620715	0.131
P14	inc3	ins	FC	0.693571	0.939938	0.738
P14	inc3	ins	WMC	0.001651	0.002139	0.772
P14	inc5	del	SEM	2.075539	1.458627	1.423
P14	inc5	del	PRN	0.142391	1.195119	0.119
P14	inc5	del	FC	0.039733	0.082667	0.481
P14	inc5	del	WMC	0.000231	0.000450	0.515
P14	inc5	ins	SEM	2.981700	0.938501	3.177
P14	inc5	ins	PRN	0.329946	3.609158	0.091
P14	inc5	ins	FC	0.727687	1.065345	0.683
P14	inc5	ins	WMC	0.002116	0.002334	0.906
P15	inc1	del	SEM	6.986261	1.585136	4.407
P15	inc1	del	PRN	0.490746	0.955204	0.514
P15	inc1	del	FC	0.905954	0.207364	4.369
P15	inc1	del	WMC	0.001647	0.002464	0.668
P15	inc1	ins	SEM	7.435397	0.482602	15.407
P15	inc1	ins	PRN	0.631630	2.872320	0.220
P15	inc1	ins	FC	2.408297	1.975453	1.219
P15	inc1	ins	WMC	0.004651	0.005025	0.926
P15	inc3	del	SEM	5.891017	2.974583	1.980
P15	inc3	del	PRN	0.288169	1.905459	0.151
P15	inc3	del	FC	0.023034	0.145963	0.158
P15	inc3	del	WMC	0.000340	0.000634	0.536
P15	inc3	ins	SEM	7.493096	1.523228	4.919
P15	inc3	ins	PRN	0.674577	6.432035	0.105
P15	inc3	ins	FC	2.400981	2.239904	1.072
P15	inc3	ins	WMC	0.004472	0.005519	0.810
P15	inc5	del	SEM	4.950324	3.475464	1.424
P15	inc5	del	PRN	0.252453	2.568903	0.098
P15	inc5	del	FC	0.018454	0.152134	0.121
P15	inc5	del	WMC	0.000316	0.000473	0.667
P15	inc5	ins	SEM	7.403819	2.197909	3.369
P15	inc5	ins	PRN	0.663303	7.295310	0.091
P15	inc5	ins	FC	2.390035	2.933782	0.815
P15	inc5	ins	WMC	0.003986	0.006476	0.615
P16	inc1	del	SEM	10.975435	4.515618	2.431
P16	inc1	del	PRN	0.699559	1.622124	0.431
P16	inc1	del	FC	1.539313	0.491509	3.132
P16	inc1	del	WMC	0.003001	0.004396	0.683
P16	inc1	ins	SEM	11.465530	0.834887	13.733
P16	inc1	ins	PRN	1.049156	3.766347	0.279
P16	inc1	ins	FC	3.989139	4.211829	0.947
P16	inc1	ins	WMC	0.008413	0.008987	0.936
P16	inc3	del	SEM	9.387051	6.153769	1.525
P16	inc3	del	PRN	0.533062	3.545029	0.150
P16	inc3	del	FC	0.093480	0.370687	0.252
P16	inc3	del	WMC	0.000651	0.001079	0.603
P16	inc3	ins	SEM	11.225025	2.324650	4.829
P16	inc3	ins	PRN	1.111017	7.441712	0.149
P16	inc3	ins	FC	4.002833	4.075054	0.982
P16	inc3	ins	WMC	0.008474	0.011894	0.712
P16	inc5	del	SEM	8.468580	7.019519	1.206
P16	inc5	del	PRN	0.435963	4.643663	0.094
P16	inc5	del	FC	0.051117	0.346785	0.147
P16	inc5	del	WMC	0.000566	0.001011	0.559
P16	inc5	ins	SEM	11.316014	3.699514	3.059
P16	inc5	ins	PRN	1.049687	10.747929	0.098
P16	inc5	ins	FC	3.949415	4.241539	0.931
P16	inc5	ins	WMC	0.007685	0.010215	0.752
P17	inc1	del	SEM	16.138063	7.795415	2.070
P17	inc1	del	PRN	0.841384	2.075817	0.405
P17	inc1	del	FC	1.663688	0.697266	2.386
P17	inc1	del	WMC	0.002773	0.003828	0.725
P17	inc1	ins	SEM	16.713840	1.049018	15.933
P17	inc1	ins	PRN	1.530934	8.335932	0.184
P17	inc1	ins	FC	5.103743	7.761294	0.658
P17	inc1	ins	WMC	0.012738	0.017394	0.732
P17	inc3	del	SEM	13.857574	10.087168	1.374
P17	inc3	del	PRN	0.655642	4.484519	0.146
P17	inc3	del	FC	0.058198	0.537933	0.108
P17	inc3	del	WMC	0.000967	0.001605	0.602
P17	inc3	ins	SEM	16.686241	3.200940	5.213
P17	inc3	ins	PRN	1.400418	13.636774	0.103
P17	inc3	ins	FC	5.015177	7.640902	0.656
P17	inc3	ins	WMC	0.012339	0.017823	0.692
P17	inc5	del	SEM	11.680647	11.556862	1.011
P17	inc5	del	PRN	0.597542	6.823066	0.088
P17	inc5	del	FC	0.045925	0.542056	0.085
P17	inc5	del	WMC	0.000891	0.001241	0.718
P17	inc5	ins	SEM	16.689915	5.373781	3.106
P17	inc5	ins	PRN	1.451857	16.354883	0.089
P17	inc5	ins	FC	5.038975	7.498621	0.672
P17	inc5	ins	WMC	0.013021	0.018242	0.714
P18	inc1	del	SEM	18.976903	13.436108	1.412
P18	inc1	del	PRN	1.080570	3.324535	0.325
P18	inc1	del	FC	1.565117	0.937248	1.670
P18	inc1	del	WMC	0.002943	0.003832	0.768
P18	inc1	ins	SEM	21.210024	2.001859	10.595
P18	inc1	ins	PRN	1.831063	12.102001	0.151
P18	inc1	ins	FC	6.165108	4.883320	1.262
P18	inc1	ins	WMC	0.021030	0.033452	0.629
P18	inc3	del	SEM	17.154974	14.777614	1.161
P18	inc3	del	PRN	0.812582	6.630425	0.123
P18	inc3	del	FC	0.056087	0.785400	0.071
P18	inc3	del	WMC	0.001326	0.001645	0.806
P18	inc3	ins	SEM	21.170891	5.100546	4.151
P18	inc3	ins	PRN	2.068435	20.498193	0.101
P18	inc3	ins	FC	6.318911	4.430526	1.426
P18	inc3	ins	WMC	0.019142	0.027296	0.701
P18	inc5	del	SEM	15.258193	16.056035	0.950
P18	inc5	del	PRN	0.737913	9.541551	0.077
P18	inc5	del	FC	0.015912	0.765683	0.021
P18	inc5	del	WMC	0.001069	0.001156	0.925
P18	inc5	ins	SEM	20.576998	7.380478	2.788
P18	inc5	ins	PRN	1.853081	24.391576	0.076
P18	inc5	ins	FC	6.142754	4.758892	1.291
P18	inc5	ins	WMC	0.021636	0.024774	0.873
P19	inc1	del	SEM	24.409247	20.202007	1.208
P19	inc1	del	PRN	1.385906	3.941049	0.352
P19	inc1	del	FC	1.651545	1.277386	1.293
P19	inc1	del	WMC	0.003828	0.005251	0.729
P19	inc1	ins	SEM	28.737163	2.183931	13.158
P19	inc1	ins	PRN	2.479310	18.789670	0.132
P19	inc1	ins	FC	8.195748	5.459357	1.501
P19	inc1	ins	WMC	0.027279	0.030283	0.901
P19	inc3	del	SEM	22.027390	21.829340	1.009
P19	inc3	del	PRN	1.108596	8.088786	0.137
P19	inc3	del	FC	0.106113	1.126427	0.094
P19	inc3	del	WMC	0.001374	0.002102	0.654
P19	inc3	ins	SEM	29.223146	5.900015	4.953
P19	inc3	ins	PRN	2.516055	29.219920	0.086
P19	inc3	ins	FC	8.181255	6.799188	1.203
P19	inc3	ins	WMC	0.027218	0.042528	0.640
P19	inc5	del	SEM	19.440257	23.456679	0.829
P19	inc5	del	PRN	0.980592	12.456281	0.079
P19	inc5	del	FC	0.101951	1.156558	0.088
P19	inc5	del	WMC	0.001355	0.001754	0.772
P19	inc5	ins	SEM	28.950370	9.471341	3.057
P19	inc5	ins	PRN	2.434148	33.529468	0.073
P19	inc5	ins	FC	8.703763	6.197246	1.404
P19	inc5	ins	WMC	0.026790	0.034620	0.774
P20	inc1	del	SEM	17.824630	7.049249	2.529
P20	inc1	del	PRN	1.987636	3.117125	0.638
P20	inc1	del	FC	2.600869	1.949388	1.334
P20	inc1	del	WMC	0.012314	0.012005	1.026
P20	inc1	ins	SEM	18.830822	0.949854	19.825
P20	inc1	ins	PRN	2.464124	8.474288	0.291
P20	inc1	ins	FC	4.557335	4.308826	1.058
P20	inc1	ins	WMC	0.017015	0.016538	1.029
P20	inc3	del	SEM	16.806544	15.376363	1.093
P20	inc3	del	PRN	1.569239	4.950025	0.317
P20	inc3	del	FC	1.001882	2.275464	0.440
P20	inc3	del	WMC	0.006374	0.012005	0.531
P20	inc3	ins	SEM	19.055086	3.044875	6.258
P20	inc3	ins	PRN	2.468683	11.701473	0.211
P20	inc3	ins	FC	6.021844	13.149732	0.458
P20	inc3	ins	WMC	0.018505	0.077025	0.240
P20	inc5	del	SEM	15.467195	18.695150	0.827
P20	inc5	del	PRN	0.968819	6.675238	0.145
P20	inc5	del	FC	0.469338	2.117876	0.222
P20	inc5	del	WMC	0.004095	0.006336	0.646
P20	inc5	ins	SEM	18.974118	4.988900	3.803
P20	inc5	ins	PRN	2.577071	41.991174	0.061
P20	inc5	ins	FC	4.634768	40.840566	0.113
P20	inc5	ins	WMC	0.018302	0.042977	0.426
```

#### 2026-01-09 (pre-fix run)
Legacy run: 2026-01-09 pre-fix, inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	Turn	Stage	Full_s	Inc_s	Speedup
P1	inc1	del	SEM	0.036916	0.107275	0.344
P1	inc1	del	PRN	0.001881	0.110591	0.017
P1	inc1	del	FC	0.000432	0.000298	1.448
P1	inc1	del	WMC	0.000024	0.000032	0.741
P1	inc1	ins	SEM	0.146112	0.087999	1.660
P1	inc1	ins	PRN	0.004293	0.116420	0.037
P1	inc1	ins	FC	0.000430	0.000286	1.502
P1	inc1	ins	WMC	0.000025	0.000030	0.819
P1	inc3	del	SEM	0.027843	0.140441	0.198
P1	inc3	del	PRN	0.001665	0.163259	0.010
P1	inc3	del	FC	0.000455	0.000331	1.373
P1	inc3	del	WMC	0.000025	0.000068	0.375
P1	inc3	ins	SEM	0.166298	0.146420	1.136
P1	inc3	ins	PRN	0.006219	0.133987	0.046
P1	inc3	ins	FC	0.000468	0.000254	1.841
P1	inc3	ins	WMC	0.000027	0.000030	0.895
P1	inc5	del	SEM	0.028716	0.117594	0.244
P1	inc5	del	PRN	0.001230	0.123589	0.010
P1	inc5	del	FC	0.000260	0.000254	1.027
P1	inc5	del	WMC	0.000024	0.000027	0.881
P1	inc5	ins	SEM	0.156379	0.114796	1.362
P1	inc5	ins	PRN	0.004872	0.138778	0.035
P1	inc5	ins	FC	0.000443	0.001402	0.316
P1	inc5	ins	WMC	0.000026	0.000037	0.682
P3	inc1	del	SEM	0.048905	0.097985	0.499
P3	inc1	del	PRN	0.002098	0.124664	0.017
P3	inc1	del	FC	0.000419	0.000326	1.288
P3	inc1	del	WMC	0.000024	0.000035	0.693
P3	inc1	ins	SEM	0.141773	0.092253	1.537
P3	inc1	ins	PRN	0.004461	0.131177	0.034
P3	inc1	ins	FC	0.000448	0.000247	1.815
P3	inc1	ins	WMC	0.000025	0.000031	0.799
P3	inc3	del	SEM	0.048009	0.088201	0.544
P3	inc3	del	PRN	0.002084	0.096404	0.022
P3	inc3	del	FC	0.000432	0.000291	1.485
P3	inc3	del	WMC	0.000024	0.000032	0.748
P3	inc3	ins	SEM	0.150190	0.072500	2.072
P3	inc3	ins	PRN	0.004457	0.100844	0.044
P3	inc3	ins	FC	0.000437	0.000249	1.753
P3	inc3	ins	WMC	0.000025	0.000030	0.823
P3	inc5	del	SEM	0.025540	0.098974	0.258
P3	inc5	del	PRN	0.001577	0.117308	0.013
P3	inc5	del	FC	0.000439	0.000293	1.500
P3	inc5	del	WMC	0.000024	0.000032	0.769
P3	inc5	ins	SEM	0.159499	0.117883	1.353
P3	inc5	ins	PRN	0.005659	0.164969	0.034
P3	inc5	ins	FC	0.000567	0.000270	2.098
P3	inc5	ins	WMC	0.000033	0.000037	0.872
P4	inc1	del	SEM	0.000921	0.000961	0.958
P4	inc1	del	PRN	0.000134	0.000147	0.909
P4	inc1	del	FC	0.000087	0.000037	2.348
P4	inc1	del	WMC	0.000022	0.000023	0.931
P4	inc1	ins	SEM	0.000884	0.000866	1.021
P4	inc1	ins	PRN	0.000126	0.000119	1.058
P4	inc1	ins	FC	0.000098	0.000033	2.947
P4	inc1	ins	WMC	0.000022	0.000022	0.994
P4	inc3	del	SEM	0.000903	0.000894	1.010
P4	inc3	del	PRN	0.000125	0.000126	0.996
P4	inc3	del	FC	0.000091	0.000037	2.459
P4	inc3	del	WMC	0.000022	0.000022	0.986
P4	inc3	ins	SEM	0.001640	0.000787	2.085
P4	inc3	ins	PRN	0.000112	0.000106	1.057
P4	inc3	ins	FC	0.000086	0.000033	2.600
P4	inc3	ins	WMC	0.000021	0.000022	0.979
P4	inc5	del	SEM	0.000886	0.001143	0.775
P4	inc5	del	PRN	0.000124	0.000133	0.930
P4	inc5	del	FC	0.000088	0.000034	2.574
P4	inc5	del	WMC	0.000022	0.000022	0.962
P4	inc5	ins	SEM	0.000891	0.001432	0.622
P4	inc5	ins	PRN	0.000124	0.000129	0.961
P4	inc5	ins	FC	0.000088	0.000033	2.677
P4	inc5	ins	WMC	0.000022	0.000022	1.000
P5	inc1	del	SEM	0.000258	0.001385	0.186
P5	inc1	del	PRN	0.000098	0.000126	0.776
P5	inc1	del	FC	0.000087	0.000717	0.121
P5	inc1	del	WMC	0.000022	0.000023	0.960
P5	inc1	ins	SEM	0.000278	0.002362	0.118
P5	inc1	ins	PRN	0.000287	0.000520	0.551
P5	inc1	ins	FC	0.000194	0.000495	0.393
P5	inc1	ins	WMC	0.000023	0.000025	0.948
P5	inc3	del	SEM	0.000261	0.001015	0.257
P5	inc3	del	PRN	0.000095	0.000128	0.741
P5	inc3	del	FC	0.000077	0.000784	0.098
P5	inc3	del	WMC	0.000022	0.000025	0.904
P5	inc3	ins	SEM	0.000275	0.001243	0.221
P5	inc3	ins	PRN	0.000260	0.000396	0.657
P5	inc3	ins	FC	0.000198	0.000394	0.503
P5	inc3	ins	WMC	0.000024	0.000025	0.966
P5	inc5	del	SEM	0.000259	0.001801	0.144
P5	inc5	del	PRN	0.000104	0.000128	0.815
P5	inc5	del	FC	0.000077	0.000819	0.094
P5	inc5	del	WMC	0.000022	0.000029	0.767
P5	inc5	ins	SEM	0.000280	0.001702	0.164
P5	inc5	ins	PRN	0.000278	0.000395	0.705
P5	inc5	ins	FC	0.000214	0.000375	0.571
P5	inc5	ins	WMC	0.000024	0.000025	0.952
P6	inc1	del	SEM	0.003643	0.003390	1.075
P6	inc1	del	PRN	0.003013	0.001313	2.295
P6	inc1	del	FC	0.001827	0.001738	1.052
P6	inc1	del	WMC	0.000028	0.000030	0.942
P6	inc1	ins	SEM	0.004171	0.003336	1.250
P6	inc1	ins	PRN	0.003008	0.001850	1.626
P6	inc1	ins	FC	0.001967	0.001592	1.236
P6	inc1	ins	WMC	0.000031	0.000031	1.022
P6	inc3	del	SEM	0.002740	0.005383	0.509
P6	inc3	del	PRN	0.001450	0.001707	0.850
P6	inc3	del	FC	0.000749	0.001297	0.577
P6	inc3	del	WMC	0.000046	0.000028	1.615
P6	inc3	ins	SEM	0.003907	0.003790	1.031
P6	inc3	ins	PRN	0.003184	0.011571	0.275
P6	inc3	ins	FC	0.002064	0.008875	0.233
P6	inc3	ins	WMC	0.000031	0.000031	0.977
P6	inc5	del	SEM	0.002669	0.004438	0.601
P6	inc5	del	PRN	0.001463	0.001440	1.016
P6	inc5	del	FC	0.000685	0.001348	0.508
P6	inc5	del	WMC	0.000028	0.000028	0.972
P6	inc5	ins	SEM	0.004169	0.002740	1.522
P6	inc5	ins	PRN	0.002946	0.011529	0.256
P6	inc5	ins	FC	0.002119	0.009233	0.230
P6	inc5	ins	WMC	0.000038	0.000039	0.987
P7	inc1	del	SEM	0.013023	0.004702	2.770
P7	inc1	del	PRN	0.009266	0.003851	2.406
P7	inc1	del	FC	0.006582	0.003731	1.764
P7	inc1	del	WMC	0.000039	0.000037	1.047
P7	inc1	ins	SEM	0.013293	0.002075	6.405
P7	inc1	ins	PRN	0.008465	0.003608	2.346
P7	inc1	ins	FC	0.006555	0.003008	2.179
P7	inc1	ins	WMC	0.000040	0.000036	1.094
P7	inc3	del	SEM	0.011901	0.004278	2.782
P7	inc3	del	PRN	0.008918	0.005354	1.666
P7	inc3	del	FC	0.006562	0.003965	1.655
P7	inc3	del	WMC	0.000038	0.000039	0.973
P7	inc3	ins	SEM	0.011909	0.002934	4.059
P7	inc3	ins	PRN	0.008756	0.004578	1.913
P7	inc3	ins	FC	0.006485	0.003135	2.068
P7	inc3	ins	WMC	0.000039	0.000036	1.082
P7	inc5	del	SEM	0.011736	0.004673	2.512
P7	inc5	del	PRN	0.008845	0.005315	1.664
P7	inc5	del	FC	0.006250	0.003878	1.611
P7	inc5	del	WMC	0.000037	0.000036	1.037
P7	inc5	ins	SEM	0.011901	0.003143	3.787
P7	inc5	ins	PRN	0.008424	0.004831	1.744
P7	inc5	ins	FC	0.006142	0.003257	1.886
P7	inc5	ins	WMC	0.000039	0.000046	0.851
P8	inc1	del	SEM	0.048436	0.002659	18.213
P8	inc1	del	PRN	0.030608	0.013638	2.244
P8	inc1	del	FC	0.027437	0.011120	2.467
P8	inc1	del	WMC	0.000048	0.000048	0.988
P8	inc1	ins	SEM	0.049501	0.002519	19.653
P8	inc1	ins	PRN	0.032639	0.013502	2.417
P8	inc1	ins	FC	0.029863	0.010907	2.738
P8	inc1	ins	WMC	0.000069	0.000048	1.431
P8	inc3	del	SEM	0.045830	0.004410	10.392
P8	inc3	del	PRN	0.030744	0.018604	1.653
P8	inc3	del	FC	0.028764	0.012391	2.321
P8	inc3	del	WMC	0.000047	0.000042	1.121
P8	inc3	ins	SEM	0.048340	0.005250	9.208
P8	inc3	ins	PRN	0.031380	0.018211	1.723
P8	inc3	ins	FC	0.028752	0.013169	2.183
P8	inc3	ins	WMC	0.000053	0.000070	0.754
P8	inc5	del	SEM	0.043373	0.012522	3.464
P8	inc5	del	PRN	0.024957	0.018376	1.358
P8	inc5	del	FC	0.021289	0.010051	2.118
P8	inc5	del	WMC	0.000042	0.000041	1.012
P8	inc5	ins	SEM	0.048339	0.007430	6.505
P8	inc5	ins	PRN	0.030377	0.109237	0.278
P8	inc5	ins	FC	0.027974	0.042521	0.658
P8	inc5	ins	WMC	0.000055	0.000058	0.946
P9	inc1	del	SEM	0.023308	0.009787	2.382
P9	inc1	del	PRN	0.013647	0.007858	1.737
P9	inc1	del	FC	0.010157	0.006077	1.672
P9	inc1	del	WMC	0.000048	0.000046	1.060
P9	inc1	ins	SEM	0.025373	0.003356	7.560
P9	inc1	ins	PRN	0.014119	0.007649	1.846
P9	inc1	ins	FC	0.010771	0.010580	1.018
P9	inc1	ins	WMC	0.000055	0.000074	0.744
P9	inc3	del	SEM	0.020228	0.010211	1.981
P9	inc3	del	PRN	0.013055	0.010500	1.243
P9	inc3	del	FC	0.009874	0.005720	1.726
P9	inc3	del	WMC	0.000043	0.000041	1.053
P9	inc3	ins	SEM	0.025115	0.006206	4.047
P9	inc3	ins	PRN	0.014612	0.011557	1.264
P9	inc3	ins	FC	0.011165	0.008706	1.283
P9	inc3	ins	WMC	0.000053	0.000070	0.762
P9	inc5	del	SEM	0.020036	0.011923	1.680
P9	inc5	del	PRN	0.012995	0.010170	1.278
P9	inc5	del	FC	0.010354	0.005513	1.878
P9	inc5	del	WMC	0.000045	0.000037	1.212
P9	inc5	ins	SEM	0.025344	0.006571	3.857
P9	inc5	ins	PRN	0.014708	0.014068	1.045
P9	inc5	ins	FC	0.011296	0.009891	1.142
P9	inc5	ins	WMC	0.000061	0.000083	0.731
P10	inc1	del	SEM	0.132803	0.048911	2.715
P10	inc1	del	PRN	0.006191	0.013722	0.451
P10	inc1	del	FC	0.000749	0.001393	0.538
P10	inc1	del	WMC	0.000035	0.000037	0.941
P10	inc1	ins	SEM	0.138227	0.011944	11.573
P10	inc1	ins	PRN	0.006139	0.014517	0.423
P10	inc1	ins	FC	0.000883	0.000937	0.942
P10	inc1	ins	WMC	0.000036	0.000035	1.029
P10	inc3	del	SEM	0.125004	0.033687	3.711
P10	inc3	del	PRN	0.005927	0.025436	0.233
P10	inc3	del	FC	0.000718	0.001452	0.494
P10	inc3	del	WMC	0.000037	0.000037	1.001
P10	inc3	ins	SEM	0.159665	0.019541	8.171
P10	inc3	ins	PRN	0.009914	0.025927	0.382
P10	inc3	ins	FC	0.000904	0.001469	0.616
P10	inc3	ins	WMC	0.000040	0.000035	1.126
P10	inc5	del	SEM	0.121538	0.042503	2.860
P10	inc5	del	PRN	0.005263	0.033952	0.155
P10	inc5	del	FC	0.000668	0.001467	0.456
P10	inc5	del	WMC	0.000035	0.000039	0.900
P10	inc5	ins	SEM	0.138532	0.026602	5.208
P10	inc5	ins	PRN	0.006426	0.037870	0.170
P10	inc5	ins	FC	0.000847	0.001769	0.479
P10	inc5	ins	WMC	0.000037	0.000042	0.889
P11	inc1	del	SEM	0.152564	0.037789	4.037
P11	inc1	del	PRN	0.006231	0.011619	0.536
P11	inc1	del	FC	0.000214	0.000109	1.962
P11	inc1	del	WMC	0.000030	0.000042	0.702
P11	inc1	ins	SEM	0.145554	0.011635	12.510
P11	inc1	ins	PRN	0.005119	0.022081	0.232
P11	inc1	ins	FC	0.000212	0.000126	1.675
P11	inc1	ins	WMC	0.000030	0.000040	0.754
P11	inc3	del	SEM	0.125391	0.039399	3.183
P11	inc3	del	PRN	0.004548	0.026764	0.170
P11	inc3	del	FC	0.000261	0.000116	2.255
P11	inc3	del	WMC	0.000067	0.000039	1.701
P11	inc3	ins	SEM	0.133104	0.018303	7.272
P11	inc3	ins	PRN	0.003578	0.026395	0.136
P11	inc3	ins	FC	0.000208	0.000102	2.034
P11	inc3	ins	WMC	0.000028	0.000032	0.877
P11	inc5	del	SEM	0.118603	0.040901	2.900
P11	inc5	del	PRN	0.004125	0.028649	0.144
P11	inc5	del	FC	0.000209	0.000115	1.823
P11	inc5	del	WMC	0.000029	0.000032	0.914
P11	inc5	ins	SEM	0.136019	0.023475	5.794
P11	inc5	ins	PRN	0.004513	0.028662	0.157
P11	inc5	ins	FC	0.000218	0.000100	2.177
P11	inc5	ins	WMC	0.000029	0.000031	0.932
P12	inc1	del	SEM	0.706162	0.069146	10.213
P12	inc1	del	PRN	0.180070	0.149637	1.203
P12	inc1	del	FC	0.112910	0.053408	2.114
P12	inc1	del	WMC	0.000218	0.000203	1.074
P12	inc1	ins	SEM	0.706354	0.039483	17.890
P12	inc1	ins	PRN	0.161958	0.163607	0.990
P12	inc1	ins	FC	0.115098	0.226074	0.509
P12	inc1	ins	WMC	0.000313	0.000512	0.611
P12	inc3	del	SEM	0.580622	0.199154	2.915
P12	inc3	del	PRN	0.120223	0.258237	0.466
P12	inc3	del	FC	0.085617	0.054422	1.573
P12	inc3	del	WMC	0.000184	0.000244	0.753
P12	inc3	ins	SEM	0.755593	0.128586	5.876
P12	inc3	ins	PRN	0.179580	0.920395	0.195
P12	inc3	ins	FC	0.124589	0.415761	0.300
P12	inc3	ins	WMC	0.000342	0.000756	0.452
P12	inc5	del	SEM	0.507442	0.343902	1.476
P12	inc5	del	PRN	0.107783	0.381928	0.282
P12	inc5	del	FC	0.082839	0.057025	1.453
P12	inc5	del	WMC	0.000177	0.000173	1.022
P12	inc5	ins	SEM	0.818368	0.263307	3.108
P12	inc5	ins	PRN	0.181420	1.027531	0.177
P12	inc5	ins	FC	0.121804	0.410560	0.297
P12	inc5	ins	WMC	0.000294	0.000583	0.504
P13	inc1	del	SEM	1.376175	0.177471	7.754
P13	inc1	del	PRN	0.209882	0.264800	0.793
P13	inc1	del	FC	0.118220	0.083413	1.417
P13	inc1	del	WMC	0.000509	0.000813	0.626
P13	inc1	ins	SEM	1.640598	0.080342	20.420
P13	inc1	ins	PRN	0.247054	0.268913	0.919
P13	inc1	ins	FC	0.348457	0.748099	0.466
P13	inc1	ins	WMC	0.001340	0.001329	1.009
P13	inc3	del	SEM	1.355083	0.412714	3.283
P13	inc3	del	PRN	0.198196	0.419911	0.472
P13	inc3	del	FC	0.100041	0.086767	1.153
P13	inc3	del	WMC	0.000278	0.000475	0.587
P13	inc3	ins	SEM	1.595062	0.193344	8.250
P13	inc3	ins	PRN	0.247303	0.560871	0.441
P13	inc3	ins	FC	0.348970	0.785185	0.444
P13	inc3	ins	WMC	0.001123	0.001694	0.663
P13	inc5	del	SEM	1.078506	0.647298	1.666
P13	inc5	del	PRN	0.170888	0.570788	0.299
P13	inc5	del	FC	0.106741	0.104288	1.024
P13	inc5	del	WMC	0.000234	0.000557	0.420
P13	inc5	ins	SEM	1.573994	0.343302	4.585
P13	inc5	ins	PRN	0.231334	0.780910	0.296
P13	inc5	ins	FC	0.340908	0.597854	0.570
P13	inc5	ins	WMC	0.001159	0.001318	0.880
P14	inc1	del	SEM	2.392938	0.301984	7.924
P14	inc1	del	PRN	0.215407	0.343602	0.627
P14	inc1	del	FC	0.387862	0.079331	4.889
P14	inc1	del	WMC	0.001033	0.001008	1.025
P14	inc1	ins	SEM	2.715976	0.117581	23.099
P14	inc1	ins	PRN	0.329294	1.526004	0.216
P14	inc1	ins	FC	0.699357	1.144845	0.611
P14	inc1	ins	WMC	0.001656	0.002153	0.769
P14	inc3	del	SEM	1.971623	0.883591	2.231
P14	inc3	del	PRN	0.178619	0.597094	0.299
P14	inc3	del	FC	0.055217	0.069663	0.793
P14	inc3	del	WMC	0.000295	0.000782	0.377
P14	inc3	ins	SEM	2.842919	0.342532	8.300
P14	inc3	ins	PRN	0.307377	2.252709	0.136
P14	inc3	ins	FC	0.690042	0.921871	0.749
P14	inc3	ins	WMC	0.001683	0.002056	0.819
P14	inc5	del	SEM	1.680476	1.129641	1.488
P14	inc5	del	PRN	0.131941	0.917638	0.144
P14	inc5	del	FC	0.030098	0.057168	0.526
P14	inc5	del	WMC	0.000226	0.000442	0.512
P14	inc5	ins	SEM	2.713648	0.704194	3.854
P14	inc5	ins	PRN	0.286903	3.357879	0.085
P14	inc5	ins	FC	0.696758	1.139095	0.612
P14	inc5	ins	WMC	0.001694	0.002355	0.720
P15	inc1	del	SEM	6.273233	1.520492	4.126
P15	inc1	del	PRN	0.435408	0.995624	0.437
P15	inc1	del	FC	0.826746	0.197307	4.190
P15	inc1	del	WMC	0.001700	0.002664	0.638
P15	inc1	ins	SEM	6.872711	0.425052	16.169
P15	inc1	ins	PRN	0.575119	1.908556	0.301
P15	inc1	ins	FC	2.280432	2.085590	1.093
P15	inc1	ins	WMC	0.005589	0.005999	0.932
P15	inc3	del	SEM	5.400065	3.166347	1.705
P15	inc3	del	PRN	0.328415	1.857592	0.177
P15	inc3	del	FC	0.080458	0.144181	0.558
P15	inc3	del	WMC	0.000534	0.000905	0.590
P15	inc3	ins	SEM	6.997433	1.436146	4.872
P15	inc3	ins	PRN	0.589018	3.593563	0.164
P15	inc3	ins	FC	2.183685	1.935416	1.128
P15	inc3	ins	WMC	0.003759	0.005819	0.646
P15	inc5	del	SEM	4.450649	3.969988	1.121
P15	inc5	del	PRN	0.300337	2.621763	0.115
P15	inc5	del	FC	0.069302	0.144472	0.480
P15	inc5	del	WMC	0.000329	0.000695	0.474
P15	inc5	ins	SEM	6.703326	2.242605	2.989
P15	inc5	ins	PRN	0.616994	4.564118	0.135
P15	inc5	ins	FC	2.192157	1.960262	1.118
P15	inc5	ins	WMC	0.003659	0.005718	0.640
P16	inc1	del	SEM	9.711456	4.291374	2.263
P16	inc1	del	PRN	0.584370	1.400244	0.417
P16	inc1	del	FC	0.716000	0.313340	2.285
P16	inc1	del	WMC	0.002050	0.002878	0.713
P16	inc1	ins	SEM	10.817472	0.645041	16.770
P16	inc1	ins	PRN	0.917932	4.842083	0.190
P16	inc1	ins	FC	3.904436	4.819968	0.810
P16	inc1	ins	WMC	0.007676	0.011812	0.650
P16	inc3	del	SEM	8.573984	5.470151	1.567
P16	inc3	del	PRN	0.450589	2.791517	0.161
P16	inc3	del	FC	0.047553	0.240114	0.198
P16	inc3	del	WMC	0.000586	0.000983	0.596
P16	inc3	ins	SEM	10.703239	1.904506	5.620
P16	inc3	ins	PRN	0.954194	8.636341	0.110
P16	inc3	ins	FC	3.835040	6.326674	0.606
P16	inc3	ins	WMC	0.007555	0.024845	0.304
P16	inc5	del	SEM	7.482854	6.587379	1.136
P16	inc5	del	PRN	0.357360	4.142917	0.086
P16	inc5	del	FC	0.022690	0.227379	0.100
P16	inc5	del	WMC	0.000500	0.000872	0.574
P16	inc5	ins	SEM	10.516854	3.441247	3.056
P16	inc5	ins	PRN	0.963656	10.789722	0.089
P16	inc5	ins	FC	4.076262	4.746248	0.859
P16	inc5	ins	WMC	0.008252	0.011902	0.693
P17	inc1	del	SEM	14.759640	7.556252	1.953
P17	inc1	del	PRN	0.789784	2.067958	0.382
P17	inc1	del	FC	1.481357	0.536880	2.759
P17	inc1	del	WMC	0.002620	0.003321	0.789
P17	inc1	ins	SEM	15.657365	0.957138	16.359
P17	inc1	ins	PRN	1.316661	7.626537	0.173
P17	inc1	ins	FC	4.878439	9.104438	0.536
P17	inc1	ins	WMC	0.011332	0.018551	0.611
P17	inc3	del	SEM	12.956262	9.191922	1.410
P17	inc3	del	PRN	0.602789	4.069167	0.148
P17	inc3	del	FC	0.041900	0.407055	0.103
P17	inc3	del	WMC	0.000781	0.001588	0.492
P17	inc3	ins	SEM	15.656860	3.018116	5.188
P17	inc3	ins	PRN	1.357313	12.754430	0.106
P17	inc3	ins	FC	4.812331	8.234357	0.584
P17	inc3	ins	WMC	0.011447	0.024424	0.469
P17	inc5	del	SEM	11.574487	10.242238	1.130
P17	inc5	del	PRN	0.536938	6.098437	0.088
P17	inc5	del	FC	0.033334	0.379668	0.088
P17	inc5	del	WMC	0.000730	0.000998	0.731
P17	inc5	ins	SEM	16.134601	4.845710	3.330
P17	inc5	ins	PRN	1.519964	15.482508	0.098
P17	inc5	ins	FC	5.096244	7.109718	0.717
P17	inc5	ins	WMC	0.011968	0.019395	0.617
P18	inc1	del	SEM	18.454210	11.865074	1.555
P18	inc1	del	PRN	1.073524	2.643020	0.406
P18	inc1	del	FC	1.716826	0.706594	2.430
P18	inc1	del	WMC	0.002866	0.004171	0.687
P18	inc1	ins	SEM	20.306671	1.276674	15.906
P18	inc1	ins	PRN	1.933512	11.709643	0.165
P18	inc1	ins	FC	5.735151	4.120599	1.392
P18	inc1	ins	WMC	0.019568	0.020720	0.944
P18	inc3	del	SEM	16.183393	13.652050	1.185
P18	inc3	del	PRN	0.791877	5.553240	0.143
P18	inc3	del	FC	0.046186	0.645300	0.072
P18	inc3	del	WMC	0.001060	0.001653	0.641
P18	inc3	ins	SEM	19.669804	4.135282	4.757
P18	inc3	ins	PRN	2.034789	18.329526	0.111
P18	inc3	ins	FC	6.092775	4.134612	1.474
P18	inc3	ins	WMC	0.020893	0.025752	0.811
P18	inc5	del	SEM	14.690028	15.338593	0.958
P18	inc5	del	PRN	0.674955	8.298910	0.081
P18	inc5	del	FC	0.021409	0.613346	0.035
P18	inc5	del	WMC	0.000939	0.001249	0.752
P18	inc5	ins	SEM	19.821241	6.282177	3.155
P18	inc5	ins	PRN	1.651563	21.486107	0.077
P18	inc5	ins	FC	5.606612	4.337210	1.293
P18	inc5	ins	WMC	0.021032	0.020639	1.019
P19	inc1	del	SEM	23.004712	19.334280	1.190
P19	inc1	del	PRN	1.326849	3.847736	0.345
P19	inc1	del	FC	1.591398	1.147028	1.387
P19	inc1	del	WMC	0.003182	0.005445	0.584
P19	inc1	ins	SEM	27.851344	2.054866	13.554
P19	inc1	ins	PRN	2.374115	17.698753	0.134
P19	inc1	ins	FC	7.861757	5.886065	1.336
P19	inc1	ins	WMC	0.026029	0.038075	0.684
P19	inc3	del	SEM	20.971962	20.105154	1.043
P19	inc3	del	PRN	1.036829	7.138445	0.145
P19	inc3	del	FC	0.045277	0.907793	0.050
P19	inc3	del	WMC	0.001421	0.002104	0.675
P19	inc3	ins	SEM	27.365268	5.281558	5.181
P19	inc3	ins	PRN	2.338283	28.766281	0.081
P19	inc3	ins	FC	7.599500	6.382484	1.191
P19	inc3	ins	WMC	0.026272	0.060380	0.435
P19	inc5	del	SEM	18.709208	21.445900	0.872
P19	inc5	del	PRN	0.830907	10.407022	0.080
P19	inc5	del	FC	0.007060	0.908383	0.008
P19	inc5	del	WMC	0.001202	0.001486	0.809
P19	inc5	ins	SEM	27.096717	8.038793	3.371
P19	inc5	ins	PRN	2.437151	34.573206	0.070
P19	inc5	ins	FC	7.664833	5.348321	1.433
P19	inc5	ins	WMC	0.025820	0.036849	0.701
P20	inc1	del	SEM	17.438649	6.625350	2.632
P20	inc1	del	PRN	2.005902	2.858654	0.702
P20	inc1	del	FC	2.771887	1.805652	1.535
P20	inc1	del	WMC	0.012785	0.013082	0.977
P20	inc1	ins	SEM	18.201902	1.006348	18.087
P20	inc1	ins	PRN	2.403779	3.228741	0.744
P20	inc1	ins	FC	5.406622	3.919304	1.379
P20	inc1	ins	WMC	0.024990	0.015374	1.625
P20	inc3	del	SEM	16.014717	15.075597	1.062
P20	inc3	del	PRN	1.474861	4.531112	0.325
P20	inc3	del	FC	0.938678	2.050426	0.458
P20	inc3	del	WMC	0.005645	0.010999	0.513
P20	inc3	ins	SEM	18.287194	2.734843	6.687
P20	inc3	ins	PRN	2.357485	6.393569	0.369
P20	inc3	ins	FC	4.475122	7.800497	0.574
P20	inc3	ins	WMC	0.015949	0.020347	0.784
P20	inc5	del	SEM	15.035446	18.383152	0.818
P20	inc5	del	PRN	1.131045	5.965972	0.190
P20	inc5	del	FC	0.674798	1.959478	0.344
P20	inc5	del	WMC	0.004394	0.005278	0.833
P20	inc5	ins	SEM	18.059595	4.419234	4.087
P20	inc5	ins	PRN	2.372707	12.468033	0.190
P20	inc5	ins	FC	4.488667	8.463204	0.530
P20	inc5	ins	WMC	0.017387	0.019268	0.902
```

### SEM summary (avg ΔE/|E| + avg times)
Avg ΔE/|E| uses applyDelta rule-app counts (pre-prune) and apply_delta_graph totalEdges (post-applyDelta, pre-prune). Speedup = AvgFull / AvgInc.
#### 2026-01-11 (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_trimmed_eval_small.
```tsv
DeltaPct	Turn	AvgFullSem_s	AvgIncSem_s	AvgSpeedup	AvgDeltaEdgeRatio_pct
0.001	ins	0.12	0.02	4.74	9.8
0.001	del	0.10	0.02	4.47	10.9
0.003	ins	0.11	0.03	3.71	24.7
0.003	del	0.09	0.03	3.35	32.7
0.005	ins	0.11	0.03	3.55	33.9
0.005	del	0.08	0.03	2.48	51.2
```
#### 2026-01-11 (full ruleset, det-opt, apply_delta_graph)
Run: 2026-01-11, full ruleset, det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_eval.
```tsv
DeltaPct	Turn	AvgFullSem_s	AvgIncSem_s	AvgSpeedup	AvgDeltaEdgeRatio_pct
0.01	ins	0.27	0.10	2.60	47.4
0.01	del	0.23	0.16	1.46	90.2
0.03	ins	0.26	0.15	1.74	67.1
0.03	del	0.19	0.31	0.62	204.3
0.05	ins	0.26	0.19	1.36	75.0
0.05	del	0.17	0.42	0.41	300.1
```

#### 2026-01-11 (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_trimmed_eval.
```tsv
DeltaPct	Turn	AvgFullSem_s	AvgIncSem_s	AvgSpeedup	AvgDeltaEdgeRatio_pct
0.01	ins	0.12	0.04	2.69	47.6
0.01	del	0.08	0.04	1.95	90.9
0.03	ins	0.12	0.05	2.56	67.7
0.03	del	0.07	0.05	1.51	209.6
0.05	ins	0.12	0.05	2.18	75.4
0.05	del	0.05	0.05	1.03	306.1
```

Delete SEM speedup (mean/median of per-case Full_SEM/Inc_SEM, delete turns).
```tsv
Delta	FullMean	FullMedian	TrimmedMean	TrimmedMedian
inc1	1.107	1.380	1.663	1.642
inc3	0.522	0.555	1.224	0.884
inc5	0.379	0.381	0.708	0.795
```

Delete SEM speedup (mean/median of per-case Full_SEM/Inc_SEM, delete turns; trimmed 0.1/0.3/0.5%).
```tsv
Delta	TrimmedMean	TrimmedMedian
inc0p1	3.591	4.412
inc0p3	2.803	2.821
inc0p5	2.310	2.179
```

### SEM speedup vs DeltaE/|E| scatter (0.05s filter, trimmed 0.1/0.3/0.5%)
Points include both del/ins turns with Full_SEM_s and Inc_SEM_s >= 0.05s to reduce
fixed-overhead noise. Colors: delete=blue, insert=orange. The gray dashed curve
is the ideal 1/(DeltaE/|E|) relationship (clipped for visibility).
Plot: `img/sem_speedup_vs_delta_ratio_filtered_50ms/sem_speedup_vs_delta_ratio_filtered_50ms_by_turn_with_ideal_nofit.png`.
Summary table: `img/sem_speedup_vs_delta_ratio_filtered_50ms/sem_speedup_vs_delta_ratio_filtered_50ms_summary.tsv`.
Source table + script: `img/sem_speedup_vs_delta_ratio_filtered_50ms/semnaive-sem-delta-ratio.tsv`,
`img/sem_speedup_vs_delta_ratio_filtered_50ms/plot_sem_speedup_vs_delta_ratio_filtered_50ms.py`.

Legacy SEM summary tables below are pre apply_delta_graph (ΔE/|E| used pruned view edges).

#### 2026-01-11 (full ruleset, det-opt)
Legacy run: 2026-01-11, full ruleset, det-opt, pre apply_delta_graph; ΔE/|E| uses pruned view edges (do not compare).
```tsv
DeltaPct	Turn	AvgFullSem_s	AvgIncSem_s	AvgSpeedup	AvgDeltaEdgeRatio_pct
0.01	ins	0.27	0.08	3.24	70.9
0.01	del	0.22	0.13	1.74	197.3
0.03	ins	0.26	0.08	3.08	100.3
0.03	del	0.20	0.18	1.14	704.8
0.05	ins	0.27	0.09	2.87	112.1
0.05	del	0.18	0.22	0.79	1486.0
```

### SEM del/ins time (inc vs full)
Speedup = Full_SEM_s / Inc_SEM_s (>1.0 means inc faster). DelDeltaEdgeRatio/InsDeltaEdgeRatio = ΔE/|E| (fraction) using apply_delta_graph totalEdges.
#### 2026-01-11 (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc0p1/inc0p3/inc0p5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_trimmed_eval_small.
```tsv
Case	Delta	IncDelSem_s	FullDelSem_s	DelSpeedup	DelDeltaEdgeRatio	IncInsSem_s	FullInsSem_s	InsSpeedup	InsDeltaEdgeRatio
P1	inc0p1	0.001337	0.004462	3.337	0.000000	0.000877	0.004459	5.084	0.000000
P1	inc0p3	0.001294	0.004789	3.701	0.000000	0.001761	0.004914	2.790	0.000000
P1	inc0p5	0.001850	0.004895	2.646	0.000000	0.002201	0.004703	2.137	0.000000
P3	inc0p1	0.000877	0.004446	5.070	0.000000	0.000889	0.004410	4.961	0.000000
P3	inc0p3	0.001602	0.004418	2.758	0.000000	0.000746	0.004350	5.831	0.000000
P3	inc0p5	0.000926	0.004419	4.772	0.000000	0.000925	0.004451	4.812	0.000000
P4	inc0p1	0.000909	0.000266	0.293	0.000000	0.001761	0.000301	0.171	0.000000
P4	inc0p3	0.000792	0.000248	0.313	0.000000	0.001131	0.000236	0.209	0.000000
P4	inc0p5	0.002054	0.000253	0.123	0.000000	0.000441	0.000229	0.519	0.000000
P5	inc0p1	0.001177	0.000268	0.228	0.833333	0.001045	0.000318	0.304	0.454545
P5	inc0p3	0.000621	0.000242	0.390	0.833333	0.002492	0.000304	0.122	0.454545
P5	inc0p5	0.000803	0.000253	0.315	0.833333	0.002410	0.000318	0.132	0.454545
P6	inc0p1	0.001169	0.000451	0.386	0.016667	0.001046	0.000409	0.391	0.016393
P6	inc0p3	0.002456	0.000465	0.189	0.016667	0.002647	0.000420	0.159	0.016393
P6	inc0p5	0.001437	0.000468	0.326	0.016667	0.002191	0.000529	0.241	0.016393
P7	inc0p1	0.001997	0.001030	0.516	0.023952	0.001919	0.000996	0.519	0.023392
P7	inc0p3	0.002429	0.001064	0.438	0.023952	0.001320	0.001063	0.805	0.023392
P7	inc0p5	0.001371	0.001088	0.794	0.266667	0.001967	0.001249	0.635	0.210526
P8	inc0p1	0.001457	0.001715	1.177	0.000000	0.001040	0.001491	1.434	0.000000
P8	inc0p3	0.001795	0.001634	0.910	0.000000	0.001229	0.001525	1.241	0.000000
P8	inc0p5	0.002366	0.001611	0.681	0.055147	0.001306	0.001529	1.171	0.052265
P9	inc0p1	0.001286	0.001257	0.977	0.034653	0.001156	0.001407	1.217	0.033493
P9	inc0p3	0.002341	0.001222	0.522	0.034653	0.001924	0.001107	0.575	0.033493
P9	inc0p5	0.001289	0.001198	0.929	0.050251	0.002088	0.001103	0.528	0.047847
P10	inc0p1	0.003102	0.017382	5.603	0.065356	0.008052	0.016505	2.050	0.061347
P10	inc0p3	0.004797	0.014270	2.975	0.268917	0.006660	0.016878	2.534	0.211926
P10	inc0p5	0.006738	0.013707	2.034	0.438001	0.005310	0.016625	3.131	0.304590
P11	inc0p1	0.009417	0.014945	1.587	0.157264	0.003209	0.016825	5.243	0.135893
P11	inc0p3	0.004124	0.013123	3.182	0.317794	0.006339	0.015848	2.500	0.241156
P11	inc0p5	0.005800	0.014515	2.503	0.875405	0.005728	0.019501	3.405	0.466782
P12	inc0p1	0.003384	0.014929	4.412	0.028889	0.003251	0.012602	3.876	0.028078
P12	inc0p3	0.003490	0.013814	3.958	0.050363	0.003236	0.013780	4.258	0.047948
P12	inc0p5	0.003690	0.013045	3.535	0.070768	0.002218	0.013609	6.136	0.066091
P13	inc0p1	0.004762	0.029434	6.181	0.025583	0.004537	0.024921	5.493	0.024945
P13	inc0p3	0.004828	0.026197	5.426	0.061138	0.005028	0.028873	5.742	0.057616
P13	inc0p5	0.005810	0.021999	3.786	0.146545	0.005834	0.027698	4.748	0.127815
P14	inc0p1	0.004783	0.040013	8.366	0.025544	0.004882	0.044723	9.161	0.024907
P14	inc0p3	0.005703	0.040072	7.026	0.066235	0.005614	0.041167	7.333	0.062120
P14	inc0p5	0.007684	0.041958	5.460	0.119316	0.007388	0.042660	5.774	0.106597
P15	inc0p1	0.018486	0.108737	5.882	0.067141	0.034138	0.110210	3.228	0.062917
P15	inc0p3	0.028815	0.095775	3.324	0.211377	0.025520	0.111738	4.378	0.174493
P15	inc0p5	0.034535	0.075237	2.179	0.273417	0.027915	0.097734	3.501	0.214711
P16	inc0p1	0.030880	0.163779	5.304	0.087447	0.037250	0.196405	5.273	0.080415
P16	inc0p3	0.060967	0.162215	2.661	0.339298	0.065006	0.201602	3.101	0.253340
P16	inc0p5	0.059488	0.137573	2.313	0.632186	0.059098	0.181818	3.077	0.387325
P17	inc0p1	0.049873	0.281173	5.638	0.114130	0.064152	0.327089	5.099	0.102439
P17	inc0p3	0.062241	0.237279	3.812	0.286050	0.067742	0.328813	4.854	0.222426
P17	inc0p5	0.085284	0.206403	2.420	0.512088	0.080864	0.325402	4.024	0.338663
P18	inc0p1	0.075882	0.359826	4.742	0.144294	0.078139	0.396444	5.074	0.126099
P18	inc0p3	0.112652	0.317769	2.821	0.580826	0.123031	0.379968	3.088	0.367419
P18	inc0p5	0.127900	0.272955	2.134	0.945548	0.131476	0.392006	2.982	0.486006
P19	inc0p1	0.140386	0.458766	3.268	0.213032	0.162099	0.567848	3.503	0.175619
P19	inc0p3	0.162682	0.394764	2.427	0.651683	0.178437	0.507724	2.845	0.394557
P19	inc0p5	0.205365	0.362572	1.766	0.959832	0.177267	0.527415	2.975	0.489752
P20	inc0p1	0.084684	0.445935	5.266	0.019362	0.064647	0.522146	8.077	0.018994
P20	inc0p3	0.073470	0.471718	6.421	0.058172	0.070169	0.457180	6.515	0.054974
P20	inc0p5	0.075693	0.391553	5.173	0.112589	0.081904	0.464711	5.674	0.101196
```
#### 2026-01-11 (full ruleset, det-opt, apply_delta_graph)
Run: 2026-01-11, full ruleset, det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	IncDelSem_s	FullDelSem_s	DelSpeedup	DelDeltaEdgeRatio	IncInsSem_s	FullInsSem_s	InsSpeedup	InsDeltaEdgeRatio
P1	inc1	0.005426	0.005949	1.096	0.000000	0.001506	0.005568	3.697	0.000000
P1	inc3	0.019603	0.004767	0.243	0.000000	0.003737	0.004770	1.276	0.000000
P1	inc5	0.018827	0.001490	0.079	0.000000	0.013487	0.004413	0.327	0.000000
P3	inc1	0.018805	0.002377	0.126	0.000000	0.006261	0.004394	0.702	0.000000
P3	inc3	0.019016	0.002250	0.118	1.000000	0.006041	0.004686	0.776	0.500000
P3	inc5	0.014696	0.001073	0.073	1.000000	0.010542	0.004469	0.424	0.500000
P4	inc1	0.001842	0.000342	0.186	0.000000	0.002716	0.000308	0.113	0.000000
P4	inc3	0.002448	0.000335	0.137	0.000000	0.002428	0.000406	0.167	0.000000
P4	inc5	0.002118	0.000382	0.180	0.000000	0.001917	0.000315	0.164	0.000000
P5	inc1	0.001689	0.000265	0.157	0.650000	0.001391	0.000306	0.220	0.393939
P5	inc3	0.001198	0.000281	0.235	0.650000	0.001429	0.000388	0.272	0.393939
P5	inc5	0.000921	0.000184	0.200	2.300000	0.001943	0.000319	0.164	0.696970
P6	inc1	0.006285	0.000745	0.119	0.015873	0.003410	0.000752	0.221	0.015625
P6	inc3	0.004428	0.000887	0.200	0.361702	0.002372	0.000731	0.308	0.265625
P6	inc5	0.003109	0.000648	0.208	0.454545	0.003838	0.000744	0.194	0.312500
P7	inc1	0.004001	0.001875	0.469	0.095808	0.008006	0.001939	0.242	0.087432
P7	inc3	0.003866	0.001825	0.472	0.220000	0.005045	0.002089	0.414	0.180328
P7	inc5	0.004204	0.001733	0.412	0.335766	0.003515	0.001962	0.558	0.251366
P8	inc1	0.004240	0.005327	1.256	0.178571	0.002971	0.004307	1.450	0.151515
P8	inc3	0.006255	0.004764	0.762	0.470297	0.003714	0.005635	1.517	0.319865
P8	inc5	0.007586	0.003227	0.425	0.800000	0.006153	0.004233	0.688	0.444444
P9	inc1	0.003866	0.002239	0.579	0.327273	0.003623	0.002551	0.704	0.246575
P9	inc3	0.008827	0.002331	0.264	0.825000	0.003432	0.002559	0.746	0.452055
P9	inc5	0.006254	0.001970	0.315	1.009174	0.003404	0.002433	0.715	0.502283
P10	inc1	0.016401	0.022626	1.380	0.937862	0.013756	0.029375	2.135	0.483968
P10	inc3	0.018088	0.016200	0.896	3.773469	0.015238	0.025992	1.706	0.790509
P10	inc5	0.018954	0.015023	0.793	5.879412	0.015600	0.027699	1.776	0.854639
P11	inc1	0.012252	0.019965	1.630	0.989700	0.010427	0.026763	2.567	0.497412
P11	inc3	0.015371	0.016677	1.085	4.028200	0.014536	0.028387	1.953	0.801122
P11	inc5	0.017463	0.016679	0.955	4.824121	0.013835	0.025602	1.851	0.828300
P12	inc1	0.022958	0.042451	1.849	0.175101	0.015938	0.045460	2.852	0.149009
P12	inc3	0.053465	0.035452	0.663	0.569980	0.031985	0.049778	1.556	0.363049
P12	inc5	0.079825	0.030687	0.384	1.124428	0.032396	0.043567	1.345	0.529285
P13	inc1	0.048741	0.073189	1.502	0.258324	0.025728	0.085161	3.310	0.205292
P13	inc3	0.123701	0.066443	0.537	0.860895	0.057483	0.083246	1.448	0.462624
P13	inc5	0.156645	0.054103	0.345	1.546322	0.060950	0.084723	1.390	0.607277
P14	inc1	0.070215	0.131847	1.878	0.370837	0.041328	0.133311	3.226	0.270519
P14	inc3	0.162903	0.098265	0.603	0.974261	0.066679	0.126148	1.892	0.493481
P14	inc5	0.247546	0.094283	0.381	1.679635	0.089933	0.130085	1.446	0.626815
P15	inc1	0.181820	0.265276	1.459	0.641602	0.102930	0.299944	2.914	0.390839
P15	inc3	0.387001	0.214627	0.555	2.248791	0.182655	0.276546	1.514	0.692193
P15	inc5	0.504491	0.188237	0.373	3.303631	0.240085	0.289823	1.207	0.767638
P16	inc1	0.319361	0.448604	1.405	1.149078	0.214506	0.516224	2.407	0.534684
P16	inc3	0.642074	0.382780	0.596	2.706541	0.290410	0.511636	1.762	0.730207
P16	inc5	0.820305	0.319658	0.390	3.939064	0.406757	0.468832	1.153	0.797532
P17	inc1	0.418753	0.626876	1.497	1.230383	0.280982	0.710549	2.529	0.551647
P17	inc3	0.925038	0.501872	0.543	2.894184	0.443286	0.715911	1.615	0.743207
P17	inc5	1.099398	0.486997	0.443	4.121659	0.499823	0.680069	1.361	0.804751
P18	inc1	0.568449	0.733412	1.290	1.455688	0.379410	0.909348	2.397	0.592782
P18	inc3	1.043887	0.650327	0.623	3.172946	0.509513	0.933957	1.833	0.760361
P18	inc5	1.499644	0.608425	0.406	4.523691	0.655736	0.975995	1.488	0.818962
P19	inc1	0.734954	1.026818	1.397	1.785124	0.520638	1.299704	2.496	0.640950
P19	inc3	1.423235	0.862460	0.606	3.689343	0.732256	1.180612	1.612	0.786750
P19	inc5	1.994920	0.722430	0.362	5.164391	0.891869	1.196871	1.342	0.837778
P20	inc1	0.519754	0.912271	1.755	0.216332	0.322497	1.005359	3.117	0.177856
P20	inc3	1.009805	0.790884	0.783	0.707793	0.480662	1.002361	2.085	0.414449
P20	inc5	1.524599	0.729993	0.479	1.219568	0.636884	0.928317	1.458	0.549462
```

#### 2026-01-11 (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_trimmed_eval.
```tsv
Case	Delta	IncDelSem_s	FullDelSem_s	DelSpeedup	DelDeltaEdgeRatio	IncInsSem_s	FullInsSem_s	InsSpeedup	InsDeltaEdgeRatio
P1	inc1	0.002514	0.004743	1.887	0.000000	0.000987	0.004392	4.448	0.000000
P1	inc3	0.000980	0.004344	4.431	0.000000	0.001215	0.004562	3.755	0.000000
P1	inc5	0.019502	0.001337	0.069	0.000000	0.008072	0.004464	0.553	0.000000
P3	inc1	0.019236	0.002458	0.128	0.000000	0.006778	0.004907	0.724	0.000000
P3	inc3	0.018872	0.002005	0.106	0.000000	0.006775	0.004519	0.667	0.000000
P3	inc5	0.019515	0.001783	0.091	0.000000	0.007456	0.006603	0.886	0.000000
P4	inc1	0.002511	0.000242	0.096	0.266667	0.001367	0.000261	0.191	0.210526
P4	inc3	0.001048	0.000228	0.218	0.266667	0.000871	0.000344	0.395	0.210526
P4	inc5	0.000539	0.000234	0.434	0.357143	0.002628	0.000234	0.089	0.263158
P5	inc1	0.001770	0.000298	0.169	0.178571	0.001114	0.000299	0.268	0.151515
P5	inc3	0.001214	0.000295	0.243	0.178571	0.002563	0.000395	0.154	0.151515
P5	inc5	0.002252	0.000311	0.138	0.434783	0.001292	0.000355	0.275	0.303030
P6	inc1	0.001690	0.000494	0.292	0.000000	0.001266	0.000417	0.329	0.000000
P6	inc3	0.001962	0.000387	0.197	0.525000	0.001318	0.000516	0.392	0.344262
P6	inc5	0.001505	0.000388	0.258	0.564103	0.001223	0.000491	0.402	0.360656
P7	inc1	0.001241	0.000918	0.740	0.155405	0.001470	0.000986	0.671	0.134503
P7	inc3	0.001814	0.001008	0.556	0.390244	0.001994	0.001194	0.599	0.280702
P7	inc5	0.004235	0.000777	0.184	0.628571	0.002830	0.001042	0.368	0.385965
P8	inc1	0.001447	0.001619	1.119	0.121094	0.001378	0.002586	1.877	0.108014
P8	inc3	0.001548	0.001140	0.736	0.839744	0.001501	0.001791	1.193	0.456446
P8	inc5	0.002834	0.001054	0.372	1.296000	0.002585	0.001975	0.764	0.564460
P9	inc1	0.001394	0.001463	1.050	0.094241	0.001204	0.001239	1.029	0.086124
P9	inc3	0.002986	0.001117	0.374	0.148352	0.001173	0.001157	0.986	0.129187
P9	inc5	0.001262	0.000989	0.784	0.402685	0.001295	0.001137	0.878	0.287081
P10	inc1	0.006068	0.010677	1.760	1.034031	0.005938	0.017543	2.955	0.508366
P10	inc3	0.007867	0.007039	0.895	4.420930	0.010369	0.016966	1.636	0.815530
P10	inc5	0.008821	0.006235	0.707	5.334239	0.009525	0.014899	1.564	0.842128
P11	inc1	0.007544	0.011549	1.531	1.379877	0.006399	0.021025	3.286	0.579810
P11	inc3	0.007554	0.006676	0.884	5.817647	0.007536	0.019375	2.571	0.853322
P11	inc5	0.008546	0.006796	0.795	7.133333	0.008368	0.019100	2.282	0.877049
P12	inc1	0.004136	0.013257	3.205	0.203848	0.003676	0.012471	3.393	0.169330
P12	inc3	0.005048	0.011190	2.217	0.574830	0.004250	0.018049	4.247	0.365011
P12	inc5	0.006657	0.009671	1.453	0.980325	0.005519	0.017568	3.183	0.495032
P13	inc1	0.007896	0.033115	4.194	0.284378	0.006199	0.039622	6.391	0.221413
P13	inc3	0.010539	0.020484	1.944	1.022321	0.009800	0.033269	3.395	0.505519
P13	inc5	0.012813	0.012205	0.953	1.879847	0.010787	0.035694	3.309	0.652759
P14	inc1	0.013184	0.038203	2.898	0.384158	0.011808	0.062351	5.280	0.277539
P14	inc3	0.023567	0.019084	0.810	1.433261	0.019502	0.039272	2.014	0.589029
P14	inc5	0.017414	0.017785	1.021	2.379259	0.016584	0.048149	2.903	0.704077
P15	inc1	0.035372	0.083113	2.350	0.495016	0.043150	0.119745	2.775	0.331111
P15	inc3	0.043043	0.064004	1.487	1.650000	0.046392	0.095203	2.052	0.622642
P15	inc5	0.045516	0.045485	0.999	3.527296	0.051747	0.113216	2.188	0.779118
P16	inc1	0.072585	0.147153	2.027	1.248117	0.076654	0.259013	3.379	0.555183
P16	inc3	0.075636	0.090289	1.194	2.651052	0.097046	0.217355	2.240	0.726106
P16	inc5	0.083557	0.068150	0.816	3.889710	0.101574	0.216881	2.135	0.795489
P17	inc1	0.099465	0.175295	1.762	1.412578	0.123004	0.350136	2.847	0.585506
P17	inc3	0.123260	0.134602	1.092	3.419594	0.136419	0.303583	2.225	0.773735
P17	inc5	0.133294	0.119744	0.898	4.477654	0.137578	0.310351	2.256	0.817440
P18	inc1	0.165667	0.271961	1.642	1.579121	0.162323	0.389432	2.399	0.612271
P18	inc3	0.171126	0.151291	0.884	3.327010	0.167023	0.462724	2.770	0.768894
P18	inc5	0.163466	0.138465	0.847	4.492256	0.183545	0.420255	2.290	0.817925
P19	inc1	0.229725	0.355481	1.547	1.630072	0.270420	0.548455	2.028	0.619782
P19	inc3	0.228191	0.198553	0.870	3.646377	0.266115	0.502165	1.887	0.784779
P19	inc5	0.193918	0.212476	1.096	4.706941	0.294687	0.488407	1.657	0.824775
P20	inc1	0.127433	0.408499	3.206	0.201518	0.122291	0.446217	3.649	0.167719
P20	inc3	0.145318	0.598049	4.115	0.713965	0.129668	0.610870	4.711	0.416558
P20	inc5	0.198381	0.303287	1.529	1.255291	0.191767	0.566586	2.955	0.556598
```

Legacy tables below omit ΔE/|E| columns and were collected before apply_delta_graph logging.
#### 2026-01-11 (full ruleset, det-opt)
Legacy run: 2026-01-11, full ruleset, det-opt, pre apply_delta_graph; ΔE/|E| uses pruned view edges (do not compare).
```tsv
Case	Delta	IncDelSem_s	FullDelSem_s	DelSpeedup	IncInsSem_s	FullInsSem_s	InsSpeedup
P1	inc1	0.001509	0.004251	2.817	0.005950	0.004478	0.753
P1	inc3	0.007915	0.003155	0.399	0.001304	0.004452	3.414
P1	inc5	0.010097	0.001473	0.146	0.002672	0.004766	1.784
P3	inc1	0.015203	0.002297	0.151	0.003420	0.004700	1.374
P3	inc3	0.011148	0.002324	0.208	0.001191	0.004608	3.869
P3	inc5	0.009461	0.001052	0.111	0.002811	0.004785	1.702
P4	inc1	0.004944	0.000302	0.061	0.001038	0.000442	0.426
P4	inc3	0.002161	0.000312	0.144	0.002006	0.000325	0.162
P4	inc5	0.001742	0.000392	0.225	0.002253	0.000307	0.136
P5	inc1	0.006048	0.000242	0.040	0.003256	0.000302	0.093
P5	inc3	0.001235	0.000261	0.211	0.002696	0.000304	0.113
P5	inc5	0.001952	0.000204	0.105	0.003192	0.000320	0.100
P6	inc1	0.013218	0.000831	0.063	0.004374	0.000879	0.201
P6	inc3	0.012503	0.000724	0.058	0.008265	0.000723	0.087
P6	inc5	0.004099	0.000676	0.165	0.005760	0.000741	0.129
P7	inc1	0.014671	0.002124	0.145	0.004589	0.002021	0.440
P7	inc3	0.004234	0.002022	0.478	0.007001	0.002694	0.385
P7	inc5	0.002656	0.001698	0.639	0.001980	0.002232	1.127
P8	inc1	0.022280	0.003797	0.170	0.007588	0.004016	0.529
P8	inc3	0.007939	0.003454	0.435	0.006126	0.003848	0.628
P8	inc5	0.006612	0.004138	0.626	0.002592	0.004168	1.608
P9	inc1	0.018669	0.002218	0.119	0.007336	0.002604	0.355
P9	inc3	0.010515	0.002041	0.194	0.005795	0.002537	0.438
P9	inc5	0.006511	0.002015	0.309	0.003643	0.002585	0.710
P10	inc1	0.020477	0.020981	1.025	0.013681	0.026745	1.955
P10	inc3	0.013988	0.016264	1.163	0.017276	0.026539	1.536
P10	inc5	0.017872	0.016651	0.932	0.014015	0.027808	1.984
P11	inc1	0.018087	0.020579	1.138	0.011481	0.031059	2.705
P11	inc3	0.016069	0.018832	1.172	0.014512	0.028418	1.958
P11	inc5	0.016590	0.015537	0.937	0.013775	0.026677	1.937
P12	inc1	0.024790	0.049613	2.001	0.011840	0.045153	3.814
P12	inc3	0.029860	0.037833	1.267	0.013767	0.047211	3.429
P12	inc5	0.041353	0.032127	0.777	0.010736	0.050788	4.731
P13	inc1	0.039754	0.079615	2.003	0.019020	0.089094	4.684
P13	inc3	0.056309	0.068048	1.208	0.019942	0.093261	4.677
P13	inc5	0.091148	0.056393	0.619	0.022039	0.082277	3.733
P14	inc1	0.057824	0.108704	1.880	0.028363	0.133728	4.715
P14	inc3	0.094255	0.097584	1.035	0.031611	0.131120	4.148
P14	inc5	0.120522	0.084118	0.698	0.034240	0.126602	3.697
P15	inc1	0.174149	0.273208	1.569	0.112487	0.307830	2.737
P15	inc3	0.222286	0.229333	1.032	0.083295	0.300941	3.613
P15	inc5	0.294825	0.216101	0.733	0.095902	0.372062	3.880
P16	inc1	0.242774	0.425021	1.751	0.134090	0.551677	4.114
P16	inc3	0.323300	0.421373	1.303	0.152172	0.462971	3.042
P16	inc5	0.446609	0.349955	0.784	0.174257	0.508594	2.919
P17	inc1	0.320419	0.563367	1.758	0.194515	0.706945	3.634
P17	inc3	0.483841	0.655752	1.355	0.228624	0.748730	3.275
P17	inc5	0.624983	0.486171	0.778	0.246373	0.721795	2.930
P18	inc1	0.423318	0.754565	1.783	0.322139	0.967544	3.003
P18	inc3	0.598993	0.606934	1.013	0.318369	0.960113	3.016
P18	inc5	0.789984	0.623686	0.789	0.317627	0.945563	2.977
P19	inc1	0.573492	1.004522	1.752	0.392632	1.185190	3.019
P19	inc3	0.824897	0.864874	1.048	0.389017	1.192149	3.065
P19	inc5	1.018640	0.687928	0.675	0.476608	1.196731	2.511
P20	inc1	0.432936	0.896109	2.070	0.288688	1.013685	3.511
P20	inc3	0.636448	0.808855	1.271	0.311388	0.966461	3.104
P20	inc5	0.761576	0.806323	1.059	0.354827	1.050306	2.960
```
#### 2026-01-10 (full ruleset, equal_assign)
Legacy run: 2026-01-10, full ruleset (equal_assign), inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	IncDelSem_s	FullDelSem_s	DelSpeedup	IncInsSem_s	FullInsSem_s	InsSpeedup
P1	inc1	0.003080	0.308619	100.193	0.001686	0.374344	221.972
P1	inc3	0.152906	0.163037	1.066	0.048598	0.271973	5.596
P1	inc5	0.143623	0.053398	0.372	0.120375	0.281798	2.341
P3	inc1	0.133366	0.104790	0.786	0.104623	0.278659	2.663
P3	inc3	0.125322	0.105291	0.840	0.075171	0.289040	3.845
P3	inc5	0.109813	0.030692	0.279	0.121540	0.275340	2.265
P4	inc1	0.001209	0.001471	1.216	0.000723	0.001402	1.938
P4	inc3	0.000575	0.001379	2.400	0.000647	0.001398	2.159
P4	inc5	0.000545	0.001357	2.489	0.000679	0.001390	2.047
P5	inc1	0.001691	0.000403	0.238	0.001123	0.000450	0.401
P5	inc3	0.000851	0.000307	0.361	0.000675	0.000341	0.505
P5	inc5	0.000833	0.000230	0.276	0.000823	0.000339	0.412
P6	inc1	0.002067	0.006450	3.121	0.001593	0.005883	3.694
P6	inc3	0.002108	0.006460	3.064	0.001430	0.007524	5.261
P6	inc5	0.001707	0.006187	3.625	0.001423	0.006507	4.572
P7	inc1	0.006067	0.024047	3.963	0.002191	0.022083	10.077
P7	inc3	0.005189	0.022926	4.418	0.002552	0.021937	8.594
P7	inc5	0.006266	0.032670	5.214	0.004370	0.024209	5.540
P8	inc1	0.005588	0.086423	15.465	0.002195	0.092403	42.089
P8	inc3	0.022903	0.079150	3.456	0.005852	0.084835	14.497
P8	inc5	0.024602	0.067322	2.736	0.011488	0.083994	7.311
P9	inc1	0.006424	0.041433	6.449	0.003908	0.044510	11.391
P9	inc3	0.004028	0.040597	10.078	0.003945	0.045158	11.448
P9	inc5	0.017907	0.035834	2.001	0.006311	0.046493	7.367
P10	inc1	0.042013	0.226347	5.388	0.012684	0.259165	20.432
P10	inc3	0.036504	0.227994	6.246	0.020437	0.229410	11.225
P10	inc5	0.041479	0.234801	5.661	0.028447	0.267687	9.410
P11	inc1	0.044417	0.259653	5.846	0.012636	0.277955	21.997
P11	inc3	0.040740	0.227570	5.586	0.021466	0.260369	12.129
P11	inc5	0.044790	0.228618	5.104	0.027646	0.272081	9.842
P12	inc1	0.060281	1.586246	26.314	0.036487	2.419714	66.317
P12	inc3	0.235974	0.951421	4.032	0.152247	1.351382	8.876
P12	inc5	0.347674	0.863031	2.482	0.285558	1.192526	4.176
P13	inc1	0.212703	2.245982	10.559	0.079152	3.584528	45.287
P13	inc3	0.547251	1.896529	3.466	0.297707	3.199152	10.746
P13	inc5	0.939059	1.621187	1.726	0.702775	2.341568	3.332
P14	inc1	0.393562	3.481304	8.846	0.130845	4.301843	32.877
P14	inc3	1.053372	2.940271	2.791	0.528349	3.676304	6.958
P14	inc5	1.572643	2.453899	1.560	0.933557	3.550889	3.804
P15	inc1	1.559447	13.138471	8.425	0.409398	9.890460	24.159
P15	inc3	2.931849	7.293411	2.488	1.363925	9.217068	6.758
P15	inc5	3.564849	6.093589	1.709	2.188749	9.350145	4.272
P16	inc1	4.104828	13.325187	3.246	0.867676	14.419324	16.618
P16	inc3	5.847362	11.907698	2.036	2.244387	14.285393	6.365
P16	inc5	6.828901	10.726381	1.571	3.635544	15.019261	4.131
P17	inc1	7.508286	19.783993	2.635	1.049936	21.167731	20.161
P17	inc3	9.361490	17.842647	1.906	3.045086	20.845412	6.846
P17	inc5	10.990110	15.255336	1.388	4.829687	21.058234	4.360
P18	inc1	12.008071	24.164998	2.012	1.371948	27.236640	19.853
P18	inc3	13.843759	21.498865	1.553	4.583560	26.043408	5.682
P18	inc5	15.362085	19.051526	1.240	7.121243	25.993145	3.650
P19	inc1	18.830237	31.182333	1.656	2.230873	40.772985	18.277
P19	inc3	20.662074	27.125491	1.313	7.197824	37.194128	5.167
P19	inc5	21.502240	24.128818	1.122	10.776478	36.095263	3.349
P20	inc1	7.224893	23.230283	3.215	0.921280	23.458028	25.462
P20	inc3	15.019120	20.872690	1.390	2.927604	24.262939	8.288
P20	inc5	18.150697	18.968011	1.045	4.724020	23.712349	5.020
```

#### 2026-01-10 (trimmed ruleset, no equal_assign)
Legacy run: 2026-01-10, trimmed ruleset (no equal_assign), inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_trimmed_eval.
```tsv
Case	Delta	IncDelSem_s	FullDelSem_s	DelSpeedup	IncInsSem_s	FullInsSem_s	InsSpeedup
P1	inc1	0.001535	0.385245	250.984	0.001224	0.521563	426.080
P1	inc3	0.000876	0.298912	341.231	0.000835	0.377565	452.009
P1	inc5	0.151882	0.052206	0.344	0.139657	0.295593	2.117
P3	inc1	0.137226	0.085397	0.622	0.113269	0.295461	2.609
P3	inc3	0.129838	0.081318	0.626	0.120856	0.301446	2.494
P3	inc5	0.133459	0.078053	0.585	0.122928	0.289006	2.351
P4	inc1	0.001424	0.000267	0.188	0.000782	0.000285	0.365
P4	inc3	0.001536	0.000271	0.177	0.004219	0.000290	0.069
P4	inc5	0.000460	0.000275	0.599	0.000682	0.000288	0.422
P5	inc1	0.001921	0.000353	0.184	0.001445	0.000357	0.247
P5	inc3	0.001559	0.000321	0.206	0.001460	0.000486	0.333
P5	inc5	0.001764	0.000318	0.180	0.001349	0.000369	0.274
P6	inc1	0.001331	0.000577	0.434	0.001175	0.000511	0.435
P6	inc3	0.001073	0.000498	0.464	0.001593	0.000551	0.346
P6	inc5	0.001005	0.000500	0.497	0.001087	0.000567	0.522
P7	inc1	0.001729	0.001283	0.742	0.002042	0.001337	0.655
P7	inc3	0.002003	0.001144	0.571	0.002106	0.001355	0.643
P7	inc5	0.002202	0.001134	0.515	0.002157	0.001322	0.613
P8	inc1	0.002342	0.002143	0.915	0.002291	0.002062	0.900
P8	inc3	0.002270	0.001668	0.735	0.001243	0.002148	1.728
P8	inc5	0.001689	0.001627	0.963	0.002270	0.002301	1.014
P9	inc1	0.001792	0.001482	0.827	0.001506	0.001361	0.904
P9	inc3	0.001140	0.001557	1.366	0.002248	0.001411	0.628
P9	inc5	0.001722	0.001309	0.760	0.001698	0.001515	0.892
P10	inc1	0.007109	0.016497	2.321	0.007115	0.022307	3.135
P10	inc3	0.007572	0.011049	1.459	0.006946	0.027290	3.929
P10	inc5	0.009497	0.010862	1.144	0.008745	0.020957	2.396
P11	inc1	0.006090	0.014723	2.418	0.005739	0.020583	3.586
P11	inc3	0.006840	0.011051	1.616	0.007046	0.021368	3.033
P11	inc5	0.006432	0.010305	1.602	0.007001	0.024023	3.431
P12	inc1	0.003826	0.017927	4.686	0.003104	0.018963	6.108
P12	inc3	0.003982	0.015555	3.906	0.003466	0.018792	5.422
P12	inc5	0.005125	0.013238	2.583	0.004588	0.020125	4.386
P13	inc1	0.006497	0.031940	4.916	0.005295	0.038475	7.266
P13	inc3	0.009377	0.026386	2.814	0.009386	0.041025	4.371
P13	inc5	0.009931	0.024283	2.445	0.008919	0.044441	4.983
P14	inc1	0.009290	0.047071	5.067	0.008696	0.056581	6.507
P14	inc3	0.014381	0.036507	2.539	0.017245	0.067469	3.912
P14	inc5	0.015297	0.032569	2.129	0.015090	0.060942	4.039
P15	inc1	0.032168	0.113367	3.524	0.031465	0.147830	4.698
P15	inc3	0.037838	0.095963	2.536	0.041470	0.130419	3.145
P15	inc5	0.048718	0.109446	2.247	0.057390	0.158768	2.766
P16	inc1	0.057738	0.168664	2.921	0.057660	0.239183	4.148
P16	inc3	0.065251	0.151248	2.318	0.070245	0.240819	3.428
P16	inc5	0.076431	0.241312	3.157	0.075052	0.242772	3.235
P17	inc1	0.100536	0.230810	2.296	0.096283	0.396708	4.120
P17	inc3	0.111565	0.198731	1.781	0.107714	0.390036	3.621
P17	inc5	0.110010	0.194008	1.764	0.117621	0.374737	3.186
P18	inc1	0.130858	0.335190	2.561	0.153409	0.461087	3.006
P18	inc3	0.128429	0.285182	2.221	0.152332	0.455197	2.988
P18	inc5	0.119644	0.234277	1.958	0.156283	0.508392	3.253
P19	inc1	0.179769	0.472441	2.628	0.225704	0.706978	3.132
P19	inc3	0.177076	0.370329	2.091	0.220714	0.624964	2.832
P19	inc5	0.187138	0.345540	1.846	0.260906	0.608440	2.332
P20	inc1	0.092535	0.629298	6.801	0.099873	0.673522	6.744
P20	inc3	0.146947	0.461004	3.137	0.136890	0.728237	5.320
P20	inc5	0.150087	0.471920	3.144	0.149270	0.609385	4.082
```

### Pre-prune delta edge ratios (apply_delta_view / total edges)
Legacy run: pre apply_delta_graph; ratios based on apply_delta_view vs pruned total edges (do not compare).
DeltaEdges come from apply_delta_view (pre-prune); total edges come from dumpStatisticsInc (full graph).
Delete ratios can exceed 1.0 when removed edges outnumber the remaining edges after the delete turn.
#### 2026-01-10 (full ruleset, equal_assign)
Legacy run: 2026-01-10, full ruleset (equal_assign), inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
Collected via `--dumpstat --derv-only` to read dumpStatisticsInc totals.
```tsv
Case	Delta	DelEdges	DelTotalEdges	DelEdgeRatio	InsEdges	InsTotalEdges	InsEdgeRatio
P1	inc1	406	88293	0.004598	406	88699	0.004577
P1	inc3	30981	57718	0.536765	30981	88699	0.349282
P1	inc5	68773	19926	3.451420	68773	88699	0.775353
P3	inc1	51493	37206	1.383997	51493	88699	0.580536
P3	inc3	51603	37096	1.391066	51603	88699	0.581777
P3	inc5	77060	11639	6.620844	77060	88699	0.868781
P4	inc1	0	471	0.000000	0	471	0.000000
P4	inc3	0	471	0.000000	0	471	0.000000
P4	inc5	0	471	0.000000	0	471	0.000000
P5	inc1	13	45	0.288889	13	58	0.224138
P5	inc3	13	45	0.288889	13	58	0.224138
P5	inc5	24	34	0.705882	24	58	0.413793
P6	inc1	73	2251	0.032430	73	2324	0.031411
P6	inc3	89	2235	0.039821	89	2324	0.038296
P6	inc5	92	2232	0.041219	92	2324	0.039587
P7	inc1	478	7673	0.062296	478	8151	0.058643
P7	inc3	547	7604	0.071936	547	8151	0.067108
P7	inc5	1234	6917	0.178401	1234	8151	0.151392
P8	inc1	189	28800	0.006562	189	28989	0.006520
P8	inc3	2181	26808	0.081356	2181	28989	0.075235
P8	inc5	5846	23143	0.252603	5846	28989	0.201663
P9	inc1	1294	14097	0.091793	1294	15391	0.084075
P9	inc3	1365	14026	0.097319	1365	15391	0.088688
P9	inc5	2566	12825	0.200078	2566	15391	0.166721
P10	inc1	2502	65843	0.037999	2502	68345	0.036608
P10	inc3	5279	63066	0.083706	5279	68345	0.077240
P10	inc5	7749	60596	0.127880	7749	68345	0.113381
P11	inc1	2375	65031	0.036521	2375	67406	0.035234
P11	inc3	5355	62051	0.086300	5355	67406	0.079444
P11	inc5	7658	59748	0.128172	7658	67406	0.113610
P12	inc1	13700	303197	0.045185	13700	316897	0.043232
P12	inc3	58827	258070	0.227950	58827	316897	0.185634
P12	inc5	93043	223854	0.415641	93043	316897	0.293606
P13	inc1	29521	541406	0.054527	29521	570927	0.051707
P13	inc3	98204	472723	0.207741	98204	570927	0.172008
P13	inc5	165438	405489	0.407996	165438	570927	0.289771
P14	inc1	39208	782523	0.050105	39208	821731	0.047714
P14	inc3	124375	697356	0.178352	124375	821731	0.151357
P14	inc5	223799	597932	0.374288	223799	821731	0.272351
P15	inc1	104730	1719089	0.060922	104730	1823819	0.057423
P15	inc3	313794	1510025	0.207807	313794	1823819	0.172053
P15	inc5	491096	1332723	0.368491	491096	1823819	0.269268
P16	inc1	168201	2661008	0.063210	168201	2829209	0.059452
P16	inc3	473007	2356202	0.200750	473007	2829209	0.167187
P16	inc5	749713	2079496	0.360526	749713	2829209	0.264990
P17	inc1	209053	3623658	0.057691	209053	3832711	0.054544
P17	inc3	612120	3220591	0.190064	612120	3832711	0.159709
P17	inc5	921018	2911693	0.316317	921018	3832711	0.240305
P18	inc1	275194	4560120	0.060348	275194	4835314	0.056913
P18	inc3	794754	4040560	0.196694	794754	4835314	0.164365
P18	inc5	1187731	3647583	0.325621	1187731	4835314	0.245637
P19	inc1	383265	5706504	0.067163	383265	6089769	0.062936
P19	inc3	1044802	5044967	0.207098	1044802	6089769	0.171567
P19	inc5	1642630	4447139	0.369368	1642630	6089769	0.269736
P20	inc1	163339	4345547	0.037588	163339	4508886	0.036226
P20	inc3	503257	4005629	0.125637	503257	4508886	0.111614
P20	inc5	800432	3708454	0.215840	800432	4508886	0.177523
```

#### 2026-01-10 (trimmed ruleset, no equal_assign)
Legacy run: 2026-01-10, trimmed ruleset (no equal_assign), inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_trimmed_eval.
```tsv
Case	Delta	DelEdges	DelTotalEdges	DelEdgeRatio	InsEdges	InsTotalEdges	InsEdgeRatio
P1	inc1	0	88699	0.000000	0	88699	0.000000
P1	inc3	0	88699	0.000000	0	88699	0.000000
P1	inc5	73343	15356	4.776179	73343	88699	0.826875
P3	inc1	60097	28602	2.101147	60097	88699	0.677539
P3	inc3	60426	28273	2.137233	60426	88699	0.681248
P3	inc5	62773	25926	2.421237	62773	88699	0.707708
P4	inc1	5	33	0.151515	5	38	0.131579
P4	inc3	5	33	0.151515	5	38	0.131579
P4	inc5	6	32	0.187500	6	38	0.157895
P5	inc1	6	52	0.115385	6	58	0.103448
P5	inc3	6	52	0.115385	6	58	0.103448
P5	inc5	12	46	0.260870	12	58	0.206897
P6	inc1	0	108	0.000000	0	108	0.000000
P6	inc3	22	86	0.255814	22	108	0.203704
P6	inc5	24	84	0.285714	24	108	0.222222
P7	inc1	25	278	0.089928	25	303	0.082508
P7	inc3	52	251	0.207171	52	303	0.171617
P7	inc5	76	227	0.334802	76	303	0.250825
P8	inc1	32	504	0.063492	32	536	0.059701
P8	inc3	136	400	0.340000	136	536	0.253731
P8	inc5	175	361	0.484765	175	536	0.326493
P9	inc1	20	351	0.056980	20	371	0.053908
P9	inc3	31	340	0.091176	31	371	0.083558
P9	inc5	68	303	0.224422	68	371	0.183288
P10	inc1	1206	2933	0.411183	1206	4139	0.291375
P10	inc3	1955	2184	0.895147	1955	4139	0.472336
P10	inc5	2043	2096	0.974714	2043	4139	0.493597
P11	inc1	1365	2759	0.494744	1365	4124	0.330989
P11	inc3	2027	2097	0.966619	2027	4124	0.491513
P11	inc5	2120	2004	1.057884	2120	4124	0.514064
P12	inc1	405	3800	0.106579	405	4205	0.096314
P12	inc3	889	3316	0.268094	889	4205	0.211415
P12	inc5	1234	2971	0.415348	1234	4205	0.293460
P13	inc1	1033	7202	0.143432	1033	8235	0.125440
P13	inc3	2406	5829	0.412764	2406	8235	0.292168
P13	inc5	3151	5084	0.619788	3151	8235	0.382635
P14	inc1	1934	10331	0.187204	1934	12265	0.157684
P14	inc3	4131	8134	0.507868	4131	12265	0.336812
P14	inc5	5024	7241	0.693827	5024	12265	0.409621
P15	inc1	4799	22061	0.217533	4799	26860	0.178667
P15	inc3	9144	17716	0.516144	9144	26860	0.340432
P15	inc5	11606	15254	0.760850	11606	26860	0.432092
P16	inc1	13796	30725	0.449015	13796	44521	0.309876
P16	inc3	18388	26133	0.703631	18388	44521	0.413019
P16	inc5	20505	24016	0.853806	20505	44521	0.460569
P17	inc1	19763	40886	0.483368	19763	60649	0.325859
P17	inc3	26588	34061	0.780600	26588	60649	0.438391
P17	inc5	28621	32028	0.893624	28621	60649	0.471912
P18	inc1	26192	50577	0.517864	26192	76769	0.341179
P18	inc3	33500	43269	0.774226	33500	76769	0.436374
P18	inc5	36274	40495	0.895765	36274	76769	0.472508
P19	inc1	33439	63496	0.526632	33439	96935	0.344963
P19	inc3	43103	53832	0.800695	43103	96935	0.444659
P19	inc5	46087	50848	0.906368	46087	96935	0.475442
P20	inc1	7959	86265	0.092262	7959	94224	0.084469
P20	inc3	19980	74244	0.269113	19980	94224	0.212048
P20	inc5	27193	67031	0.405678	27193	94224	0.288600
```

### SEMINAIVE stage vs delta counts (ApplyDeltaOps + ΔE/|E|)
DeltaRuleApps uses applyDelta (pre-prune) counts, so del/ins are symmetric in this dataset: deleted tuples do not retain alternative derivations and are reinserted verbatim.
As of 2026-01-11, GraphEdges uses `apply_delta_graph` (post-applyDelta, pre-prune) full-graph totals; earlier tables that used pruned view totals are legacy-only.
#### 2026-01-11 (full ruleset, det-opt, apply_delta_graph)
Run: 2026-01-11, full ruleset, det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	Turn	Full_SEM_s	Inc_SEM_s	Speedup	DeltaRuleApps	GraphEdges	DeltaEdgeRatio
P1	inc1	del	0.005949	0.005426	1.096	0	58	0.000000
P1	inc1	ins	0.005568	0.001506	3.697	0	58	0.000000
P1	inc3	del	0.004767	0.019603	0.243	0	58	0.000000
P1	inc3	ins	0.004770	0.003737	1.276	0	58	0.000000
P1	inc5	del	0.001490	0.018827	0.079	0	58	0.000000
P1	inc5	ins	0.004413	0.013487	0.327	0	58	0.000000
P3	inc1	del	0.002377	0.018805	0.126	0	58	0.000000
P3	inc1	ins	0.004394	0.006261	0.702	0	58	0.000000
P3	inc3	del	0.002250	0.019016	0.118	29	29	1.000000
P3	inc3	ins	0.004686	0.006041	0.776	29	58	0.500000
P3	inc5	del	0.001073	0.014696	0.073	29	29	1.000000
P3	inc5	ins	0.004469	0.010542	0.424	29	58	0.500000
P4	inc1	del	0.000342	0.001842	0.186	0	19	0.000000
P4	inc1	ins	0.000308	0.002716	0.113	0	19	0.000000
P4	inc3	del	0.000335	0.002448	0.137	0	19	0.000000
P4	inc3	ins	0.000406	0.002428	0.167	0	19	0.000000
P4	inc5	del	0.000382	0.002118	0.180	0	19	0.000000
P4	inc5	ins	0.000315	0.001917	0.164	0	19	0.000000
P5	inc1	del	0.000265	0.001689	0.157	13	20	0.650000
P5	inc1	ins	0.000306	0.001391	0.220	13	33	0.393939
P5	inc3	del	0.000281	0.001198	0.235	13	20	0.650000
P5	inc3	ins	0.000388	0.001429	0.272	13	33	0.393939
P5	inc5	del	0.000184	0.000921	0.200	23	10	2.300000
P5	inc5	ins	0.000319	0.001943	0.164	23	33	0.696970
P6	inc1	del	0.000745	0.006285	0.119	1	63	0.015873
P6	inc1	ins	0.000752	0.003410	0.221	1	64	0.015625
P6	inc3	del	0.000887	0.004428	0.200	17	47	0.361702
P6	inc3	ins	0.000731	0.002372	0.308	17	64	0.265625
P6	inc5	del	0.000648	0.003109	0.208	20	44	0.454545
P6	inc5	ins	0.000744	0.003838	0.194	20	64	0.312500
P7	inc1	del	0.001875	0.004001	0.469	16	167	0.095808
P7	inc1	ins	0.001939	0.008006	0.242	16	183	0.087432
P7	inc3	del	0.001825	0.003866	0.472	33	150	0.220000
P7	inc3	ins	0.002089	0.005045	0.414	33	183	0.180328
P7	inc5	del	0.001733	0.004204	0.412	46	137	0.335766
P7	inc5	ins	0.001962	0.003515	0.558	46	183	0.251366
P8	inc1	del	0.005327	0.004240	1.256	45	252	0.178571
P8	inc1	ins	0.004307	0.002971	1.450	45	297	0.151515
P8	inc3	del	0.004764	0.006255	0.762	95	202	0.470297
P8	inc3	ins	0.005635	0.003714	1.517	95	297	0.319865
P8	inc5	del	0.003227	0.007586	0.425	132	165	0.800000
P8	inc5	ins	0.004233	0.006153	0.688	132	297	0.444444
P9	inc1	del	0.002239	0.003866	0.579	54	165	0.327273
P9	inc1	ins	0.002551	0.003623	0.704	54	219	0.246575
P9	inc3	del	0.002331	0.008827	0.264	99	120	0.825000
P9	inc3	ins	0.002559	0.003432	0.746	99	219	0.452055
P9	inc5	del	0.001970	0.006254	0.315	110	109	1.009174
P9	inc5	ins	0.002433	0.003404	0.715	110	219	0.502283
P10	inc1	del	0.022626	0.016401	1.380	1132	1207	0.937862
P10	inc1	ins	0.029375	0.013756	2.135	1132	2339	0.483968
P10	inc3	del	0.016200	0.018088	0.896	1849	490	3.773469
P10	inc3	ins	0.025992	0.015238	1.706	1849	2339	0.790509
P10	inc5	del	0.015023	0.018954	0.793	1999	340	5.879412
P10	inc5	ins	0.027699	0.015600	1.776	1999	2339	0.854639
P11	inc1	del	0.019965	0.012252	1.630	1153	1165	0.989700
P11	inc1	ins	0.026763	0.010427	2.567	1153	2318	0.497412
P11	inc3	del	0.016677	0.015371	1.085	1857	461	4.028200
P11	inc3	ins	0.028387	0.014536	1.953	1857	2318	0.801122
P11	inc5	del	0.016679	0.017463	0.955	1920	398	4.824121
P11	inc5	ins	0.025602	0.013835	1.851	1920	2318	0.828300
P12	inc1	del	0.042451	0.022958	1.849	346	1976	0.175101
P12	inc1	ins	0.045460	0.015938	2.852	346	2322	0.149009
P12	inc3	del	0.035452	0.053465	0.663	843	1479	0.569980
P12	inc3	ins	0.049778	0.031985	1.556	843	2322	0.363049
P12	inc5	del	0.030687	0.079825	0.384	1229	1093	1.124428
P12	inc5	ins	0.043567	0.032396	1.345	1229	2322	0.529285
P13	inc1	del	0.073189	0.048741	1.502	931	3604	0.258324
P13	inc1	ins	0.085161	0.025728	3.310	931	4535	0.205292
P13	inc3	del	0.066443	0.123701	0.537	2098	2437	0.860895
P13	inc3	ins	0.083246	0.057483	1.448	2098	4535	0.462624
P13	inc5	del	0.054103	0.156645	0.345	2754	1781	1.546322
P13	inc5	ins	0.084723	0.060950	1.390	2754	4535	0.607277
P14	inc1	del	0.131847	0.070215	1.878	1826	4924	0.370837
P14	inc1	ins	0.133311	0.041328	3.226	1826	6750	0.270519
P14	inc3	del	0.098265	0.162903	0.603	3331	3419	0.974261
P14	inc3	ins	0.126148	0.066679	1.892	3331	6750	0.493481
P14	inc5	del	0.094283	0.247546	0.381	4231	2519	1.679635
P14	inc5	ins	0.130085	0.089933	1.446	4231	6750	0.626815
P15	inc1	del	0.265276	0.181820	1.459	5512	8591	0.641602
P15	inc1	ins	0.299944	0.102930	2.914	5512	14103	0.390839
P15	inc3	del	0.214627	0.387001	0.555	9762	4341	2.248791
P15	inc3	ins	0.276546	0.182655	1.514	9762	14103	0.692193
P15	inc5	del	0.188237	0.504491	0.373	10826	3277	3.303631
P15	inc5	ins	0.289823	0.240085	1.207	10826	14103	0.767638
P16	inc1	del	0.448604	0.319361	1.405	13088	11390	1.149078
P16	inc1	ins	0.516224	0.214506	2.407	13088	24478	0.534684
P16	inc3	del	0.382780	0.642074	0.596	17874	6604	2.706541
P16	inc3	ins	0.511636	0.290410	1.762	17874	24478	0.730207
P16	inc5	del	0.319658	0.820305	0.390	19522	4956	3.939064
P16	inc5	ins	0.468832	0.406757	1.153	19522	24478	0.797532
P17	inc1	del	0.626876	0.418753	1.497	18393	14949	1.230383
P17	inc1	ins	0.710549	0.280982	2.529	18393	33342	0.551647
P17	inc3	del	0.501872	0.925038	0.543	24780	8562	2.894184
P17	inc3	ins	0.715911	0.443286	1.615	24780	33342	0.743207
P17	inc5	del	0.486997	1.099398	0.443	26832	6510	4.121659
P17	inc5	ins	0.680069	0.499823	1.361	26832	33342	0.804751
P18	inc1	del	0.733412	0.568449	1.290	25016	17185	1.455688
P18	inc1	ins	0.909348	0.379410	2.397	25016	42201	0.592782
P18	inc3	del	0.650327	1.043887	0.623	32088	10113	3.172946
P18	inc3	ins	0.933957	0.509513	1.833	32088	42201	0.760361
P18	inc5	del	0.608425	1.499644	0.406	34561	7640	4.523691
P18	inc5	ins	0.975995	0.655736	1.488	34561	42201	0.818962
P19	inc1	del	1.026818	0.734954	1.397	34153	19132	1.785124
P19	inc1	ins	1.299704	0.520638	2.496	34153	53285	0.640950
P19	inc3	del	0.862460	1.423235	0.606	41922	11363	3.689343
P19	inc3	ins	1.180612	0.732256	1.612	41922	53285	0.786750
P19	inc5	del	0.722430	1.994920	0.362	44641	8644	5.164391
P19	inc5	ins	1.196871	0.891869	1.342	44641	53285	0.837778
P20	inc1	del	0.912271	0.519754	1.755	7900	36518	0.216332
P20	inc1	ins	1.005359	0.322497	3.117	7900	44418	0.177856
P20	inc3	del	0.790884	1.009805	0.783	18409	26009	0.707793
P20	inc3	ins	1.002361	0.480662	2.085	18409	44418	0.414449
P20	inc5	del	0.729993	1.524599	0.479	24406	20012	1.219568
P20	inc5	ins	0.928317	0.636884	1.458	24406	44418	0.549462
```

#### 2026-01-11 (trimmed ruleset, no equal_assign, det-opt, apply_delta_graph)
Run: 2026-01-11, trimmed ruleset (no equal_assign), det-opt, apply_delta_graph, inc1/inc3/inc5, sample=1, run timeout=180s, compile timeout=300s, base-dir experiments/side_channel_inc_trimmed_eval.
```tsv
Case	Delta	Turn	Full_SEM_s	Inc_SEM_s	Speedup	DeltaRuleApps	GraphEdges	DeltaEdgeRatio
P1	inc1	del	0.004743	0.002514	1.887	0	58	0.000000
P1	inc1	ins	0.004392	0.000987	4.448	0	58	0.000000
P1	inc3	del	0.004344	0.000980	4.431	0	58	0.000000
P1	inc3	ins	0.004562	0.001215	3.755	0	58	0.000000
P1	inc5	del	0.001337	0.019502	0.069	0	58	0.000000
P1	inc5	ins	0.004464	0.008072	0.553	0	58	0.000000
P3	inc1	del	0.002458	0.019236	0.128	0	58	0.000000
P3	inc1	ins	0.004907	0.006778	0.724	0	58	0.000000
P3	inc3	del	0.002005	0.018872	0.106	0	58	0.000000
P3	inc3	ins	0.004519	0.006775	0.667	0	58	0.000000
P3	inc5	del	0.001783	0.019515	0.091	0	58	0.000000
P3	inc5	ins	0.006603	0.007456	0.886	0	58	0.000000
P4	inc1	del	0.000242	0.002511	0.096	4	15	0.266667
P4	inc1	ins	0.000261	0.001367	0.191	4	19	0.210526
P4	inc3	del	0.000228	0.001048	0.218	4	15	0.266667
P4	inc3	ins	0.000344	0.000871	0.395	4	19	0.210526
P4	inc5	del	0.000234	0.000539	0.434	5	14	0.357143
P4	inc5	ins	0.000234	0.002628	0.089	5	19	0.263158
P5	inc1	del	0.000298	0.001770	0.169	5	28	0.178571
P5	inc1	ins	0.000299	0.001114	0.268	5	33	0.151515
P5	inc3	del	0.000295	0.001214	0.243	5	28	0.178571
P5	inc3	ins	0.000395	0.002563	0.154	5	33	0.151515
P5	inc5	del	0.000311	0.002252	0.138	10	23	0.434783
P5	inc5	ins	0.000355	0.001292	0.275	10	33	0.303030
P6	inc1	del	0.000494	0.001690	0.292	0	61	0.000000
P6	inc1	ins	0.000417	0.001266	0.329	0	61	0.000000
P6	inc3	del	0.000387	0.001962	0.197	21	40	0.525000
P6	inc3	ins	0.000516	0.001318	0.392	21	61	0.344262
P6	inc5	del	0.000388	0.001505	0.258	22	39	0.564103
P6	inc5	ins	0.000491	0.001223	0.402	22	61	0.360656
P7	inc1	del	0.000918	0.001241	0.740	23	148	0.155405
P7	inc1	ins	0.000986	0.001470	0.671	23	171	0.134503
P7	inc3	del	0.001008	0.001814	0.556	48	123	0.390244
P7	inc3	ins	0.001194	0.001994	0.599	48	171	0.280702
P7	inc5	del	0.000777	0.004235	0.184	66	105	0.628571
P7	inc5	ins	0.001042	0.002830	0.368	66	171	0.385965
P8	inc1	del	0.001619	0.001447	1.119	31	256	0.121094
P8	inc1	ins	0.002586	0.001378	1.877	31	287	0.108014
P8	inc3	del	0.001140	0.001548	0.736	131	156	0.839744
P8	inc3	ins	0.001791	0.001501	1.193	131	287	0.456446
P8	inc5	del	0.001054	0.002834	0.372	162	125	1.296000
P8	inc5	ins	0.001975	0.002585	0.764	162	287	0.564460
P9	inc1	del	0.001463	0.001394	1.050	18	191	0.094241
P9	inc1	ins	0.001239	0.001204	1.029	18	209	0.086124
P9	inc3	del	0.001117	0.002986	0.374	27	182	0.148352
P9	inc3	ins	0.001157	0.001173	0.986	27	209	0.129187
P9	inc5	del	0.000989	0.001262	0.784	60	149	0.402685
P9	inc5	ins	0.001137	0.001295	0.878	60	209	0.287081
P10	inc1	del	0.010677	0.006068	1.760	1185	1146	1.034031
P10	inc1	ins	0.017543	0.005938	2.955	1185	2331	0.508366
P10	inc3	del	0.007039	0.007867	0.895	1901	430	4.420930
P10	inc3	ins	0.016966	0.010369	1.636	1901	2331	0.815530
P10	inc5	del	0.006235	0.008821	0.707	1963	368	5.334239
P10	inc5	ins	0.014899	0.009525	1.564	1963	2331	0.842128
P11	inc1	del	0.011549	0.007544	1.531	1344	974	1.379877
P11	inc1	ins	0.021025	0.006399	3.286	1344	2318	0.579810
P11	inc3	del	0.006676	0.007554	0.884	1978	340	5.817647
P11	inc3	ins	0.019375	0.007536	2.571	1978	2318	0.853322
P11	inc5	del	0.006796	0.008546	0.795	2033	285	7.133333
P11	inc5	ins	0.019100	0.008368	2.282	2033	2318	0.877049
P12	inc1	del	0.013257	0.004136	3.205	392	1923	0.203848
P12	inc1	ins	0.012471	0.003676	3.393	392	2315	0.169330
P12	inc3	del	0.011190	0.005048	2.217	845	1470	0.574830
P12	inc3	ins	0.018049	0.004250	4.247	845	2315	0.365011
P12	inc5	del	0.009671	0.006657	1.453	1146	1169	0.980325
P12	inc5	ins	0.017568	0.005519	3.183	1146	2315	0.495032
P13	inc1	del	0.033115	0.007896	4.194	1003	3527	0.284378
P13	inc1	ins	0.039622	0.006199	6.391	1003	4530	0.221413
P13	inc3	del	0.020484	0.010539	1.944	2290	2240	1.022321
P13	inc3	ins	0.033269	0.009800	3.395	2290	4530	0.505519
P13	inc5	del	0.012205	0.012813	0.953	2957	1573	1.879847
P13	inc5	ins	0.035694	0.010787	3.309	2957	4530	0.652759
P14	inc1	del	0.038203	0.013184	2.898	1872	4873	0.384158
P14	inc1	ins	0.062351	0.011808	5.280	1872	6745	0.277539
P14	inc3	del	0.019084	0.023567	0.810	3973	2772	1.433261
P14	inc3	ins	0.039272	0.019502	2.014	3973	6745	0.589029
P14	inc5	del	0.017785	0.017414	1.021	4749	1996	2.379259
P14	inc5	ins	0.048149	0.016584	2.903	4749	6745	0.704077
P15	inc1	del	0.083113	0.035372	2.350	4668	9430	0.495016
P15	inc1	ins	0.119745	0.043150	2.775	4668	14098	0.331111
P15	inc3	del	0.064004	0.043043	1.487	8778	5320	1.650000
P15	inc3	ins	0.095203	0.046392	2.052	8778	14098	0.622642
P15	inc5	del	0.045485	0.045516	0.999	10984	3114	3.527296
P15	inc5	ins	0.113216	0.051747	2.188	10984	14098	0.779118
P16	inc1	del	0.147153	0.072585	2.027	13587	10886	1.248117
P16	inc1	ins	0.259013	0.076654	3.379	13587	24473	0.555183
P16	inc3	del	0.090289	0.075636	1.194	17770	6703	2.651052
P16	inc3	ins	0.217355	0.097046	2.240	17770	24473	0.726106
P16	inc5	del	0.068150	0.083557	0.816	19468	5005	3.889710
P16	inc5	ins	0.216881	0.101574	2.135	19468	24473	0.795489
P17	inc1	del	0.175295	0.099465	1.762	19519	13818	1.412578
P17	inc1	ins	0.350136	0.123004	2.847	19519	33337	0.585506
P17	inc3	del	0.134602	0.123260	1.092	25794	7543	3.419594
P17	inc3	ins	0.303583	0.136419	2.225	25794	33337	0.773735
P17	inc5	del	0.119744	0.133294	0.898	27251	6086	4.477654
P17	inc5	ins	0.310351	0.137578	2.256	27251	33337	0.817440
P18	inc1	del	0.271961	0.165667	1.642	25836	16361	1.579121
P18	inc1	ins	0.389432	0.162323	2.399	25836	42197	0.612271
P18	inc3	del	0.151291	0.171126	0.884	32445	9752	3.327010
P18	inc3	ins	0.462724	0.167023	2.770	32445	42197	0.768894
P18	inc5	del	0.138465	0.163466	0.847	34514	7683	4.492256
P18	inc5	ins	0.420255	0.183545	2.290	34514	42197	0.817925
P19	inc1	del	0.355481	0.229725	1.547	33022	20258	1.630072
P19	inc1	ins	0.548455	0.270420	2.028	33022	53280	0.619782
P19	inc3	del	0.198553	0.228191	0.870	41813	11467	3.646377
P19	inc3	ins	0.502165	0.266115	1.887	41813	53280	0.784779
P19	inc5	del	0.212476	0.193918	1.096	43944	9336	4.706941
P19	inc5	ins	0.488407	0.294687	1.657	43944	53280	0.824775
P20	inc1	del	0.408499	0.127433	3.206	7435	36895	0.201518
P20	inc1	ins	0.446217	0.122291	3.649	7435	44330	0.167719
P20	inc3	del	0.598049	0.145318	4.115	18466	25864	0.713965
P20	inc3	ins	0.610870	0.129668	4.711	18466	44330	0.416558
P20	inc5	del	0.303287	0.198381	1.529	24674	19656	1.255291
P20	inc5	ins	0.566586	0.191767	2.955	24674	44330	0.556598
```

Legacy SEM delta tables below are pre apply_delta_graph (do not compare ΔE/|E|).

#### 2026-01-11 (full ruleset, det-opt)
Legacy run: 2026-01-11, full ruleset, det-opt, pre apply_delta_graph; ΔE/|E| uses pruned view edges (do not compare).
```tsv
Case	Delta	Turn	Full_SEM_s	Inc_SEM_s	Speedup	DeltaRuleApps	GraphEdges	DeltaEdgeRatio
P1	inc1	del	0.004251	0.001509	2.818	0	40	0.000000
P1	inc1	ins	0.004478	0.005950	0.753	0	40	0.000000
P1	inc3	del	0.003155	0.007915	0.399	0	40	0.000000
P1	inc3	ins	0.004452	0.001304	3.413	0	40	0.000000
P1	inc5	del	0.001473	0.010097	0.146	0	40	0.000000
P1	inc5	ins	0.004766	0.002672	1.784	0	40	0.000000
P3	inc1	del	0.002297	0.015203	0.151	0	40	0.000000
P3	inc1	ins	0.004700	0.003420	1.374	0	40	0.000000
P3	inc3	del	0.002324	0.011148	0.208	29	20	1.450000
P3	inc3	ins	0.004608	0.001191	3.870	29	40	0.725000
P3	inc5	del	0.001052	0.009461	0.111	29	20	1.450000
P3	inc5	ins	0.004785	0.002811	1.702	29	40	0.725000
P4	inc1	del	0.000302	0.004944	0.061	0	1	0.000000
P4	inc1	ins	0.000442	0.001038	0.426	0	1	0.000000
P4	inc3	del	0.000312	0.002161	0.145	0	1	0.000000
P4	inc3	ins	0.000325	0.002006	0.162	0	1	0.000000
P4	inc5	del	0.000392	0.001742	0.225	0	1	0.000000
P4	inc5	ins	0.000307	0.002253	0.136	0	1	0.000000
P5	inc1	del	0.000242	0.006048	0.040	13	15	0.866667
P5	inc1	ins	0.000302	0.003256	0.093	13	15	0.866667
P5	inc3	del	0.000261	0.001235	0.211	13	15	0.866667
P5	inc3	ins	0.000304	0.002696	0.113	13	15	0.866667
P5	inc5	del	0.000204	0.001952	0.105	23	1	23.000000
P5	inc5	ins	0.000320	0.003192	0.100	23	15	1.533333
P6	inc1	del	0.000831	0.013218	0.063	1	61	0.016393
P6	inc1	ins	0.000879	0.004374	0.201	1	62	0.016129
P6	inc3	del	0.000724	0.012503	0.058	17	40	0.425000
P6	inc3	ins	0.000723	0.008265	0.088	17	62	0.274194
P6	inc5	del	0.000676	0.004099	0.165	20	33	0.606061
P6	inc5	ins	0.000741	0.005760	0.129	20	62	0.322581
P7	inc1	del	0.002124	0.014671	0.145	28	81	0.345679
P7	inc1	ins	0.002021	0.004589	0.440	16	82	0.195122
P7	inc3	del	0.002022	0.004234	0.477	45	62	0.725806
P7	inc3	ins	0.002694	0.007001	0.385	33	82	0.402439
P7	inc5	del	0.001698	0.002656	0.639	58	36	1.611111
P7	inc5	ins	0.002232	0.001980	1.127	46	82	0.560976
P8	inc1	del	0.003797	0.022280	0.170	45	135	0.333333
P8	inc1	ins	0.004016	0.007588	0.529	45	144	0.312500
P8	inc3	del	0.003454	0.007939	0.435	95	82	1.158537
P8	inc3	ins	0.003848	0.006126	0.628	85	134	0.634328
P8	inc5	del	0.004138	0.006612	0.626	132	52	2.538462
P8	inc5	ins	0.004168	0.002592	1.608	122	134	0.910448
P9	inc1	del	0.002218	0.018669	0.119	54	108	0.500000
P9	inc1	ins	0.002604	0.007336	0.355	54	198	0.272727
P9	inc3	del	0.002041	0.010515	0.194	99	76	1.302632
P9	inc3	ins	0.002537	0.005795	0.438	99	198	0.500000
P9	inc5	del	0.002015	0.006511	0.309	118	63	1.873016
P9	inc5	ins	0.002585	0.003643	0.710	108	188	0.574468
P10	inc1	del	0.020981	0.020477	1.025	1132	36	31.444444
P10	inc1	ins	0.026745	0.013681	1.955	1132	37	30.594595
P10	inc3	del	0.016264	0.013988	1.163	1849	36	51.361111
P10	inc3	ins	0.026539	0.017276	1.536	1849	37	49.972973
P10	inc5	del	0.016651	0.017872	0.932	1999	34	58.794118
P10	inc5	ins	0.027808	0.014015	1.984	1999	37	54.027027
P11	inc1	del	0.020579	0.018087	1.138	1153	16	72.062500
P11	inc1	ins	0.031059	0.011481	2.705	1153	16	72.062500
P11	inc3	del	0.018832	0.016069	1.172	1857	15	123.800000
P11	inc3	ins	0.028418	0.014512	1.958	1857	16	116.062500
P11	inc5	del	0.015537	0.016590	0.937	1920	14	137.142857
P11	inc5	ins	0.026677	0.013775	1.937	1920	16	120.000000
P12	inc1	del	0.049613	0.024790	2.001	347	1054	0.329222
P12	inc1	ins	0.045153	0.011840	3.814	346	1318	0.262519
P12	inc3	del	0.037833	0.029860	1.267	844	598	1.411371
P12	inc3	ins	0.047211	0.013767	3.429	843	1318	0.639605
P12	inc5	del	0.032127	0.041353	0.777	1231	272	4.525735
P12	inc5	ins	0.050788	0.010736	4.731	1228	1316	0.933131
P13	inc1	del	0.079615	0.039754	2.003	931	2117	0.439773
P13	inc1	ins	0.089094	0.019020	4.684	931	3038	0.306452
P13	inc3	del	0.068048	0.056309	1.208	2099	1056	1.987689
P13	inc3	ins	0.093261	0.019942	4.677	2097	3036	0.690711
P13	inc5	del	0.056393	0.091148	0.619	2756	609	4.525452
P13	inc5	ins	0.082277	0.022039	3.733	2753	3035	0.907084
P14	inc1	del	0.108704	0.057824	1.880	1826	2668	0.684408
P14	inc1	ins	0.133728	0.028363	4.715	1826	4475	0.408045
P14	inc3	del	0.097584	0.094255	1.035	3331	1324	2.515861
P14	inc3	ins	0.131120	0.031611	4.148	3331	4475	0.744358
P14	inc5	del	0.084118	0.120522	0.698	4231	554	7.637184
P14	inc5	ins	0.126602	0.034240	3.698	4231	4475	0.945475
P15	inc1	del	0.273208	0.174149	1.569	5512	3868	1.425026
P15	inc1	ins	0.307830	0.112487	2.737	5512	9045	0.609397
P15	inc3	del	0.229333	0.222286	1.032	9762	397	24.589421
P15	inc3	ins	0.300941	0.083295	3.613	9762	9045	1.079270
P15	inc5	del	0.216101	0.294825	0.733	10826	217	49.889401
P15	inc5	ins	0.372062	0.095902	3.880	10826	9045	1.196904
P16	inc1	del	0.425021	0.242774	1.751	13089	3948	3.315350
P16	inc1	ins	0.551677	0.134090	4.114	13088	15964	0.819845
P16	inc3	del	0.421373	0.323300	1.303	17875	1042	17.154511
P16	inc3	ins	0.462971	0.152172	3.042	17874	15964	1.119644
P16	inc5	del	0.349955	0.446609	0.784	19524	355	54.997183
P16	inc5	ins	0.508594	0.174257	2.919	19521	15962	1.222967
P17	inc1	del	0.563367	0.320419	1.758	18393	5071	3.627095
P17	inc1	ins	0.706945	0.194515	3.634	18393	21719	0.846862
P17	inc3	del	0.655752	0.483841	1.355	24780	1247	19.871692
P17	inc3	ins	0.748730	0.228624	3.275	24780	21719	1.140937
P17	inc5	del	0.486171	0.624983	0.778	26833	588	45.634354
P17	inc5	ins	0.721795	0.246373	2.930	26832	21718	1.235473
P18	inc1	del	0.754565	0.423318	1.783	25016	5748	4.352122
P18	inc1	ins	0.967544	0.322139	3.003	25016	27477	0.910434
P18	inc3	del	0.606934	0.598993	1.013	32088	1444	22.221607
P18	inc3	ins	0.960113	0.318369	3.016	32088	27477	1.167813
P18	inc5	del	0.623686	0.789984	0.789	34562	506	68.304348
P18	inc5	ins	0.945563	0.317627	2.977	34561	27476	1.257861
P19	inc1	del	1.004522	0.573492	1.752	34154	4998	6.833533
P19	inc1	ins	1.185190	0.392632	3.019	34153	34669	0.985116
P19	inc3	del	0.864874	0.824897	1.048	41924	972	43.131687
P19	inc3	ins	1.192149	0.389017	3.065	41922	34668	1.209242
P19	inc5	del	0.687928	1.018640	0.675	44643	320	139.509375
P19	inc5	ins	1.196731	0.476608	2.511	44641	34668	1.287672
P20	inc1	del	0.896109	0.432936	2.070	7946	25557	0.310913
P20	inc1	ins	1.013685	0.288688	3.511	7900	36252	0.217919
P20	inc3	del	0.808855	0.636448	1.271	18425	13543	1.360481
P20	inc3	ins	0.966461	0.311388	3.104	18361	36234	0.506734
P20	inc5	del	0.806323	0.761576	1.059	24422	7949	3.072336
P20	inc5	ins	1.050306	0.354827	2.960	24358	36234	0.672242
```

### SEMINAIVE stage vs delta counts (DeltaNodes/DeltaEdges)
Delta* columns come from `[inc-naive] delta counts` in FORWARD_COMPILATION logs.
#### 2026-01-10 (full ruleset, equal_assign)
Legacy run: 2026-01-10, full ruleset (equal_assign), inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_eval.
```tsv
Case	Delta	Turn	Full_SEM_s	Inc_SEM_s	Speedup	DeltaInsNodes	DeltaInsEdges	DeltaDelNodes	DeltaDelEdges
P1	inc1	del	0.308619	0.003080	100.193	0	0	0	0
P1	inc1	ins	0.374344	0.001686	221.972	0	0	0	0
P1	inc3	del	0.163037	0.152906	1.066	0	0	0	0
P1	inc3	ins	0.271973	0.048598	5.596	0	0	0	0
P1	inc5	del	0.053398	0.143623	0.372	0	0	0	0
P1	inc5	ins	0.281798	0.120375	2.341	0	0	0	0
P3	inc1	del	0.104790	0.133366	0.786	0	0	0	0
P3	inc1	ins	0.278659	0.104623	2.663	0	0	0	0
P3	inc3	del	0.105291	0.125322	0.840	0	0	17	75
P3	inc3	ins	0.289040	0.075171	3.845	40	126	0	0
P3	inc5	del	0.030692	0.109813	0.279	0	0	17	75
P3	inc5	ins	0.275340	0.121540	2.265	40	126	0	0
P4	inc1	del	0.001471	0.001209	1.216	0	0	0	0
P4	inc1	ins	0.001402	0.000723	1.938	0	0	0	0
P4	inc3	del	0.001379	0.000575	2.400	0	0	0	0
P4	inc3	ins	0.001398	0.000647	2.159	0	0	0	0
P4	inc5	del	0.001357	0.000545	2.489	0	0	0	0
P4	inc5	ins	0.001390	0.000679	2.047	0	0	0	0
P5	inc1	del	0.000403	0.001691	0.238	0	0	0	0
P5	inc1	ins	0.000450	0.001123	0.401	0	0	0	0
P5	inc3	del	0.000307	0.000851	0.361	0	0	0	0
P5	inc3	ins	0.000341	0.000675	0.505	0	0	0	0
P5	inc5	del	0.000230	0.000833	0.276	0	0	12	11
P5	inc5	ins	0.000339	0.000823	0.412	49	24	0	0
P6	inc1	del	0.006450	0.002067	3.121	0	0	3	2
P6	inc1	ins	0.005883	0.001593	3.694	3	2	0	0
P6	inc3	del	0.006460	0.002108	3.064	0	0	20	18
P6	inc3	ins	0.007524	0.001430	5.261	76	37	0	0
P6	inc5	del	0.006187	0.001707	3.625	0	0	25	21
P6	inc5	ins	0.006507	0.001423	4.572	97	50	0	0
P7	inc1	del	0.024047	0.006067	3.963	0	0	26	355
P7	inc1	ins	0.022083	0.002191	10.077	26	355	0	0
P7	inc3	del	0.022926	0.005189	4.418	0	0	44	370
P7	inc3	ins	0.021937	0.002552	8.594	86	389	0	0
P7	inc5	del	0.032670	0.006266	5.214	0	0	63	386
P7	inc5	ins	0.024209	0.004370	5.540	166	435	0	0
P8	inc1	del	0.086423	0.005588	15.465	0	0	8	6
P8	inc1	ins	0.092403	0.002195	42.089	28	17	0	0
P8	inc3	del	0.079150	0.022903	3.456	0	0	80	994
P8	inc3	ins	0.084835	0.005852	14.497	799	10650	0	0
P8	inc5	del	0.067322	0.024602	2.736	0	0	209	3286
P8	inc5	ins	0.083994	0.011488	7.311	893	10706	0	0
P9	inc1	del	0.041433	0.006424	6.449	0	0	60	56
P9	inc1	ins	0.044510	0.003908	11.391	292	161	0	0
P9	inc3	del	0.040597	0.004028	10.078	0	0	103	93
P9	inc3	ins	0.045158	0.003945	11.448	392	215	0	0
P9	inc5	del	0.035834	0.017907	2.001	0	0	142	342
P9	inc5	ins	0.046493	0.006311	7.367	426	459	0	0
P10	inc1	del	0.226347	0.042013	5.388	0	0	6	12
P10	inc1	ins	0.259165	0.012684	20.432	20	29	0	0
P10	inc3	del	0.227994	0.036504	6.246	0	0	6	12
P10	inc3	ins	0.229410	0.020437	11.225	20	29	0	0
P10	inc5	del	0.234801	0.041479	5.661	0	0	9	14
P10	inc5	ins	0.267687	0.028447	9.410	71	201	0	0
P11	inc1	del	0.259653	0.044417	5.846	0	0	0	0
P11	inc1	ins	0.277955	0.012636	21.997	0	0	0	0
P11	inc3	del	0.227570	0.040740	5.586	0	0	3	2
P11	inc3	ins	0.260369	0.021466	12.129	3	2	0	0
P11	inc5	del	0.228618	0.044790	5.104	0	0	6	4
P11	inc5	ins	0.272081	0.027646	9.842	6	4	0	0
P12	inc1	del	1.586246	0.060281	26.314	0	0	399	4670
P12	inc1	ins	2.419714	0.036487	66.317	1128	6396	0	0
P12	inc3	del	0.951421	0.235974	4.032	0	0	811	5039
P12	inc3	ins	1.351382	0.152247	8.876	2562	7235	0	0
P12	inc5	del	0.863031	0.347674	2.482	0	0	1181	6910
P12	inc5	ins	1.192526	0.285558	4.176	4044	16714	0	0
P13	inc1	del	2.245982	0.212703	10.559	0	0	783	735
P13	inc1	ins	3.584528	0.079152	45.287	2941	1666	0	0
P13	inc3	del	1.896529	0.547251	3.466	0	0	1833	3862
P13	inc3	ins	3.199152	0.297707	10.746	6823	13116	0	0
P13	inc5	del	1.621187	0.939059	1.726	0	0	2788	12156
P13	inc5	ins	2.341568	0.702775	3.332	8629	21105	0	0
P14	inc1	del	3.481304	0.393562	8.846	0	0	1474	1411
P14	inc1	ins	4.301843	0.130845	32.877	6551	17951	0	0
P14	inc3	del	2.940271	1.053372	2.791	0	0	2788	3023
P14	inc3	ins	3.676304	0.528349	6.958	11006	22319	0	0
P14	inc5	del	2.453899	1.572643	1.560	0	0	3635	3744
P14	inc5	ins	3.550889	0.933557	3.804	13812	29944	0	0
P15	inc1	del	13.138471	1.559447	8.425	0	0	4629	4496
P15	inc1	ins	9.890460	0.409398	24.159	16840	15634	0	0
P15	inc3	del	7.293411	2.931849	2.488	0	0	8060	7695
P15	inc3	ins	9.217068	1.363925	6.758	28432	30108	0	0
P15	inc5	del	6.093589	3.564849	1.709	0	0	9017	8406
P15	inc5	ins	9.350145	2.188749	4.272	29009	30435	0	0
P16	inc1	del	13.325187	4.104828	3.246	0	0	11029	11726
P16	inc1	ins	14.419324	0.867676	16.618	38385	24787	0	0
P16	inc3	del	11.907698	5.847362	2.036	0	0	15100	16493
P16	inc3	ins	14.285393	2.244387	6.365	48032	38569	0	0
P16	inc5	del	10.726381	6.828901	1.571	0	0	16811	19921
P16	inc5	ins	15.019261	3.635544	4.131	50268	41631	0	0
P17	inc1	del	19.783993	7.508286	2.635	0	0	15500	15220
P17	inc1	ins	21.167731	1.049936	20.161	52864	30374	0	0
P17	inc3	del	17.842647	9.361490	1.906	0	0	20842	20012
P17	inc3	ins	20.845412	3.045086	6.846	65372	43571	0	0
P17	inc5	del	15.255336	10.990110	1.388	0	0	22656	21631
P17	inc5	ins	21.058234	4.829687	4.360	67843	51407	0	0
P18	inc1	del	24.164998	12.008071	2.012	0	0	20691	20311
P18	inc1	ins	27.236640	1.371948	19.853	69011	39636	0	0
P18	inc3	del	21.498865	13.843759	1.553	0	0	26881	25733
P18	inc3	ins	26.043408	4.583560	5.682	82647	47518	0	0
P18	inc5	del	19.051526	15.362085	1.240	0	0	29190	27734
P18	inc5	ins	25.993145	7.121243	3.650	85645	49647	0	0
P19	inc1	del	31.182333	18.830237	1.656	0	0	28548	32700
P19	inc1	ins	40.772985	2.230873	18.277	94477	59726	0	0
P19	inc3	del	27.125491	20.662074	1.313	0	0	35301	42689
P19	inc3	ins	37.194128	7.197824	5.167	108015	80130	0	0
P19	inc5	del	24.128818	21.502240	1.122	0	0	38002	46200
P19	inc5	ins	36.095263	10.776478	3.349	110081	81325	0	0
P20	inc1	del	23.230283	7.224893	3.215	0	0	8010	11251
P20	inc1	ins	23.458028	0.921280	25.462	34918	23188	0	0
P20	inc3	del	20.872690	15.019120	1.390	0	0	19386	47099
P20	inc3	ins	24.262939	2.927604	8.288	75610	93645	0	0
P20	inc5	del	18.968011	18.150697	1.045	0	0	26430	64477
P20	inc5	ins	23.712349	4.724020	5.020	94035	105345	0	0
```

#### 2026-01-10 (trimmed ruleset, no equal_assign)
Legacy run: 2026-01-10, trimmed ruleset (no equal_assign), inc1/inc3/inc5, sample=1, base-dir experiments/side_channel_inc_trimmed_eval.
```tsv
Case	Delta	Turn	Full_SEM_s	Inc_SEM_s	Speedup	DeltaInsNodes	DeltaInsEdges	DeltaDelNodes	DeltaDelEdges
P1	inc1	del	0.036916	0.107275	0.344	0	0	0	0
P1	inc1	ins	0.146112	0.087999	1.660	0	0	0	0
P1	inc3	del	0.027843	0.140441	0.198	0	0	0	0
P1	inc3	ins	0.166298	0.146420	1.136	0	0	0	0
P1	inc5	del	0.028716	0.117594	0.244	0	0	5	20
P1	inc5	ins	0.156379	0.114796	1.362	41	126	0	0
P3	inc1	del	0.048905	0.097985	0.499	0	0	0	0
P3	inc1	ins	0.141773	0.092253	1.537	0	0	0	0
P3	inc3	del	0.048009	0.088201	0.544	0	0	0	0
P3	inc3	ins	0.150190	0.072500	2.072	0	0	0	0
P3	inc5	del	0.025540	0.098974	0.258	0	0	0	0
P3	inc5	ins	0.159499	0.117883	1.353	0	0	0	0
P4	inc1	del	0.000921	0.000961	0.958	0	0	0	0
P4	inc1	ins	0.000884	0.000866	1.021	0	0	0	0
P4	inc3	del	0.000903	0.000894	1.010	0	0	0	0
P4	inc3	ins	0.001640	0.000787	2.085	0	0	0	0
P4	inc5	del	0.000886	0.001143	0.775	0	0	0	0
P4	inc5	ins	0.000891	0.001432	0.622	0	0	0	0
P5	inc1	del	0.000258	0.001385	0.186	0	0	7	6
P5	inc1	ins	0.000278	0.002362	0.118	49	24	0	0
P5	inc3	del	0.000261	0.001015	0.257	0	0	7	6
P5	inc3	ins	0.000275	0.001243	0.221	49	24	0	0
P5	inc5	del	0.000259	0.001801	0.144	0	0	9	7
P5	inc5	ins	0.000280	0.001702	0.164	49	24	0	0
P6	inc1	del	0.003643	0.003390	1.075	0	0	8	7
P6	inc1	ins	0.004171	0.003336	1.250	73	35	0	0
P6	inc3	del	0.002740	0.005383	0.509	0	0	58	561
P6	inc3	ins	0.003907	0.003790	1.031	143	609	0	0
P6	inc5	del	0.002669	0.004438	0.601	0	0	60	562
P6	inc5	ins	0.004169	0.002740	1.522	145	610	0	0
P7	inc1	del	0.013023	0.004702	2.770	0	0	1	1
P7	inc1	ins	0.013293	0.002075	6.405	1	1	0	0
P7	inc3	del	0.011901	0.004278	2.782	0	0	10	8
P7	inc3	ins	0.011909	0.002934	4.059	20	12	0	0
P7	inc5	del	0.011736	0.004673	2.512	0	0	16	13
P7	inc5	ins	0.011901	0.003143	3.787	35	21	0	0
P8	inc1	del	0.048436	0.002659	18.213	0	0	22	20
P8	inc1	ins	0.049501	0.002519	19.653	78	47	0	0
P8	inc3	del	0.045830	0.004410	10.392	0	0	48	42
P8	inc3	ins	0.048340	0.005250	9.208	207	130	0	0
P8	inc5	del	0.043373	0.012522	3.464	0	0	159	2039
P8	inc5	ins	0.048339	0.007430	6.505	312	2127	0	0
P9	inc1	del	0.023308	0.009787	2.382	0	0	40	38
P9	inc1	ins	0.025373	0.003356	7.560	169	94	0	0
P9	inc3	del	0.020228	0.010211	1.981	0	0	61	54
P9	inc3	ins	0.025115	0.006206	4.047	283	155	0	0
P9	inc5	del	0.020036	0.011923	1.680	0	0	121	107
P9	inc5	ins	0.025344	0.006571	3.857	380	207	0	0
P10	inc1	del	0.132803	0.048911	2.715	0	0	6	12
P10	inc1	ins	0.138227	0.011944	11.573	20	29	0	0
P10	inc3	del	0.125004	0.033687	3.711	0	0	18	36
P10	inc3	ins	0.159665	0.019541	8.171	60	87	0	0
P10	inc5	del	0.121538	0.042503	2.860	0	0	26	48
P10	inc5	ins	0.138532	0.026602	5.208	69	100	0	0
P11	inc1	del	0.152564	0.037789	4.037	0	0	0	0
P11	inc1	ins	0.145554	0.011635	12.510	0	0	0	0
P11	inc3	del	0.125391	0.039399	3.183	0	0	0	0
P11	inc3	ins	0.133104	0.018303	7.272	0	0	0	0
P11	inc5	del	0.118603	0.040901	2.900	0	0	0	0
P11	inc5	ins	0.136019	0.023475	5.794	0	0	0	0
P12	inc1	del	0.706162	0.069146	10.213	0	0	261	974
P12	inc1	ins	0.706354	0.039483	17.890	847	1240	0	0
P12	inc3	del	0.580622	0.199154	2.915	0	0	932	6519
P12	inc3	ins	0.755593	0.128586	5.876	2883	9272	0	0
P12	inc5	del	0.507442	0.343902	1.476	0	0	1133	6690
P12	inc5	ins	0.818368	0.263307	3.108	3429	9586	0	0
P13	inc1	del	1.376175	0.177471	7.754	0	0	825	784
P13	inc1	ins	1.640598	0.080342	20.420	3341	1903	0	0
P13	inc3	del	1.355083	0.412714	3.283	0	0	1524	2731
P13	inc3	ins	1.595062	0.193344	8.250	5704	4583	0	0
P13	inc5	del	1.078506	0.647298	1.666	0	0	2263	3392
P13	inc5	ins	1.573994	0.343302	4.585	8322	6105	0	0
P14	inc1	del	2.392938	0.301984	7.924	0	0	1913	11418
P14	inc1	ins	2.715976	0.117581	23.099	6366	13718	0	0
P14	inc3	del	1.971623	0.883591	2.231	0	0	3361	15286
P14	inc3	ins	2.842919	0.342532	8.300	11081	19094	0	0
P14	inc5	del	1.680476	1.129641	1.488	0	0	4398	20790
P14	inc5	ins	2.713648	0.704194	3.854	13478	25687	0	0
P15	inc1	del	6.273233	1.520492	4.126	0	0	4641	9740
P15	inc1	ins	6.872711	0.425052	16.169	16239	14719	0	0
P15	inc3	del	5.400065	3.166347	1.705	0	0	7907	13675
P15	inc3	ins	6.997433	1.436146	4.872	25469	20970	0	0
P15	inc5	del	4.450649	3.969988	1.121	0	0	9591	15183
P15	inc5	ins	6.703326	2.242605	2.989	28603	22756	0	0
P16	inc1	del	9.711456	4.291374	2.263	0	0	10667	11358
P16	inc1	ins	10.817472	0.645041	16.770	37555	30695	0	0
P16	inc3	del	8.573984	5.470151	1.567	0	0	15306	20180
P16	inc3	ins	10.703239	1.904506	5.620	48378	42378	0	0
P16	inc5	del	7.482854	6.587379	1.136	0	0	16867	21450
P16	inc5	ins	10.516854	3.441247	3.056	50417	49738	0	0
P17	inc1	del	14.759640	7.556252	1.953	0	0	15085	14826
P17	inc1	ins	15.657365	0.957138	16.359	51854	41787	0	0
P17	inc3	del	12.956262	9.191922	1.410	0	0	21043	24797
P17	inc3	ins	15.656860	3.018116	5.188	65907	55114	0	0
P17	inc5	del	11.574487	10.242238	1.130	0	0	23414	31172
P17	inc5	ins	16.134601	4.845710	3.330	69310	57072	0	0
P18	inc1	del	18.454210	11.865074	1.555	0	0	22006	27139
P18	inc1	ins	20.306671	1.276674	15.906	72790	49800	0	0
P18	inc3	del	16.183393	13.652050	1.185	0	0	27665	32015
P18	inc3	ins	19.669804	4.135282	4.757	84577	56586	0	0
P18	inc5	del	14.690028	15.338593	0.958	0	0	29782	38152
P18	inc5	ins	19.821241	6.282177	3.155	86647	63407	0	0
P19	inc1	del	23.004712	19.334280	1.190	0	0	28104	31623
P19	inc1	ins	27.851344	2.054866	13.554	92331	57249	0	0
P19	inc3	del	20.971962	20.105154	1.043	0	0	35264	48592
P19	inc3	ins	27.365268	5.281558	5.181	107060	78040	0	0
P19	inc5	del	18.709208	21.445900	0.872	0	0	37900	50323
P19	inc5	ins	27.096717	8.038793	3.371	110344	89927	0	0
P20	inc1	del	17.438649	6.625350	2.632	0	0	8029	7494
P20	inc1	ins	18.201902	1.006348	18.087	34920	19243	0	0
P20	inc3	del	16.014717	15.075597	1.062	0	0	19218	20656
P20	inc3	ins	18.287194	2.734843	6.687	77874	47293	0	0
P20	inc5	del	15.035446	18.383152	0.818	0	0	26642	39472
P20	inc5	ins	18.059595	4.419234	4.087	97639	73529	0	0
```

## Notes
- Do not commit anything under `experiments/`; keep it local.
- `side_channel_inc.py` invokes `souffle` from PATH; always point PATH at
  `build/src` (repo build) so compile/run uses the latest binary, and verify
  `souffle --version` matches `build/src/souffle --version` before collecting
  results.
- If you edit delta files manually (`delta/inc1*.txt`), rerun compile+run so
  probabilities match.
- If you edit legacy delta files (`delta/inc10_*.txt`), rerun compile+run so
  probabilities match.
- `reordering_runtime` is a CUDD delta timer with millisecond resolution
  (computed from `Cudd_ReadReorderingTime()`).
- If `time` reports `user > real`, some work is running in parallel threads
  (OpenMP or other). To force single-thread runs: `OMP_NUM_THREADS=1
  OMP_THREAD_LIMIT=1` or pass `-j 1`.
- CLI runs invoke `tryGarbageCollection()` at the end of each turn to encourage
  BDD cleanup after deletions.
- CLI accepts non-interactive stdin (e.g., `< delta/inc1.txt`) and will process
  `insert/delete/commit/q` lines.
- `results-souffle-inc.tsv` includes all `delta-*.json` present under each
  `output/` directory; remove stale delta outputs (e.g., old `inc10`) before
  collecting if you want a clean inc1/inc3/inc5-only TSV.

## How I run the inc experiments (exact commands)
Note: for future inc evaluations, only run P12–P20; smaller cases are too
short/noisy for meaningful comparisons. Legacy runs above include P1/P3/P4-P20.
From repo root, with build Souffle on PATH:
```bash
export PATH="/home/hugh/research/datalog/souffle/build/src:$PATH"

# Generate cases (P12-P20)
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval generate --cases 12-20 --cleanup

# Generate deltas (default change spec inc1/inc3/inc5)
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval delta --cases 12-20 --cleanup

# Compile compute for each case using online CLI support
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval compile --cases 12-20 --timeout 300

# Run baseline (full+inc) and one sample of inc1/inc3/inc5 per case
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval run   --cases 12-20 --delta-labels inc1,inc3,inc5 --delta-samples 1 --timeout 180 --run-arg=--det-opt

# Collect TSV
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval collect --cases 12-20
```

### Trimmed ruleset run (no equal_assign)
```bash
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval \
  generate --cases 12-20 --cleanup --rule-set trimmed

python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval \
  delta --cases 12-20 --cleanup

JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval \
  compile --cases 12-20 --timeout 300 --jobs ${JOBS}

python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval \
  run --cases 12-20 --delta-labels inc1,inc3,inc5 --delta-samples 1 --timeout 180 --run-arg=--det-opt

python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval \
  collect --cases 12-20
```
### Trimmed ruleset run (0.1/0.3/0.5% deltas)
```bash
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval_small \
  generate --cases 12-20 --cleanup --rule-set trimmed

python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval_small \
  delta --cases 12-20 --cleanup --change-spec "inc0p1=0.001,inc0p3=0.003,inc0p5=0.005"

JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval_small \
  compile --cases 12-20 --timeout 300 --jobs ${JOBS}

python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval_small \
  run --cases 12-20 --delta-labels inc0p1,inc0p3,inc0p5 --delta-samples 1 --timeout 180 --run-arg=--det-opt

python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval_small \
  collect --cases 12-20
```
### Legacy inc10 run (2025-12-21)
```bash
export PATH="/home/hugh/research/datalog/souffle/cmake-build-release/src:$PATH"

# Generate cases P4-P13 (skip P1/P3 due to current inc loop issue)
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval generate --cases 4-13 --cleanup

# Generate deltas (overwrite delta/)
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval delta --cases 4-13 --cleanup

# Compile compute for each case using online CLI support
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval compile --cases 4-13 --timeout 300

# Run baseline (full+inc) and one sample of inc10 per case, timeout 30s
python /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval run \
  --cases 4-13 --delta-labels inc10 --delta-samples 1 --timeout 30
```
Artifacts per case:
- Stage logs: `output/log_P*_<label>_1_{inc,full}_*.json`.
- Consistency + paths: `output/delta-<label>-1.json` (base/iter comparisons,
  `max|d|`, etc).
- Prob outputs: `output/facts.{inc,full}.prob`,
  `output/fact-iterN-{inc-naive,inc-regional,full}.prob`, plus runner-prefixed
  `output/delta-<label>-1-{inc-naive,inc-regional,full}-fact-iterN-<mode>.prob`.

## Related commits
- `739ee83cb` — refactor(inc-region): align regional insert with naive propagation
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
- `b22a891b0` — fix(inc): sync det-opt deltas and artifact docs
