# Incremental Artifact (Side-Channel)

## Source references
- [problog-benchmark/side_channel_inc.py](problog-benchmark/side_channel_inc.py)
- [sh/run_artifact_inc.sh](sh/run_artifact_inc.sh)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)


This document provides a minimal, reproducible path to run the incremental
artifact experiments for the side-channel benchmark:

- SEM stage performance (per-turn SEM/PRN/FC/WMC from logs)
- INC_NAIVE delete/insert performance (setmode `inc`)

For full script details, see:
`problog-benchmark/README.side-channel-inc.md`.

## Prereqs

- Python 3.9+
- Souffle built with `--online` CLI support (use repo build commands).
- problog-benchmark repo checked out on the `inc-artifact` branch.

Build Souffle from the repo root:
```bash
cmake -S . -B build
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake --build build -j${JOBS}
```

## Quick Run (P12–P20, full ruleset)

Run the helper script from the Souffle repo root:
```bash
sh/run_artifact_inc.sh
```

Optional (for regional/disjunction experiments): run strengthen before delta:
```bash
cd /path/to/problog-benchmark
python3 side_channel_inc.py --base-dir <base-dir> strengthen --cases P12,P13,... --augment-derivations 200
```

The script defaults to:
- Cases: `P12`–`P20`
- Rule set: `full`
- Delta sizes: `inc0p1=0.001`, `inc0p3=0.003`, `inc0p5=0.005`
- Runs: `--det-opt` (reuse-var-index enabled by default; disable with `--no-reuse-var-index`)
- Output base dir: `../problog-benchmark/side_channel_inc_artifact`

You can override defaults via environment variables:
```bash
PROBLOG_BENCH=/path/to/problog-benchmark \
BASE_DIR=/tmp/side_channel_inc_artifact \
CASES=P12,P13,P14 \
RULE_SET=full \
CHANGE_SPEC=inc0p1=0.001,inc0p3=0.003,inc0p5=0.005 \
DELTA_LABELS=inc0p1,inc0p3,inc0p5 \
DELTA_SAMPLES=1 \
TIMEOUT=1200 \
sh/run_artifact_inc.sh
```

## Outputs to Inspect

Under `<base-dir>/P*/output/`:
- `facts.full.prob`, `facts.inc.prob` (baseline outputs)
- `delta-<label>-<sample>-*.prob` (per-run outputs)
- `delta-<label>-<sample>.json` (comparisons + per-turn stage summaries)

Summary file:
- `<base-dir>/results-souffle-inc.tsv` (end-to-end times + `OK` flag)

Stage breakdown (SEM/PRN/FC/WMC) is recorded per turn in each
`delta-<label>-<sample>.json`. Compare turn 2 (delete) and turn 3 (insert)
between `inc` and `full`.

## Notes

- `setmode inc` maps to `INC_NAIVE` in the CLI.
- Incremental runs keep a persistent DD manager across turns.
- Full runs reset the manager each turn in `full-hard`; `full-soft` reuses the manager state.
- `--full-only` must remain disabled for incremental runs (the script does not use it).

## Related commits
- `b22a891b0` — fix(inc): sync det-opt deltas and artifact docs
