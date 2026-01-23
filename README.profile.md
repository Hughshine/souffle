# Profiling Notes (Online Incremental)

## Source references
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)
- [src/include/souffle/problog/debug/Debugger.h](src/include/souffle/problog/debug/Debugger.h)
- [src/problog/debug/Debugger.cpp](src/problog/debug/Debugger.cpp)
- [src/include/souffle/problog/formula/CuddManager.h](src/include/souffle/problog/formula/CuddManager.h)
- [src/include/souffle/problog/formula/SddManager.h](src/include/souffle/problog/formula/SddManager.h)
- [src/Derivation.cpp](src/Derivation.cpp)


This note explains where timing/memory/graph statistics are reported, how to collect them, and how to interpret the key fields. It is written for the online incremental path (default).

## Status
- Active profiling guide for online incremental pipelines.

## Scope
- Focus: P12-style runs with `inc-naive`, `inc-regional`, and `full` pipelines (`full` maps to `full-hard`; `full-soft` is optional).
- No dump options by default (`--dumpjson/--dumpdot/--dumpstat` add overhead).

## How To Run (Minimal)
From `experiments/side_channel_inc_eval/P12`:
```
./compute -F input -D output_run_inc1_1_inc_naive --setmode inc \
  --logfile log_P12_inc1_1_inc_naive < delta/inc1_1.txt > run_P12_inc1_1_inc_naive.stdout 2>&1

./compute -F input -D output_run_inc1_1_inc_regional --setmode inc-regional \
  --logfile log_P12_inc1_1_inc_regional < delta/inc1_1.txt > run_P12_inc1_1_inc_regional.stdout 2>&1

./compute -F input -D output_run_inc1_1_full_hard --setmode full \
  --logfile log_P12_inc1_1_full_hard < delta/inc1_1.txt > run_P12_inc1_1_full_hard.stdout 2>&1
```

For paper-style timing, wrap each command with:
```
/usr/bin/time -p <command>
```
The debugger log filename is timestamped; use the `[pipeline] debugger log: <file>.json`
line in stdout to locate it. `--logfile` sets the base name (default `log.txt`).

## Outputs and What They Mean

### 1) `run_*.stdout` (console log)
Key lines to watch:
- `real/user/sys` from `/usr/bin/time -p`: wall/user/sys time.
- `[inc-iter N] mode=FULL-HARD|FULL-SOFT|INC`: CLI turn label.
- `[inc-naive] delta counts: insNodes=... insEdges=... delNodes=... delEdges=...`
- `[inc-regional] delta counts: ...` plus:
  - `[inc-regional rebuild] timing(ms): ...`
  - `[inc-regional] timing(ms): analyze=... rebuild=... total=...`
- `[buildFormulasCyclewise] timings(ms): ...` (full pipeline)
- `[pipeline] debugger log: <file>.json` indicates where the JSON log landed.
- SDD backend prints `[SDD][Chk] ...` stats; CUDD `dumpProfilingStatistics()` is
  currently silent (stats still appear in the JSON `info` map).

### 2) Output directory (`-D`)
Generated per run:
- `facts.prob`: initial full pipeline output (before the CLI loop).
- `fact-iterN-inc-naive.prob` / `fact-iterN-inc-regional.prob`: per-turn outputs
  from the incremental CLI.
- `fact-iterN-full.prob`: per-turn outputs when the CLI runs in full mode.
- `initial-input-relations-iter0.txt` (pipeline) and `initial-input-relations-iterN.txt`
  (CLI) snapshots of input relations.
- Debugger JSON: `<logfile>_YYYYMMDD_HHMMSS.json` (timestamped, in the output dir).
- If `--dumpdot`/`--dumpjson` is enabled, derivation dumps such as
  `derivation-inc-before-prune<N>.dot` and `derivation-inc-after-prune<N>*.json`.

### 3) Debugger JSON (`log_*.json`)
Structure:
- `turns[]`: each turn corresponds to baseline or a commit.
  - `mode` is `FULL-HARD`/`FULL-SOFT` for full turns; incremental turns are `INC`
    (inc-naive vs inc-regional is not encoded in the mode label).
- `turns[i].stages[]`: `name`, `time_seconds`, `peak_mem_kb`, `info`, `logs`.
- `turns[i].time_seconds`: total time for the turn.
- `FORWARD_COMPILATION_*` stage `info` includes formula-manager stats:
  - CUDD: `live_nodes`, `dead_nodes`, `total_nodes`, `memory_usage_mb`,
    `cache_hits`, `cache_lookups`, `cache_hit_rate`, `reordering_runtime`.
  - SDD: `live_nodes`, `dead_nodes`, `total_nodes`.
  - Incremental adds `changed_node_count` and extra per-phase counters when enabled.
- `FORWARD_COMPILATION_INC.logs.INFO` includes the delta counts line.

## Derived Metrics (Suggested)
- Effective delta ratio (nodes):
  - `del_ratio = delNodes / live_nodes`
  - `ins_ratio = insNodes / live_nodes`
- Edge ratios are not directly available (total edge count is not logged). If needed, add instrumentation or enable dumps and compute counts offline.

## DRed Sub-Phase Profiling
- Compile with `--profile --dred-profile` to include DRed sub-phase timers.
- Run with `--dred-profile -p profile.json --dumpstat` to emit per-SCC workload counters and JSON timers.
- See `README.dred.md` for DRed-specific interpretation and examples.

## Practical Notes
- `inc1` deltas produce: turn2 = delete phase, turn3 = insert phase.
- `inc-regional` logs include extra `analyze`/`rebuild` breakdowns in stdout.
- For fair timing comparisons, keep dump flags off and use the same delta file and output directory layout.

## Related commits
- `7a3109820` — perf(dred): add profiling and rederive join ordering
- `812ea4081` — docs(repo): refine README narratives
- `0f233581a` — Make online-only pipeline default and disable ctest
