# Usage

## Source references
- [../src/MainDriver.cpp](../src/MainDriver.cpp) (compiler defaults, `--online`)
- [../src/include/souffle/CompiledOptions.h](../src/include/souffle/CompiledOptions.h) (compiled-program flags + defaults)
- [../src/include/souffle/cli/Cli.h](../src/include/souffle/cli/Cli.h) (incremental CLI modes/output naming)
- [../src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp) (rewrite vs incremental CLI gating)

## Program Syntax (souffle.dl example)
```souffle
.decl edge(u:number, v:number)
.decl path(u:number, v:number)
.input edge
.output path

path(x,y) :- edge(x,y).
path(x,z) :- path(x,y), edge(y,z).
```

Notes:
- Facts are read from `-F` input directory as `<rel>.facts`.
- Probabilities are read from `<rel>.prob` with line-for-line alignment; if
  missing, probabilities default to `1.0`.
- External scripts (for example in `problog-benchmark/`, with an optional local
  `work/benchmarks/problog-benchmark/` overlay) may translate ProbLog inputs
  into `.facts`/`.prob`; the compiler itself does not auto-convert files.
- This fork also accepts Problog-style probability prefixes on rules, e.g.:
  ```
  0.7::path(x,y) :- edge(x,y).
  ```
  which attaches a probabilistic coin to the rule application.

## Compile Programs (Generated C++)
Online compilation is the default; `--online` remains accepted (but is redundant in this fork):
```
souffle -F ./input -D ./output compute.souffle.dl -o compute
```

Compiler notes:
- `-F` / `-D` at compile time set the default input/output directories baked
  into the binary.
- Canonical compiler defaults for generated runtimes are also accepted:
  `--input-dir`, `--profile-file`, `--log-file`, `--sem-mode`, `--fc-mode`,
  `--full-evaluator`, `--dd-backend`, `--rewrite-engine`, `--rewrite-split`,
  `--rewrite-detect`, `--det-mode`, `--dump`, `--profile-stage`,
  `--trace-inc-regional`.
- If you omit all compile/generate flags, the compiler defaults to `-o <basename>`
  (compile only) and prints a notice.
- Online compilation is always used; `--full-only` disables incremental code
  generation and the incremental CLI but still uses the online compiler path.
- `--dred-profile` (compile-time) adds detailed DRed sub-phase timers and per-SCC attribution; requires
  compile-time `--profile` and runtime `--dred-profile -p <file>` to emit JSON.
- `-o` controls the output binary name.
- Generated programs link against the precompiled runtime library built by CMake
  (`compiled`); keep the build tree (or install the library) available for
  `souffle-compile.py`.

## Runtime Options (Compiled Program)
Defaults are baked into each generated binary. Typical defaults in this repo are
listed below; use `-h` for the authoritative values.
- `-F, --facts <DIR>`: input directory, default `input`
- `--input-dir <DIR>`: canonical alias for `--facts`
- `-D, --output <DIR>`: output directory, default `output`
- `-p, --profile <FILE>`: profile file, default empty (only if compiled with profiling)
- `--profile-file <FILE>`: canonical alias for `--profile`
- `-k, --knowledge <bdd|sdd>`: default `bdd`
- `--dd-backend <bdd|sdd>`: canonical alias for `--knowledge`
- `-l, --logfile <FILE>`: debugger JSON base name, default `log.txt`
- `--log-file <FILE>`: canonical alias for `--logfile`
- `-d, --derv-only[=<true|false>]`: default `false` (omit value to set `true`)
- `--derivation-only[=<true|false>]`: canonical alias for `--derv-only`
- `-m, --setmode <inc-naive|inc-regional|full-hard|full-soft|full|elastic>`:
  default `inc-naive`
  - aliases: `inc`, `incr`, `incremental` map to `inc-naive`
  - `full` maps to `full-hard` (hard reset each turn); `full-soft` reuses the
    DD manager state
  - `elastic` is accepted as a compatibility mode and currently falls back to
    `inc-naive` at execution time
- `--sem-mode <full|inc>` / `--fc-mode <full-hard|full-soft|inc-naive|inc-regional|elastic>`:
  canonical mode selectors; staged combinations are valid at runtime
- `--full-evaluator <exact|scbf|approx>`: canonical full evaluator selector
  - `scbf` is supported
  - `approx` is parsed but currently rejected by compiled runtimes
- `--approx-backend <none|amc>`: reserved canonical approx selector; compiled runtimes currently reject non-`none`
- `-e, --merge-bi-imp`: always enabled in full-only binaries (`--full-only` at
  compile time); it is forced off in online/incremental binaries.
- `--no-merge-bi-imp`: disable merge-bi-imp even if the binary default enables it
- `--prune-extra`: default `false` (enable outputless-component pruning in prune)
- `--no-prune-extra`: canonical disable override
- `-r, --rewrite`: default `false`
- `--rewrite-engine <off|legacy|implicit|implicit-iter>`: canonical rewrite selector
- `--force-complete-siso-detect`: default `false` (disable dirty-frontier SISO detection and force full-graph detection each rewrite iteration)
- `--rewrite-detect <dirty-frontier|complete>`: canonical rewrite detection selector
- `--det-opt`: default `true` (enable deterministic-relation analysis + derivation gating)
- `--no-det-opt`: default `false` (disable deterministic-relation analysis + derivation gating)
- `--det-force`: default `false` (force deterministic evaluation; skip derivation graph and emit 1.0 probs)
- `--det-mode <auto|off|force>`: canonical determinism selector
- `--split-mode=<no-split|naive-split|complete-split>`: default `naive-split`
  (rewrite only; aliases: `none|naive|complete`)
