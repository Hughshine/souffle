# Evaluation Plan for SISO Rewriter (P4–P9)

This describes how to exercise the SISO-based rewriting and pipeline on the benchmark instances under `problog-benchmark/side_channel_full/P4…P9`.

## Prerequisites
- Build the updated `souffle` binary (release build assumed): `cmake --build cmake-build-release --target souffle -j4`.
- Ensure the benchmark directory is writable or run commands with sufficient permissions; logs and DOT files are written in-place.

## Per-case workflow (P4–P9)
For each `P?/compute.souffle.dl`:
1) **Build generated program**
   ```bash
   souffle_bin=/home/hugh/research/datalog/souffle/cmake-build-release/src/souffle
   "$souffle_bin" --online -F ./input -D ./output compute.souffle.dl -o compute_new > rebuild.log 2>&1
   ```
2) **Run with SISO detection + rewriting enabled**
   ```bash
   timeout 150s ./compute_new --merge-bi-imp -F ./input -D ./output > run.log 2>&1
   ```
   (Adjust flags if the rewriter is gated by a CLI option once implemented.)
3) **Artifacts to check**
   - `run.log`: contains `[pipeline] SISO detection took X ms` and rewriter stats if hooked; WMC timing lines.
   - `siso_debug.log`: per-edge/candidate diagnostics.
   - `siso_regions.dot`: full graph with SISO regions colored; SI/SO marked via peripheries.
   - `siso_info.csv`: entries for each detected region.
   - (If rewriter implemented) `rewrite.log`, `rewrite_final.dot`, `macros.dot` for collapsed graph.

## Batch script (P4–P9)
```bash
souffle_bin=/home/hugh/research/datalog/souffle/cmake-build-release/src/souffle
set -e
for d in P4/ P5/ P6/ P7/ P8/ P9/; do
  [ -f "$d/compute.souffle.dl" ] || continue
  echo "===== [build] $d ====="
  (cd "$d" && "$souffle_bin" --online -F ./input -D ./output compute.souffle.dl -o compute_new > rebuild.log 2>&1)
  echo "===== [run]   $d (timeout 150s) ====="
  (cd "$d" && timeout 150s ./compute_new --merge-bi-imp -F ./input -D ./output > run.log 2>&1)
done
```

## What to compare
- **SISO counts and timings**: use `rg "SISO detection" P*/run.log`.
- **Region overlays**: open `P*/siso_regions.dot` to confirm coverage and SI/SO markings.
- **Rewrite impact (when available)**: compare node/edge counts before/after (`rewrite.log`, `rewrite_final.dot`), and WMC timing changes.

## Notes
- If permission issues arise in `problog-benchmark/side_channel_full`, run with elevated permissions or copy cases into a writable workspace.
- The current pipeline prints SISO detection time even if no rewriter is invoked; hook rewriter stats into `run.log` once implemented.
