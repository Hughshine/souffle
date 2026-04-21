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
- External scripts (for example in `problog-benchmark/`) may translate ProbLog
  inputs into `.facts`/`.prob`; the compiler itself does not auto-convert files.
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
Defaults are baked into each generated binary.  Artifact benchmark commands use
only the stable surface below:
- `-F, --facts <DIR>`: input directory.
- `-D, --output <DIR>`: output directory.
- `-l, --logfile <FILE>`: debugger JSON base name.
- `-k, --knowledge <bdd|sdd>`: DD backend; artifact runs use the default `bdd`.
- `--det-opt`: deterministic-relation analysis and graph gating.
- `-r, --rewrite`: artifact rewrite dispatcher.

Plain comparison runs omit only `--rewrite`:
```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --logfile ae-plain
```

Optimized runs add bare `--rewrite`:
```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --rewrite --logfile ae-rewrite
```

The dispatcher selects an implicit-split rewrite implementation when any rule
has a non-`1.0` probability.  If no rule has a probabilistic weight, it selects
the graph-rewrite implementation with no split.  This classification is based
on rule probabilities, not on `.prob` input-fact values.

### Diagnostic Runtime Options
These options are useful for implementation studies but are not part of the
artifact reproduction command:
- `--explicit-rewrite`: force the explicit graph-rewrite diagnostics.
- `--implicit-rewrite`: force the implicit-split diagnostics.
- `--split-mode=<no-split|naive-split>`: explicit diagnostic split policy.
  Do not pass `--split-mode` in artifact commands; explicit split controls
  bypass the smart dispatcher.
- `-d, --derv-only[=<true|false>]`: derivation graph only.  It does not run the
  rewrite pipeline and should not be used as a graph-only rewrite benchmark.
- `-e, --merge-bi-imp`, `--prune-extra`, `-C, --fold-const`, `--det-force`,
  `--post-del`, `--no-reuse-var-index`, and `--no-single-rand-fast`.
- Dump/profiling flags: `--dumpjson`, `--dumpdot`, `--dumpstat`, `--dumpconst`,
  `--dred-profile`, `--fc-profile`, `--profile-wmc`, and
  `--profile-dep-graph`.
- Incremental profiling flags remain outside the full-mode artifact path.

## Online Incremental CLI (Interactive or Batch)
Commands:
- `insert [prob::]Rel(v1, v2, ...) [prob]`: queue insertion (probability optional)
- `delete/remove Rel(v1, v2, ...)`: queue deletion
- `list`: show pending operations
- `commit`: apply pending operations and run incremental computation
- `setmode inc-naive|inc-regional|full-hard|full-soft|full|elastic`: switch mode
- `set dumpjson|dumpdot|dumpstat` / `unset ...`: toggle dump outputs
- `dump`: print current relations (or write `preDG_dump.dot` in ground mode)
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
