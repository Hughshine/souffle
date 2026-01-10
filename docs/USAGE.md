# Usage

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
- Some generators also accept Problog-style facts in `.problog.dl` such as
  `0.3::edge(1,2).` and translate them into `.facts`/`.prob`.
- This fork also accepts Problog-style probability prefixes on rules, e.g.:
  ```
  0.7::path(x,y) :- edge(x,y).
  ```
  which attaches a probabilistic coin to the rule application.

## Compile Programs (Generated C++)
Online compilation is the default; `--online` remains accepted:
```
souffle -F ./input -D ./output compute.souffle.dl -o compute
```

Compiler notes:
- `-F` / `-D` at compile time set the default input/output directories baked
  into the binary.
- If you omit all compile/generate flags, the compiler defaults to `-o <basename>`
  (compile only) and prints a notice.
- Online CLI support and `_inc` strata are always enabled; `--online` is optional.
- `--full-only` disables incremental code generation but still uses the online
  compiler path.
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
- `-D, --output <DIR>`: output directory, default `output`
- `-p, --profile <FILE>`: profile file, default empty (only if compiled with profiling)
- `-k, --knowledge <bdd|sdd>`: default `bdd`
- `-l, --logfile <FILE>`: debugger JSON base name, default `log.txt`
- `-d, --derv-only <true|false>`: default `false`
- `-m, --setmode <inc-naive|inc-regional|full-hard|full-soft|full|elastic>`:
  default `inc-naive`
  - aliases: `inc`, `incr`, `incremental` map to `inc-naive`
  - `full` maps to `full-hard` (hard reset each turn); `full-soft` reuses the
    DD manager state
- `-e, --merge-bi-imp`: default `false`
- `-r, --rewrite`: default `false`
- `--det-opt`: default `false` (enable deterministic-first derivation gating)
- `--split-mode=<no-split|naive-split|complete-split>`: default `naive-split`
  (rewrite only)
- `--dumpjson`: default `false`
- `--dumpdot`: default `false`
- `--dumpstat`: default `false`
- `--dred-profile`: default `false` (requires compile-time `--profile --dred-profile` to emit DRed sub-phase timers;
  per-SCC workload counters also need `--dumpstat`)
- `-h`: help

## Online Incremental CLI (Interactive or Batch)
Commands:
- `insert [prob::]Rel(v1, v2, ...) [prob]`: queue insertion (probability optional)
- `delete/remove Rel(v1, v2, ...)`: queue deletion
- `list`: show pending operations
- `commit`: apply pending operations and run incremental computation
- `setmode inc-naive|inc-regional|full-hard|full-soft|full|elastic`: switch mode
- `set dumpjson|dumpdot|dumpstat` / `unset ...`: toggle dump outputs
- `dump`: print current relations (or PreDG in ground mode)
- `help`, `q`: help/quit

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
- Deterministic-first derivation gating (`--det-opt`) to skip recording
  derivations for deterministic relations.
- Optional deterministic constant pre-analysis in FC (`--fold-const`); use
  `--dumpconst` to write true/false nodes and edges (see `README.const.md`).
- Debug/profiling outputs: JSON/DOT/stats dumps after prune.

### Incremental (Online) Changes
- Online DRed-like delta relations (`$inc_delta_*`, `@inc_*`) and `_inc` strata.
- inc-naive vs inc-regional insertion paths; deletion uses DRed-like
  overdelete/rederive.
- Turn-based CLI with insert/delete/commit, and per-iteration probability outputs.
