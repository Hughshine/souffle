# Full-Mode Benchmark Context (side_channel_full.py)

## Script CLI Map
- Script: `/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_full.py`
- Global flags: `--base-dir`, `--source-dir`, `--log-file`, `--quiet`.
- Subcommands:
  - `generate`: `--cases`, `--seed`, `--cleanup`, `--force-smt`, `--rule-set {trimmed,trimmed_plus,full}`.
  - `compile`: `--cases`, `--timeout`, `--souffle-arg` (repeatable).
  - `run`: `--cases`, `--timeout`, `--problog-only`, `--souffle-only`, `--souffle-arg` (repeatable).
  - `collect`: `--cases` (aggregates TSVs).
  - `clean`: removes generated artifacts under `--base-dir`.

## Soufflé Compile/Run Behavior
- Compile calls: `souffle --online --full-only -F input -D output <compute.souffle.dl> -o compute`.
- Run calls: `./compute -F input -D output --logfile log_<Case>_full` plus any `--souffle-arg`.

## Outputs and Artifacts
- Per-case outputs: `<base>/P*/output/`
  - `facts.full.prob`
  - `souffle.full.meta.json` (elapsed, exit, consistency vs ProbLog)
  - `problog.meta.json`, `problog.out` (if ProbLog runs)
  - `log_<Case>_full*.json` (debugger output)
- Aggregates (per base-dir):
  - `results-souffle.tsv`, `results-problog.tsv`, `results-graphs.tsv`

## Rewrite vs No-Rewrite Runs
- Use separate `--base-dir` (or copy outputs) to avoid overwriting `output/`.
- Pass `--souffle-arg=--rewrite` to enable rewrite in the full binary.

## Source references
- [problog-benchmark/side_channel_full.py](problog-benchmark/side_channel_full.py)
- [README.eval.md](README.eval.md)
- [README.rewrite.impl.md](README.rewrite.impl.md)

## Related commits
- `2a9472712` — perf(problog): streamline incremental WMC updates
