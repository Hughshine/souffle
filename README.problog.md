# ProbLog Baseline Runs

## Source references
- [problog-benchmark/side_channel_full.py](problog-benchmark/side_channel_full.py)


This document records ProbLog full-inference timing runs for the
side-channel benchmark programs. It captures stdout stage timings
whenever ProbLog prints them and notes timeouts explicitly.

## Status
- Historical baseline record; re-run when datasets or timeouts change.

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
  - Latest terminal reruns only guarantee `stdout.txt`/`stderr.txt`; older `meta.json` may be stale if outputs overlap.

## Data provenance
- full: `experiments/side_channel_full_eval/P*/compute.problog.dl`
- trimmed_plus: `experiments/side_channel_full_eval_trimmed_plus_norewrite/P*/compute.problog.dl`
- Stage parsing: lines matching `Stage: <sec>s, <MB> MB` in stdout.

## Notes on timeouts and partial stages
- Stage timings are only recorded when ProbLog prints its per-stage line.
- If a case times out mid-stage, later stages will be missing.
- Partial data can appear (e.g., P10 full records only Propagating evidence).
- Codex sandbox performance can be unstable; re-run evaluation in a terminal for definitive timings (relative ratios usually stay similar).

## Summary (timeout=60s)
- full: ok=5, timeout=14, error=0
- trimmed_plus: ok=6, timeout=13, error=0

## ProbLog full run (full, timeout=60s, stdout-only)
| Case | Status | Propagating evidence_s | Grounding_s | Removing derivations of facts_s | Cycle breaking_s | Compiling SDD_s |
|--- | --- | --- | --- | --- | --- | ---|
| P1 | ok | 0.0001 | 44.9994 | 0.0002 | 0.0156 | 0.0015 |
| P3 | ok | 0.0000 | 46.0602 | 0.0002 | 0.0160 | 0.0002 |
| P4 | ok | 0.0001 | 0.7994 | 0.0004 | 0.0001 | 0.0002 |
| P5 | ok | 0.0001 | 0.1976 | 0.0004 | 0.0005 | 0.0009 |
| P6 | ok | 0.0000 | 6.9254 | 0.0014 | 0.0018 | 0.0020 |
| P7 | timeout | 0.0001 |  |  |  |  |
| P8 | timeout | 0.0001 |  |  |  |  |
| P9 | timeout | 0.0001 |  |  |  |  |
| P10 | timeout | 0.0001 |  |  |  |  |
| P11 | timeout | 0.0001 |  |  |  |  |
| P12 | timeout | 0.0001 |  |  |  |  |
| P13 | timeout | 0.0001 |  |  |  |  |
| P14 | timeout | 0.0001 |  |  |  |  |
| P15 | timeout | 0.0000 |  |  |  |  |
| P16 | timeout | 0.0001 |  |  |  |  |
| P17 | timeout | 0.0001 |  |  |  |  |
| P18 | timeout | 0.0001 |  |  |  |  |
| P19 | timeout | 0.0001 |  |  |  |  |
| P20 | timeout | 0.0001 |  |  |  |  |

## ProbLog full run (trimmed_plus, timeout=60s, stdout-only)
| Case | Status | Propagating evidence_s | Grounding_s | Removing derivations of facts_s | Cycle breaking_s | Compiling SDD_s |
|--- | --- | --- | --- | --- | --- | ---|
| P1 | timeout |  |  |  |  |  |
| P3 | timeout |  |  |  |  |  |
| P4 | ok | 0.0000 | 0.1586 | 0.0008 | 0.0001 | 0.0003 |
| P5 | ok | 0.0001 | 0.2886 | 0.0012 | 0.0010 | 0.0018 |
| P6 | ok | 0.0001 | 0.6597 | 0.0020 | 0.0030 | 0.0041 |
| P7 | ok | 0.0001 | 4.3445 | 0.0053 | 0.0041 | 0.0058 |
| P8 | ok | 0.0001 | 7.2297 | 0.0079 | 0.0063 | 0.0068 |
| P9 | ok | 0.0001 | 5.3421 | 0.0070 | 0.0098 | 0.0115 |
| P10 | timeout | 0.0001 |  |  |  |  |
| P11 | timeout |  |  |  |  |  |
| P12 | timeout |  |  |  |  |  |
| P13 | timeout |  |  |  |  |  |
| P14 | timeout |  |  |  |  |  |
| P15 | timeout |  |  |  |  |  |
| P16 | timeout |  |  |  |  |  |
| P17 | timeout |  |  |  |  |  |
| P18 | timeout |  |  |  |  |  |
| P19 | timeout |  |  |  |  |  |
| P20 | timeout |  |  |  |  |  |

## Related commits
- `812ea4081` — docs(repo): refine README narratives
- `77d699f46` — docs(problog): refresh timing tables and notes
- `11d930a11` — docs(problog): record baseline timing runs
