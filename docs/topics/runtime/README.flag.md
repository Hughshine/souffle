# Flags Reference (Compiler vs Generated Program)

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [src/synthesiser/Synthesiser.cpp](src/synthesiser/Synthesiser.cpp)
- [docs/USAGE.md](docs/USAGE.md)

## Scope
- **Compiler flags** apply to the `souffle` executable and control code generation.
- **Runtime flags** apply to the generated `./compute` binary (compiled program).
- Some flags exist in both layers (e.g., `-F`, `-D`, `-p`, `-m`, `-d`); compile-time values
  seed defaults in the generated binary and runtime flags override them.

## Compiler flags (`souffle`)

### Code generation & execution
- `-c`, `--compile`: generate C++ → compile → run.
- `-C`, `--compile-many`: generate multi-file C++ → compile → run.
- `-o`, `--dl-program <FILE>`: generate C++ to `<FILE>` and compile (no run).
- `-g`, `--generate <FILE>`: generate C++ to `<FILE>` (or `-` for stdout).
- `-G`, `--generate-many <DIR>`: generate multi-file C++ to `<DIR>`.
- `-N`, `--generate-namespace <NS>`: namespace for generated C++ (empty for anonymous).
- `-j`, `--jobs <N|auto>`: run the compiler in parallel (`auto` uses system default).
  Note: treat as **not allowed** in current workflows; keep `1` unless OpenMP is enabled
  (without OpenMP, non-`1` only emits a warning and runs serially).

### Inputs/outputs & include paths
- `-F`, `--fact-dir <DIR>`: default facts dir baked into the binary.
- `-D`, `--output-dir <DIR>`: default output dir baked into the binary (`-` for stdout).
- `-I`, `--include-dir <DIR>`: include directories (repeatable).
- `-L`, `--library-dir <DIR>`: library directories (repeatable).
- `-l`, `--libraries <FILE>`: libraries to link (repeatable).

### Preprocessor & macros
- `-M`, `--macro <MACROS>`: define preprocessor macros.
- `--preprocessor <CMD>`: set preprocessor.
- `--no-preprocessor`: disable preprocessor.

### Optimization/transform controls
- `-z`, `--disable-transformers <TRANSFORMERS>`: disable specific AST transforms.
- `--no-souffle-opt`: disable RAM optimizations (transformations, join order, guard hoisting).
- `--inline-exclude <RELATIONS>`: prevent inlining for listed relations.
- `-m`, `--magic-transform <RELATIONS>`: enable magic-set on relations (`*` for all).
- `--magic-transform-exclude <RELATIONS>`: exclude relations from magic-set (implies inline-exclude).
- `-a`, `--auto-schedule <FILE>`: use auto-schedule profile.
- `--emit-statistics`: collect auto-schedule stats (requires `--profile`).

### Online/incremental codegen
- `-O`, `--online`: enable online compilation (incremental CLI support).
- `-x`, `--full-only`: generate full-only code (disable incremental CLI paths).
- `--setmode <MODE>`: default runtime mode (`inc-naive`, `inc-regional`, `full`, `elastic`).
- `-d`, `--derv-only`: default runtime “derivation-only” mode.

### Diagnostics, warnings, and metadata
- `-r`, `--debug-report <FILE>`: HTML debug report.
- `--show <MODE>`: print AST/RAM/graph views.
- `--parse-errors`: show parse errors and exit.
- `-p`, `--profile <FILE>`: enable profiling in the generated binary and set default profile output.
- `--dred-profile`: enable detailed DRed profiling instrumentation.
- `--profile-frequency`: enable profiler frequency counter.
- `-W`, `--warn <WARN>` / `--wno <WARN>` / `-w`, `--no-warn`: warning controls.
- `-v`, `--verbose`: verbose output.
- `-s`, `--swig <LANG>`: generate SWIG interface (`java`, `python`).
- `--legacy`: enable legacy support.
- `-P`, `--pragma <OPTIONS>`: pragma options (repeatable).
- `-h`, `--help`: show help.
- `--version`: show version.

## Runtime flags (generated `./compute`)

### IO, logging, and scheduling
- `-F`, `--facts <DIR>`: input facts directory.
- `-D`, `--output <DIR>`: output directory (`-` for stdout).
- `-l`, `--logfile <FILE>`: debugger JSON base name (emits `<name>.json` in output dir).
- `-j`, `--jobs <N|auto>`: threads (OpenMP only).
  Note: treat as **not allowed** in current workflows; keep `1` unless OpenMP is enabled
  (without OpenMP, non-`1` only emits a warning and runs serially).
- `-p`, `--profile <FILE>`: profiling output (requires compile-time `--profile`).

### Help
- `-h`: show usage.

### Mode selection
- `-m`, `--setmode <MODE>`: `inc-naive` (aliases: `inc`, `incr`, `incremental`),
  `inc-regional`, `full-hard` (alias: `full`), `full-soft`, `elastic` (accepted but asserts).
- `-d`, `--derv-only[=<true|false>]`: derivation graph only.

### Semantics / graph transforms
- `-e`, `--merge-bi-imp`: merge mutually implying deterministic nodes (full-only safe).
- `--prune-extra`: drop outputless components during prune.
- `-C`, `--fold-const`: constant pre-analysis (negation ignored).
- `-r`, `--rewrite`: enable SISO-based rewrite.
- `-P`, `--split-mode <no-split|naive-split|complete-split>`: split mode for rewrite (default `naive-split`; aliases `none|naive|complete`).
- `--force-complete-siso-detect`: disable dirty-frontier SISO detection and force full-graph detection each rewrite iteration (for rewrite diff/validation runs).
- `-k`, `--knowledge <bdd|sdd>`: choose DD backend.

