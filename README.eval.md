# Evaluation Plan for SISO Rewriter (current commands)

This describes how to exercise the SISO-based rewriting on `side_channel_full` benchmarks (e.g., P4–P12).

## Scope
- Full-mode rewrite only; incremental modes do not run rewrite.
- Online compilation is default; commands keep `--online` for clarity, but evaluation here is full-mode runs.

## Prerequisites
- Build `souffle` (release): `cmake --build cmake-build-release --target souffle -j4`.
- Each benchmark dir must be writable (logs/DOT go in-place).

## Per-case workflow
For each `P?/compute.souffle.dl`:

1) **Build generated program**
   ```bash
   souffle_bin=/home/hugh/research/datalog/souffle/cmake-build-release/src/souffle
   "$souffle_bin" --online -F ./input -D ./output compute.souffle.dl -o compute_new > rebuild.log 2>&1
   ```
2) **Run (no rewrite)**
   ```bash
   ./compute_new -F ./input -D ./output_no_rewrite > run_no_rewrite.log 2>&1
   ```
3) **Run (with rewrite)**
   ```bash
   ./compute_new -r -F ./input -D ./output_rewrite > run_rewrite.log 2>&1
   ```
4) **Compare outputs**
   ```bash
   diff output_no_rewrite/facts.prob output_rewrite/facts.prob
   ```
5) **Artifacts to check**
   - `run_*.log`: `[pipeline]` timings (graph, pruning, SISO detection/rewrite, BDD init/build, WMC).
   - `siso_regions*.dot`: graph with SISO regions highlighted.
   - Optional: `rewrite*.dot` if rewrite is enabled.

## Batch template
```bash
souffle_bin=/home/hugh/research/datalog/souffle/cmake-build-release/src/souffle
for d in P4 P5 P6 P7 P8 P9 P10 P11 P12; do
  [ -f "$d/compute.souffle.dl" ] || continue
  (cd "$d" && "$souffle_bin" --online -F ./input -D ./output compute.souffle.dl -o compute_new > rebuild.log 2>&1)
  (cd "$d" && ./compute_new -F ./input -D ./output_no_rewrite > run_no_rewrite.log 2>&1)
  (cd "$d" && ./compute_new -r -F ./input -D ./output_rewrite > run_rewrite.log 2>&1)
done
```

## What to compare
- SISO detection/rewrite timing vs BDD phases.
- Node/edge and region counts in DOTs; confirm SI/SO highlighting.
- Output equality (`facts.prob`).
- Debugger JSON stage timings (`log.txt_*.json`) should be non-negative; WMC timing is now properly closed.
