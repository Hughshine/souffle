# Incremental Optimization Checklist

## Scope
Use this note for optimization-focused incremental runs (especially inc-regional).
Detailed historical timing dumps were moved to `docs/historical/OPT.md`.

## Recommended Run Pattern
```bash
export PATH="$(pwd)/build/src:$PATH"

python3 problog-benchmark/side_channel_inc.py \
  --base-dir problog-benchmark/side_channel_inc_opt \
  compile --cases 1-20 --timeout 600 --jobs 4

python3 problog-benchmark/side_channel_inc.py \
  --base-dir problog-benchmark/side_channel_inc_opt \
  run --cases 1-20 --timeout 900 --compare-all

python3 problog-benchmark/side_channel_inc.py \
  --base-dir problog-benchmark/side_channel_inc_opt \
  collect --cases 1-20
```

Note: det-opt is enabled by default. Use `--run-arg=--no-det-opt` only for
ablation runs.

## What To Extract
From `delta-*.json` and `log_*.json`:
- End-to-end time: `inc-naive` vs `inc-regional` vs `full`.
- Stage timings per turn: `SEM`, `PRN`, `FC`, `WMC`.
- Regional ratio stats:
  - `inc_regional.stdout_stats.inc_regional_final[].ratio`
- Overlap stats:
  - `inc_regional.stdout_stats.inc_regional_overlap[].multi_nodes_total`

For archived aggregate TSV outputs, use `archive/2026-02-11/tsv/`.

## Source references
- [problog-benchmark/side_channel_inc.py](problog-benchmark/side_channel_inc.py)
- [src/include/souffle/problog/RegionalIncremental.h](src/include/souffle/problog/RegionalIncremental.h)
- [archive/2026-02-11/tsv/inc_regional_vs_naive_fc_wmc_per_case_delta.tsv](archive/2026-02-11/tsv/inc_regional_vs_naive_fc_wmc_per_case_delta.tsv)

## Related commits
- `UNCOMMITTED` — docs(evaluation): condense OPT note and archive historical details