### Determinism controls
- `--det-opt`: enable deterministic-relation analysis (default on).
- `--no-det-opt`: disable deterministic-relation analysis.
- `--det-force`: skip derivation graph and force probabilities to 1.0.

### Dumps & debugging
- `--dumpjson`: dump derivation graph JSON after prune.
- `--dumpdot`: dump derivation graph DOT after prune.
- `--dumpstat`: dump derivation graph stats after prune.
- `--dumpconst`: dump constant pre-analysis details.

### Profiling toggles
- `--dred-profile`: detailed DRed profiling (requires compile-time profiling).
- `--inc-profile`: incremental stage totals (SEM/PRN/FC/WMC and per-turn totals).
- `--fc-profile`: forward compilation sub-phase profiling (preConfig, loops, reorders, etc).
- `--profile-inc-delete`: delete-phase breakdown (inc only).
- `--profile-wmc`: weighted model counting sub-phase profiling.
- `--profile-inc-regional`: inc-regional diagnostics (analyze/plan/rebuild/calibrate + region stats).
- `--profile-inc-regional-heavy`: heavy inc-regional diagnostics (very large output).
- `--inc-regional-trace-tuples=<LIST>`: comma-separated tuples to trace.
- `--profile-dep-graph`: dependency-graph profiling.

### Profiling flags: what they measure and cost
The table below focuses on runtime profiling flags used by probabilistic full/inc pipelines.

| Flag | Main outputs / measured scope | Typical log prefix | Relative overhead |
|---|---|---|---|
| `--inc-profile` | Stage-level totals and coarse breakdowns (`IO/SEM/PRN/FC/WMC`) for full/inc paths; prune/apply-delta timers and counts. | `[inc-profile]` | Low to medium |
| `--fc-profile` | Fine-grained forward-compilation counters/timers (formula ops, round/worklist stats, CUDD preconfig/create-var timings). | `[fc-profile]` | Medium to high |
| `--profile-wmc` | WMC sub-phase timings and call counts (AND/WMC hot paths), especially in hybrid/component evaluation. | (Pipeline WMC profile lines) | Medium |
| `--profile-inc-delete` | Incremental delete-only phase breakdown (`conditioning`, `overdelete`, `rederive`, affected node/edge counts). | `[inc-delete-profile]` | Medium (inc delete turns) |
| `--profile-inc-regional` | Inc-regional analysis/planning/rebuild diagnostics and timing summaries, plus region stats. | `[inc-regional-profile]` | Medium to high |
| `--profile-inc-regional-heavy` | Extra deep traces for regional analysis (closure-style traces, large diagnostic payloads). | `[inc-regional-profile]` (heavy detail) | Very high |
| `--profile-dep-graph` | Dependency graph internals (`SCC`, dependency edges, component/depth timings). | `[dep-profile]` | Low to medium |
| `--dred-profile` | Detailed DRed instrumentation in incremental maintenance paths. | DRed profile lines | Medium to high |

Notes:
- Relative overhead is workload-dependent; for large graphs, I/O and extra logging volume can dominate.
- `--fc-profile` and `--profile-inc-regional-heavy` can generate very large stdout/stderr and materially perturb runtime.
- Prefer layered enablement: start with `--inc-profile`, then add one deep flag at a time.

### Timeout behavior for profiling
- Many counters are emitted at stage end. If a run is killed by timeout while a stage is still `running`, end-of-stage stats may be missing.
- In particular, values like final DD `live_nodes` are typically recorded after FC/hybrid stage completion; a timeout during that stage may leave only partial stage time and no final node-count stats.
- Stage completion and partial status are still visible in debugger JSON (`status`, `time_seconds`, `time_seconds_is_partial`).

### Profiling organization (recommended)
- **Layered flags**: use `--inc-profile` for stage totals, then add `--fc-profile`
  and/or `--profile-wmc` for deep breakdowns.
- **Same flags for full + inc**: FC/WMC profiling applies to both modes; identify
  full vs inc by the CLI log context and stage labels (no separate full-only flags).
- **Incremental-specific add-ons**: `--profile-inc-delete` and `--profile-inc-regional`
  are optional extras; avoid `--profile-inc-regional-heavy` unless debugging.
- **Performance overhead**: hit/miss counters and fine-grained timing are guarded
  by profiling flags to avoid extra cost when profiling is off.

### Benchmarking quick set (side_channel_inc.py)
- `--det-opt` is enabled by default for incremental benchmarks; use `--no-det-opt`
  only for ablation/comparison runs.
- Suggested incremental profiling bundle:
  `--inc-profile --profile-inc-regional --profile-wmc`
- Add `--profile-inc-delete` when analyzing delete turns, and `--fc-profile` for FC details.
- Use `--profile-inc-regional-heavy` only for deep region tracing (very expensive).

### BDD/FC maintenance
- `--post-del`: post-delete variable reordering.
- `--no-reuse-var-index`: disable reuse of freed CUDD variable indices.
- `--no-single-rand-fast`: disable single-randvar fast path in component FC.

## Notes on overlap
- `-m` means **magic-set** at compile time but **setmode** at runtime.
- `-d`/`--derv-only` exists at both layers; compile-time sets the default, runtime overrides.
- `-F`/`-D`/`-p` exist at both layers; compile-time values become defaults in the binary.
- `-j` exists at both layers but controls different things (compiler threads vs runtime threads).
- `-r` is debug-report at compile time vs rewrite at runtime (different semantics).

## Related commits
- `c300c6951` — docs(opt): update WMC table and summary
- `21f1c666d` — perf(inc-wmc): cut regional cache churn
- `8a5764e19` — chore(Pipeline): add input fact size profiling
