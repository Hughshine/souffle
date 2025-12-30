# Side Channel Incremental Mini Case (P1)

Minimal slice of the side-channel incremental benchmark copied locally for quick debugging of the incremental pipeline.

## Scope
- Online incremental path only (default; `--online` optional); legacy `--inc` backend is removed.

Contents:
- `P1/compute.dl`, `compute.problog.dl`, `compute.smt2`: generated Soufflé and source SMT for the P1 case.
- `P1/input/`: facts and probability files (140 KB).
- `P1/delta/inc1.txt`, `inc3.txt`: two delta workloads (small deletes+reinserts) that exercise incremental CLI commands.

## How to run
From `experiments/side_channel_inc_mini/P1`:

```bash
# 1) Build the incremental Soufflé binary (online default; `--online` optional)
souffle -o compute compute.dl

# 2) Baseline (inc + full-hard) to establish reference outputs
./compute -F input -D output --setmode inc  --logfile log_baseline_inc <<<"q"
mv output/facts.prob output/facts.inc.prob
./compute -F input -D output --setmode full --logfile log_baseline_full <<<"q"
mv output/facts.prob output/facts.full.prob

# 3) Run a delta (example: inc1)
./compute -F input -D output --setmode inc  --logfile log_inc1_inc  < delta/inc1.txt
./compute -F input -D output --setmode full --logfile log_inc1_full < delta/inc1.txt

# 4) Inspect outputs
ls output
# facts.inc/full.prob (baselines), fact-iter*-{inc-naive,inc-regional,full}.prob from delta runs, plus dot/json/logs per run
```

Notes:
- Delta files issue deletes, `commit`, then reinsert and commit again, ending with `q`.
- You can swap `inc1.txt` for `inc3.txt` to test a larger mutation.
- Logs (`--logfile`) capture stage timings (including forward compilation stats) for comparing incremental vs full.
- `--setmode full` maps to `full-hard`; use `--setmode full-soft` to reuse the DD manager state.
