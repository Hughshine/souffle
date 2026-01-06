# ProbLog Baseline Runs

This document records ProbLog full-inference timing runs for the
side-channel benchmark programs. It captures stdout stage timings
whenever ProbLog prints them and notes timeouts explicitly.

## Scope
- Rule sets: full and trimmed_plus.
- Cases: P1..P20 (P2 missing in the dataset).
- Mode: full inference only (no ground-only in this pass).
- Timeout: 60s per case.

## Environment
- ProbLog CLI: `problog` (2.2.9) from `/usr/local/bin/problog`.
- Runs use absolute paths to `compute.problog.dl` to avoid cwd ambiguity.
- Run order: full ruleset first, then trimmed_plus.

## How runs were executed
- Binary: `problog` from PATH.
- Command template (per case):
  `problog /abs/path/to/compute.problog.dl`
- Output per case:
  - `output_problog_full/stdout.txt` (stage timings when printed)
  - `output_problog_full/stderr.txt`
  - `output_problog_full/meta.json` (status, elapsed, exit code)
  - `output_problog_full/stages.json` (parsed stage timings)

## Data provenance
- full: `experiments/side_channel_full_eval/P*/compute.problog.dl`
- trimmed_plus: `experiments/side_channel_full_eval_trimmed_plus_norewrite/P*/compute.problog.dl`
- Stage parsing: lines matching `Stage: <sec>s, <MB> MB` in stdout.

## Notes on timeouts and partial stages
- Stage timings are only recorded when ProbLog prints its per-stage line.
- If a case times out mid-stage, later stages will be missing.
- Partial data can appear (e.g., P10 full records only Propagating evidence).

## Summary (timeout=60s)
- full: ok=3, timeout=16, error=0
- trimmed_plus: ok=6, timeout=13, error=0

## ProbLog full run (full, timeout=60s)
| Case | Status | Elapsed_s | Propagating evidence_s | Grounding_s | Removing derivations of facts_s | Cycle breaking_s | Compiling SDD_s |
|--- | --- | --- | --- | --- | --- | --- | ---|
| P1 | timeout | 60.000000 |  |  |  |  |  |
| P3 | timeout | 60.000000 |  |  |  |  |  |
| P4 | ok | 1.316627 | 0.000100 | 1.167100 | 0.001300 | 0.000100 | 0.000200 |
| P5 | ok | 0.464797 | 0.000400 | 0.289400 | 0.000600 | 0.000600 | 0.001000 |
| P6 | ok | 10.286137 | 0.000100 | 9.567400 | 0.001700 | 0.001700 | 0.002300 |
| P7 | timeout | 60.000000 |  |  |  |  |  |
| P8 | timeout | 60.000000 |  |  |  |  |  |
| P9 | timeout | 60.000000 |  |  |  |  |  |
| P10 | timeout | 60.000000 | 0.000100 |  |  |  |  |
| P11 | timeout | 60.000000 |  |  |  |  |  |
| P12 | timeout | 60.000000 |  |  |  |  |  |
| P13 | timeout | 60.000000 |  |  |  |  |  |
| P14 | timeout | 60.000000 |  |  |  |  |  |
| P15 | timeout | 60.000000 |  |  |  |  |  |
| P16 | timeout | 60.000000 |  |  |  |  |  |
| P17 | timeout | 60.000000 |  |  |  |  |  |
| P18 | timeout | 60.000000 |  |  |  |  |  |
| P19 | timeout | 60.000000 |  |  |  |  |  |
| P20 | timeout | 60.000000 |  |  |  |  |  |

## ProbLog full run (trimmed_plus, timeout=60s)
| Case | Status | Elapsed_s | Propagating evidence_s | Grounding_s | Removing derivations of facts_s | Cycle breaking_s | Compiling SDD_s |
|--- | --- | --- | --- | --- | --- | --- | ---|
| P1 | timeout | 60.000000 |  |  |  |  |  |
| P3 | timeout | 60.000000 |  |  |  |  |  |
| P4 | ok | 0.314310 | 0.000000 | 0.158600 | 0.000800 | 0.000100 | 0.000300 |
| P5 | ok | 0.464746 | 0.000100 | 0.288600 | 0.001200 | 0.001000 | 0.001800 |
| P6 | ok | 0.815073 | 0.000100 | 0.659700 | 0.002000 | 0.003000 | 0.004100 |
| P7 | ok | 4.573737 | 0.000100 | 4.344500 | 0.005300 | 0.004100 | 0.005800 |
| P8 | ok | 7.481078 | 0.000100 | 7.229700 | 0.007900 | 0.006300 | 0.006800 |
| P9 | ok | 5.576376 | 0.000100 | 5.342100 | 0.007000 | 0.009800 | 0.011500 |
| P10 | timeout | 60.000000 |  |  |  |  |  |
| P11 | timeout | 60.000000 |  |  |  |  |  |
| P12 | timeout | 60.000000 |  |  |  |  |  |
| P13 | timeout | 60.000000 |  |  |  |  |  |
| P14 | timeout | 60.000000 |  |  |  |  |  |
| P15 | timeout | 60.000000 |  |  |  |  |  |
| P16 | timeout | 60.000000 |  |  |  |  |  |
| P17 | timeout | 60.000000 |  |  |  |  |  |
| P18 | timeout | 60.000000 |  |  |  |  |  |
| P19 | timeout | 60.000000 |  |  |  |  |  |
| P20 | timeout | 60.000000 |  |  |  |  |  |
