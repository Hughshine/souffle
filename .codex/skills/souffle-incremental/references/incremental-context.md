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
- Note: `ELASTIC` is currently accepted by the CLI but **asserts at runtime** (unimplemented).
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
- Full outputs: `fact-iter<N>-full` (runner still accepts historical `-full-hard/-full-soft`).
- `side_channel_inc.py` searches these patterns and renames them with a `delta-<label>-<sample>-<mode>-` prefix.

## Benchmark Script (problog-benchmark)

- Script: `/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py`
  - Subcommands: `generate`, `strengthen`, `delta`, `compile`, `run`, `collect`, `clean`.
  - Global flags: `--base-dir`, `--source-dir`, `--log-file`, `--quiet`.
  - `generate`: `--cases`, `--seed`, `--cleanup`, `--force-smt`, `--rule-set {trimmed,trimmed_plus,full}`.
  - `strengthen`: `--augment-derivations` (fixed count) or `--augment-ratio` (scaled), plus
    `--augment-rel` and `--augment-seed`.
  - `delta`: size via `--change-spec`/`--change-cap`/`--change-cap-tol`, selection via
    `--delta-cluster {assign,id,random}`, sampling via `--sets` and `--seed`, `--cleanup` to reset delta/.
  - `compile`: `--souffle-arg` (repeatable) + `--jobs` + `--timeout`.
  - `run`: `--delta-labels`, `--delta-samples`, `--delta-root`, `--delta-shuffle`, `--delta-seed`,
    `--run-arg` (repeatable), `--compare-all`.
  - `collect`: aggregates per-delta JSON to TSV.
  - `compile` uses `souffle --online` to produce `./compute`.
    - Extra compile flags pass via `--souffle-arg` (repeatable); do **not** use `--full-only` for incremental runs.
    - Parallel compile: `--jobs <N>`.
  - Always run `compile` with `PATH=/home/hugh/research/datalog/souffle/build/src:$PATH` to avoid picking up `/usr/local/bin/souffle`.
  - `run` executes baseline `full` and `inc` runs, then feeds delta file commands to the CLI via stdin.
    - `--compare-all` forces per-delta runs for `full`, `inc-naive`, and `inc-regional`.
    - When `--compare-all` is set, any `--setmode` passed via `--run-arg` is ignored.
    - Extra runtime flags pass via `--run-arg` (repeatable), e.g. `--det-opt`, `--profile-inc-regional`.
  - Delta file format: delete facts -> `commit` -> insert facts with probabilities -> `commit` -> `q`.
  - Logs: `--logfile` JSON per run; stage summaries parsed into the per-delta JSON output.

## Related Docs

- `docs/USAGE.md`
- `/home/hugh/research/datalog/souffle/problog-benchmark/README.side-channel-inc.md`
- `README.eval.inc.md`
- `README.inc.region.md`

## Source references
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [problog-benchmark/side_channel_inc.py](problog-benchmark/side_channel_inc.py)

## Related commits
- `e214cd028` — docs(repo): refresh incremental docs and index
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
- `619e52197` — fix(inc): track explicit deletes and log deltas
