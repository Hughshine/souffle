# Incremental CLI and Benchmark Context

## Code Map (Souffle)

- `src/include/souffle/cli/Cli.h`: `IncrementalCLI` template, command parsing, commit flow for ground vs non-ground, output naming.
- `src/problog/Pipeline.cpp`: constructs `IncrementalCLI` after the initial full run for BDD or SDD pipelines when `--online` is enabled.
- `src/MainDriver.cpp`: CLI flags `--online` and `--setmode` wiring.
- `src/include/souffle/CompiledOptions.h`: incremental mode defaults and parsing.
- `src/ast2ram/online/`: online translation path used when `--online`.
- `src/include/souffle/problog/`: derivation graph, rule/query managers, forward compilation, PreDerivationGraph, DerivationManager maps.

## Incremental CLI Summary

- Modes: `FULL_HARD`, `FULL_SOFT`, `INC_NAIVE`, `INC_REGIONAL`, `ELASTIC` (string mapping via `--setmode`).
- Commands: `insert`, `delete/remove`, `list`, `commit`, `setmode`, `set`/`unset` (dumpjson/dumpdot/dumpstat), `dump`, `exit/quit/q`.
- Insert syntax: `[prob::]Rel(a,b,...) [prob]`. Delete ignores probability.
- Delta relation naming: `$inc_delta_tuple_insert_<rel>` and `$inc_delta_tuple_delete_<rel>`.

## Commit Flow (High Level)

- Ground mode (`isGround=true`): uses `PreDerivationGraph` to seed/retract facts, `recompute()` and `materialize()`, then prune, forward compilation, and WMC; writes `fact-iter<N>-inc*` or `fact-iter<N>-full*`.
- Non-ground mode: inserts into delta relations, runs `program->runAllInc()`, applies `graph->applyDelta()` using `DerivationManager` deltas, then prune, forward compilation, and WMC.

## Online Compilation Notes

- Incremental pipeline assumes `--online` is enabled and `--full-only` is not used.
- `--full-only` disables incremental code generation in online mode and should stay off for incremental CLI runs.

## Output Naming Coupling

- Incremental outputs: `fact-iter<N>-inc`, `fact-iter<N>-inc-naive`, or `fact-iter<N>-inc-regional`.
- Full outputs: `fact-iter<N>-full`, `fact-iter<N>-full-hard`, or `fact-iter<N>-full-soft`.
- `side_channel_inc.py` searches these patterns and renames them with a `delta-<label>-<sample>-<mode>-` prefix.

## Benchmark Script (problog-benchmark)

- Script: `/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py`
  - Subcommands: `generate`, `delta`, `compile`, `run`, `collect`, `clean`.
  - `compile` uses `souffle --online` to produce `./compute`.
  - Always run `compile` with `PATH=/home/hugh/research/datalog/souffle/build/src:$PATH` to avoid picking up `/usr/local/bin/souffle`.
  - `compile` supports `--jobs N` for parallel per-case builds.
  - `run` executes baseline `full` and `inc` runs, then feeds delta file commands to the CLI via stdin.
  - Delta file format: delete facts -> `commit` -> insert facts with probabilities -> `commit` -> `q`.
  - Logs: `--logfile` JSON per run; stage summaries parsed into the per-delta JSON output.

## Related Docs

- `/home/hugh/research/datalog/souffle/problog-benchmark/README.side-channel-inc.md`
- `README.eval.inc.md`
- `README.inc.region.md`