- `--rewrite-split=<off|naive|complete>`: canonical split selector
- `--dumpjson`: default `false`
- `--dumpdot`: default `false`
- `--dumpstat`: default `false`
- `--dumpconst`: default `false` (write const-prepass details; see `docs/topics/pipeline/README.const.md`)
- `--dump=<json,dot,stat,const>`: canonical dump selector (repeat or comma-separated)
- `--dred-profile`: default `false` (requires compile-time `--profile --dred-profile` to emit DRed sub-phase timers;
  per-SCC workload counters also need `--dumpstat`)
- `--inc-profile`: default `false` (print per-stage incremental timings to stdout)
- `--fc-profile`: default `false` (print detailed forward-compilation sub-phase counters/timings to stdout)
- `--profile-wmc`: default `false` (print weighted model counting timing/call breakdowns to stdout)
- `--profile-inc-regional`: default `false` (inc-regional diagnostics + timing summary)
- `--profile-inc-regional-heavy`: default `false` (extra inc-regional tracing; large output)
- `--inc-regional-trace-tuples=<LIST>`: default empty (comma-separated tuples to trace)
- `--trace-inc-regional=<LIST>`: canonical alias for `--inc-regional-trace-tuples`
- `--profile-dep-graph`: default `false` (dependency-graph profiling)
- `--profile-stage=<dred,inc,fc,wmc,inc-delete,inc-regional,inc-regional-heavy,dep-graph>`:
  canonical profile selector (repeat or comma-separated)
- `--post-del`: default `false` (enable post-delete variable postprocess in FC)
- `--no-reuse-var-index`: default `false` (disable reuse of freed DD variable indices)
- `--no-single-rand-fast`: default `false` (disable single-randvar FC fast path)
- `-h`: help

## Online Incremental CLI (Interactive or Batch)
Commands:
- `insert [prob::]Rel(v1, v2, ...) [prob]`: queue insertion (probability optional)
- `delete/remove Rel(v1, v2, ...)`: queue deletion
- `list`: show pending operations
- `commit`: apply pending operations and run incremental computation
- `setmode inc-naive|inc-regional|full-hard|full-soft|full|elastic`: switch mode
- `set sem-mode <full|inc>` / `set fc-mode <...>`: canonical online mode controls
- `set dump <json|dot|stat|const>` / `unset dump <...>`: canonical dump toggles
- `set profile-stage <...>` / `unset profile-stage <...>`: canonical profiling toggles
- `set dumpjson|dumpdot|dumpstat|dumpconst` / `unset ...`: legacy dump toggles
- `show config`: print current mode and mutable runtime config
- `dump`: print current relations (or write `preDG_dump.dot` in ground mode)
- `help`, `q`: help/quit

Notes:
- Startup-only settings such as `rewrite-engine`, `rewrite-split`, `rewrite-detect`,
  `full-evaluator`, `dd-backend`, `approx-backend`, and `det-mode` are not hot-swappable
  in the online CLI.

Example (interactive):
```
./compute -F input -D output --setmode inc
insert 0.3::Edge(1,2)
delete Edge(3,4)
commit
q
```

Example (batch from delta file):
```
./compute -F input -D output --setmode inc < delta/inc10_1.txt
```

## First Turn vs Subsequent Turns
- First turn uses facts/probabilities from the `-F` input directory to compute
  the baseline.
- Subsequent turns apply queued deltas (insert/delete + commit) and run
  incremental updates.

### First Turn Input Files (Required Format)
- For each `.input` relation `R`, provide `R.facts` in the `-F` directory.
  - One tuple per line, tab-separated fields, matching the `.decl` attribute order
    (default delimiter `\t`).
  - Override per relation via IO params, e.g. `.input R(delimiter=",")` /
    `.output R(delimiter=",")`.
- Optional `R.prob` provides per-tuple probabilities:
  - One floating-point probability per line, aligned with `R.facts`.
  - If `R.prob` is missing, all tuples default to probability `1.0`.

## Differences From Upstream Souffle

### Full-Mode Changes
- Probabilistic semantics: derivation graph, pruning, forward compilation,
  weighted model counting.
- Knowledge backend selection via `-k bdd|sdd`.
- Optional SISO rewrite (`-r`) for full-mode runs only.
- Rewrite path uses component-wise FC with a single reused BDD manager and logs
  `FC_WMC_HYBRID`.
- Deterministic-first derivation gating (`--det-opt`, default on; disable with
  `--no-det-opt`) to skip recording
  derivations for deterministic relations.
- Deterministic-force mode (`--det-force`) to bypass the derivation graph and
  emit probability `1.0` for all outputs (useful for deterministic baselines).
- Optional deterministic constant pre-analysis in FC (`--fold-const`); use
  `--dumpconst` to write true/false nodes and edges (see `docs/topics/pipeline/README.const.md`).
- Debug/profiling outputs: JSON/DOT/stats dumps after prune.

### Incremental (Online) Changes
- Online DRed-like delta relations (`$inc_delta_*`, `@inc_*`) and `_inc` strata.
- inc-naive vs inc-regional insertion paths; deletion uses DRed-like
  overdelete/rederive.
- Turn-based CLI with insert/delete/commit, and per-iteration probability outputs.

## Related commits
- `4bf38b2a1` — perf(problog): make inc preConfig delta-scoped
- `9ea1b4b53` — perf(problog): add inc preConfig toggle
- `211b03b71` — chore(problog): add det-force runtime flag
