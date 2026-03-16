# Incremental Evaluation Workflow (Current)

## Scope
This is the current runnable workflow for side-channel incremental evaluation.
Use this document for commands and output locations. Use `docs/historical/README.eval.inc.log.md`
for old long-form run logs.

## Prerequisites
Build Souffle from repo root:
```bash
cmake -S . -B build
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake --build build -j${JOBS}
export PATH="$(pwd)/build/src:$PATH"
```

## Standard Incremental Run
```bash
python3 problog-benchmark/benchmarks/side_channel/cli/side_channel_inc.py \
  --base-dir problog-benchmark/benchmarks/side_channel/runs/inc_eval \
  generate --cases 12-20 --cleanup --rule-set full

python3 problog-benchmark/benchmarks/side_channel/cli/side_channel_inc.py \
  --base-dir problog-benchmark/benchmarks/side_channel/runs/inc_eval \
  delta --cases 12-20 --cleanup

python3 problog-benchmark/benchmarks/side_channel/cli/side_channel_inc.py \
  --base-dir problog-benchmark/benchmarks/side_channel/runs/inc_eval \
  compile --cases 12-20 --timeout 600 --jobs 4

python3 problog-benchmark/benchmarks/side_channel/cli/side_channel_inc.py \
  --base-dir problog-benchmark/benchmarks/side_channel/runs/inc_eval \
  run --cases 12-20 --timeout 900 --compare-all

python3 problog-benchmark/benchmarks/side_channel/cli/side_channel_inc.py \
  --base-dir problog-benchmark/benchmarks/side_channel/runs/inc_eval \
  collect --cases 12-20
```

## Mixed Delta Run (Recommended for inc-regional comparisons)
```bash
python3 problog-benchmark/benchmarks/side_channel/cli/side_channel_inc.py \
  --base-dir problog-benchmark/benchmarks/side_channel/runs/inc_eval_mix \
  delta --cases 13-20 --cleanup --delta-strategy mix --sets 5 \
  --mix-ratio 0.015 --mix-cap 450 --seed 42

python3 problog-benchmark/benchmarks/side_channel/cli/side_channel_inc.py \
  --base-dir problog-benchmark/benchmarks/side_channel/runs/inc_eval_mix \
  run --cases 13-20 --timeout 1200 --compare-all --delta-runs 5

python3 problog-benchmark/benchmarks/side_channel/cli/side_channel_inc.py \
  --base-dir problog-benchmark/benchmarks/side_channel/runs/inc_eval_mix \
  collect --cases 13-20
```

Note: det-opt is enabled by default. Use `--run-arg=--no-det-opt` only for
ablation runs.

## Outputs To Inspect
- `<base-dir>/operation_inc.log`
- `<base-dir>/results-souffle-inc.tsv`
- `<base-dir>/P*/output/delta-*/sample-*/run-*/delta-*.json`
- `<base-dir>/P*/output/log_*.json`

Latest archived batch summary:
- `archive/2026-02-11/README.md`

## Source references
- [problog-benchmark/benchmarks/side_channel/cli/side_channel_inc.py](problog-benchmark/benchmarks/side_channel/cli/side_channel_inc.py)
- [problog-benchmark/benchmarks/side_channel/docs/README.side-channel-inc.md](problog-benchmark/benchmarks/side_channel/docs/README.side-channel-inc.md)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [archive/2026-02-11/README.md](archive/2026-02-11/README.md)

## Related commits
- `UNCOMMITTED` — docs(evaluation): rewrite incremental evaluation README as current workflow
- `e214cd028` — docs(repo): refresh incremental docs and index
