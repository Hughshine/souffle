# Side-Channel Incremental Batch (2026-02-11)

## Scope
- Batch root: `archive/2026-02-11/`
- Side-channel runs: `archive/2026-02-11/side_channel/`
- Case focus: P12-P20 (mainly P13-P20 for mix-ratio comparisons)
- Result summaries: `archive/2026-02-11/tsv/`
- Plot artifacts: `archive/2026-02-11/plots/`

## Data Layout
- `tsv/`: consolidated summary tables exported from run outputs.
- `plots/`: generated summary figures for this batch.
- `side_channel/`: per-case run directories and operation logs.

## Included Run Sets

### 1) `side_channel_inc_p12_20`
- Cases: P12-P20
- Delta mode: standard (`inc1`, `inc3`, `inc5`)
- Change spec: `inc1=0.001, inc3=0.003, inc5=0.005`
- Change cap: `inc1=100, inc3=200, inc5=300`
- Cluster mode: `assign`, `cap_tol=0.1`
- Source: `archive/2026-02-11/side_channel/side_channel_inc_p12_20/operation_inc.log`

### 2) `side_channel_inc_p12_20_mix_seed42`
- Cases: P12-P20
- Delta strategy: `mix`
- Mix ratio/cap: `0.01`, `cap=300`
- Mix pool: `pool_ratio=0.02`, `pool_cap_mult=2.0`
- Mix distributions: `(0,100), (25,75), (50,50), (75,25), (100,0)`
- Notable run mode: repeated `compare-all` runs (`full`, `inc-naive`, `inc-regional`)
- Sources:
  - `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/operation_inc.log`
  - `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/reordering_inc_runs.tsv`
  - `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/reordering_inc_runs_latest.tsv`
  - `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/inc_reorder_summary.tsv`

### 3) `side_channel_inc_p13_20_mix0p005_seed42`
- Cases in run stage: P13-P20
- Delta strategy: `mix`
- Mix ratio/cap: `0.005`, `cap=150`
- Mix pool: `pool_ratio=0.02`, `pool_cap_mult=2.0`
- Mix distributions: `(0,100), (25,75), (50,50), (75,25), (100,0)`
- Source: `archive/2026-02-11/side_channel/side_channel_inc_p13_20_mix0p005_seed42/operation_inc.log`

### 4) `side_channel_inc_p13_20_mix0p015_seed42`
- Cases: P13-P20
- Delta strategy: `mix`
- Mix ratio/cap: `0.015`, `cap=450`
- Mix pool: `pool_ratio=0.02`, `pool_cap_mult=2.0`
- Mix distributions: `(0,100), (25,75), (50,50), (75,25), (100,0)`
- Source: `archive/2026-02-11/side_channel/side_channel_inc_p13_20_mix0p015_seed42/operation_inc.log`

## Key Results (from `tsv/`)

### End-to-end combo speedup (P13-P20, relative to combo1)
Source: `archive/2026-02-11/tsv/end2end_combo_speedup_overall.tsv`
- combo2 avg: `9.229x` (median `9.395x`)
- combo3 avg: `1.071x` (median `1.074x`)
- combo4 avg: `16.135x` (median `16.039x`)

### inc-regional vs inc-naive (FC+WMC combined)
Source: `archive/2026-02-11/tsv/inc_regional_vs_naive_fc_wmc_per_case_delta.tsv`
- Overall mean speedup (naive/regional): `2.031x` across 40 case-delta rows (P13-P20)
- By mix ratio (mean over P13-P20):
  - `mix-d0-i100`: `3.836x`
  - `mix-d25-i75`: `2.419x`
  - `mix-d50-i50`: `1.809x`
  - `mix-d75-i25`: `1.071x`
  - `mix-d100-i0`: `1.023x`

Interpretation:
- `inc-regional` benefit is strongest for insert-heavy deltas (`mix-d0-i100`).
- Benefit narrows near delete-heavy or pure-delete workloads.

### P15 FC-stage snapshot (from historical note)
Related note: `docs/historical/FINAL.md`
- Successful deltas: 19/25
- FC mean speedup vs full:
  - inc-naive: `17.26x`
  - inc-regional: `24.45x`
- Largest relative gain appears on insert-heavy deltas.

## Open Issues / Follow-up
Related note: `docs/historical/TODO.md`
- Timeout case to investigate:
  - `archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/P12/delta/mix-d25-i75_4.txt`
- Reordering policy is still open:
  - need clearer policy on dynamic DD reordering for incremental turns.

## Archive Hygiene Applied
- Kept: runtime data, logs, TSV summaries, plot outputs.
- Removed from `side_channel/*`: generated `compute.cpp`, compiled `compute`, and `compute_no_reord` binaries.
- Verified state: no `compute`, `compute_no_reord`, or `*.cpp` generated artifacts remain under
  `archive/2026-02-11/side_channel/`.

## Related Notes
- `docs/historical/PAPER.md`
- `docs/historical/FINAL.md`
- `docs/historical/TODO.md`

## Source references
- [archive/2026-02-11/tsv/end2end_combo_speedup_overall.tsv](archive/2026-02-11/tsv/end2end_combo_speedup_overall.tsv)
- [archive/2026-02-11/tsv/inc_regional_vs_naive_fc_wmc_per_case_delta.tsv](archive/2026-02-11/tsv/inc_regional_vs_naive_fc_wmc_per_case_delta.tsv)
- [archive/2026-02-11/side_channel/side_channel_inc_p12_20/operation_inc.log](archive/2026-02-11/side_channel/side_channel_inc_p12_20/operation_inc.log)
- [archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/operation_inc.log](archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42/operation_inc.log)
- [archive/2026-02-11/side_channel/side_channel_inc_p13_20_mix0p005_seed42/operation_inc.log](archive/2026-02-11/side_channel/side_channel_inc_p13_20_mix0p005_seed42/operation_inc.log)
- [archive/2026-02-11/side_channel/side_channel_inc_p13_20_mix0p015_seed42/operation_inc.log](archive/2026-02-11/side_channel/side_channel_inc_p13_20_mix0p015_seed42/operation_inc.log)
- [archive/2026-02-11/side_channel/side_channel_inc/operation_inc.log](archive/2026-02-11/side_channel/side_channel_inc/operation_inc.log)
- [docs/historical/PAPER.md](docs/historical/PAPER.md)
- [docs/historical/FINAL.md](docs/historical/FINAL.md)
- [docs/historical/TODO.md](docs/historical/TODO.md)

## Related commits
- `UNCOMMITTED` — chore(archive): move side-channel artifacts to dated archive and keep docs-only tracking
- `UNCOMMITTED` — docs(archive): summarize 2026-02-11 side-channel batch parameters and results
- `UNCOMMITTED` — chore(archive): remove generated C++/binaries and keep runtime data only
