# Full-Mode Evaluation Workflow (Current)

## Scope
This is the current runnable workflow for full-only side-channel evaluation.
Use this document for commands and outputs. Old full evaluation logs are in
`docs/historical/README.eval.full.log.md`.

## Prerequisites
Build Souffle from repo root and put the binary on `PATH`:
```bash
cmake -S . -B build
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake --build build -j${JOBS}
export PATH="$(pwd)/build/src:$PATH"
```

## Generate / Compile / Run / Collect
```bash
python3 problog-benchmark/side_channel_full.py \
  --base-dir problog-benchmark/side_channel_full_eval \
  generate --cases 1,3-20 --cleanup --rule-set full

python3 problog-benchmark/side_channel_full.py \
  --base-dir problog-benchmark/side_channel_full_eval \
  compile --cases 1,3-20 --timeout 600

python3 problog-benchmark/side_channel_full.py \
  --base-dir problog-benchmark/side_channel_full_eval \
  run --cases 1,3-20 --timeout 1200

python3 problog-benchmark/side_channel_full.py \
  --base-dir problog-benchmark/side_channel_full_eval \
  collect --cases 1,3-20
```

## Rewrite vs No-Rewrite
Run in two separate base directories to avoid output collisions:
```bash
python3 problog-benchmark/side_channel_full.py \
  --base-dir problog-benchmark/side_channel_full_norewrite \
  run --cases 1,3-20 --timeout 1200 --souffle-only

python3 problog-benchmark/side_channel_full.py \
  --base-dir problog-benchmark/side_channel_full_rewrite \
  run --cases 1,3-20 --timeout 1200 --souffle-only --souffle-arg=--rewrite
```

Note: det-opt is enabled by default. Use `--souffle-arg=--no-det-opt` only for
ablation runs.

## Outputs To Inspect
- `<base-dir>/operation.log`
- `<base-dir>/results-souffle.tsv`
- `<base-dir>/results-problog.tsv` (if ProbLog is installed)
- `<base-dir>/P*/output/log_*.json`

## Source references
- [problog-benchmark/side_channel_full.py](problog-benchmark/side_channel_full.py)
- [problog-benchmark/README.side-channel-full.md](problog-benchmark/README.side-channel-full.md)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)

## Related commits
- `UNCOMMITTED` — docs(evaluation): rewrite full-mode evaluation README as current workflow
- `812ea4081` — docs(repo): refine README narratives
