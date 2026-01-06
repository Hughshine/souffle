# Profiling Notes (Online Incremental)

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

## Outputs and What They Mean

### 1) `run_*.stdout` (console log)
Key lines to watch:
- `real/user/sys` from `/usr/bin/time -p`: wall/user/sys time.
- `Current live nodes` and `Memory usage`: DD backend state after each phase.
- `[inc-naive] delta counts: insNodes=... insEdges=... delNodes=... delEdges=...`
- `[inc-regional] delta counts: ...` plus:
  - `[inc-regional rebuild] timing(ms): ...`
  - `[inc-regional] timing(ms): analyze=... rebuild=... total=...`
- `[buildFormulasCyclewise] timings(ms): ...` (full pipeline)
- `[pipeline] debugger log: <file>.json` indicates where the JSON log landed.

### 2) Output directory (`-D`)
Generated per run:
- `facts.prob`: baseline output.
- `fact-iterN-{inc-naive,inc-regional,full}.prob`: per-turn probabilities.
- `initial-input-relations-iter*.txt`: snapshot of input relations per turn.
- `log_*.json`: debugger JSON (see below).

### 3) Debugger JSON (`log_*.json`)
Structure:
- `turns[]`: each turn corresponds to baseline or a commit (mode labels are `FULL-HARD`, `FULL-SOFT`, or `INC`).
- `turns[i].stages[]`: stage name and `time_seconds`.
- `turns[i].time_seconds`: total time for the turn.
- `FORWARD_COMPILATION_*` stage contains `info`:
  - `live_nodes`
  - `memory_usage_mb`
  - `reordering_runtime` (DD backend)
  - `changed_node_count` (incremental only)
  - cache stats (`cache_hits`, `cache_lookups`, `cache_hit_rate`)
- `FORWARD_COMPILATION_INC.logs.INFO` includes the delta counts line.

## Derived Metrics (Suggested)
- Effective delta ratio (nodes):
  - `del_ratio = delNodes / live_nodes`
  - `ins_ratio = insNodes / live_nodes`
- Edge ratios are not directly available (total edge count is not logged). If needed, add instrumentation or enable dumps and compute counts offline.

## Practical Notes
- `inc1` deltas produce: turn2 = delete phase, turn3 = insert phase.
- `inc-regional` logs include extra `analyze`/`rebuild` breakdowns in stdout.
- For fair timing comparisons, keep dump flags off and use the same delta file and output directory layout.
