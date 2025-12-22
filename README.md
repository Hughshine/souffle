# Souffle (Local Research Fork)

This repo extends upstream Souffle with a probabilistic pipeline and online incremental evaluation. It keeps upstream behavior but adds derivation-graph-based inference, DRed-like incremental updates, and rewrite prototypes.

## Status / Scope
- Active incremental path is `--online`. Legacy `--inc` backend is deprecated.
- Rewrite pipeline is full-mode only; incremental modes do not run rewrite.

## Build and Dependencies
- Build Souffle (release): `cmake --build cmake-build-release --target souffle -j4`
- CUDD is required for the BDD backend.
- SDD is optional, but must be built if you run with `-k sdd`.
- `ctest` is outdated in this fork and should not be used as a validation signal.

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
- Probabilities are read from `<rel>.prob` with line-for-line alignment; if missing, probabilities default to `1.0`.
- Some generators also accept Problog-style facts in `.problog.dl` such as `0.3::edge(1,2).` and translate them into `.facts`/`.prob`.
- This fork also accepts Problog-style probability prefixes on rules, e.g.:
  ```
  0.7::path(x,y) :- edge(x,y).
  ```
  which attaches a probabilistic coin to the rule application.

## Compile Programs (Generated C++)
Use `--online` to enable the online CLI and incremental path in the generated binary:
```
souffle --online -F ./input -D ./output compute.souffle.dl -o compute
```

Compiler notes:
- `-F` / `-D` at compile time set the default input/output directories baked into the binary.
- `--online` is required for the online incremental CLI and `_inc` strata generation.
- `-o` controls the output binary name.

## Runtime Options (Compiled Program)
Defaults are baked into each generated binary. Typical defaults in this repo (e.g., P12) are listed below; use `-h` for the authoritative values.
- `-F, --facts <DIR>`: input directory, default `input`
- `-D, --output <DIR>`: output directory, default `output`
- `-p, --profile <FILE>`: profile file, default empty (only if compiled with profiling)
- `-j, --jobs <NUM|auto>`: threads, default `1`
- `-k, --knowledge <bdd|sdd>`: default `bdd`
- `-l, --logfile <FILE>`: debugger JSON base name, default `log.txt`
- `-d, --derv-only <true|false>`: default `false`
- `-m, --setmode <inc-naive|inc-regional|full-hard|full-soft|full|elastic>`: default `inc-naive`
  - aliases: `inc`, `incr`, `incremental` map to `inc-naive`
  - `full` maps to `full-hard` (hard reset each turn); `full-soft` reuses the DD manager state
- `-e, --merge-bi-imp`: default `false`
- `-r, --rewrite`: default `false`
- `--dumpjson`: default `false`
- `--dumpdot`: default `false`
- `--dumpstat`: default `false`
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
- First turn uses facts/probabilities from the `-F` input directory to compute the baseline.
- Subsequent turns apply queued deltas (insert/delete + commit) and run incremental updates.

### First Turn Input Files (Required Format)
- For each `.input` relation `R`, provide `R.facts` in the `-F` directory.
  - One tuple per line, tab-separated fields, matching the `.decl` attribute order.
- Optional `R.prob` provides per-tuple probabilities:
  - One floating-point probability per line, aligned with `R.facts`.
  - If `R.prob` is missing, all tuples default to probability `1.0`.

## Differences From Upstream Souffle

### Full-Mode Changes
- Probabilistic semantics: derivation graph, pruning, forward compilation, weighted model counting.
- Knowledge backend selection via `-k bdd|sdd`.
- Optional SISO rewrite (`-r`) for full-mode runs only.
- Debug/profiling outputs: JSON/DOT/stats dumps after prune.

### Incremental (Online) Changes
- Online DRed-like delta relations (`$inc_delta_*`, `@inc_*`) and `_inc` strata.
- inc-naive vs inc-regional insertion paths; deletion uses DRed-like overdelete/rederive.
- Turn-based CLI with insert/delete/commit, and per-iteration probability outputs.

## Other README Files (Index)
- `README.dred.md`: online DRed internals, deletion bottlenecks, code pointers.
- `README.eval.inc.md`: incremental benchmark workflow and logs.
- `README.inc.region.md`: inc-regional pipeline design and profiling notes.
- `README.eval.md`: full-mode rewrite evaluation commands.
- `README.rewrite.md`: current rewrite pipeline behavior and performance.
- `README.rewrite.impl.md`: historical rewrite experiments and logs.
- `README.rewrite.120725.md`: historical notes (2025-12-07).
- `README.rewrite.120825.md`: historical notes (2025-12-08).
- `README.rewrite.2.md`: alternative rewrite plan (historical).
- `README.rewrite.conj.md`: pure-conjunctive rewrite design (plan).
- `README.siso.md`: SISO detection algorithm details.
- `README.ordering.md`: variable ordering roadmap (plan).
- `README.eqrel.md`: eqrel-style pruning design (plan).
- `README.lazy.md`: lazy DD design roadmap.
- `README.git.md`: local git hygiene for this repo.
- `experiments/side_channel_inc_mini/README.md`: mini incremental benchmark.
- `experiments/ground/README.txt`: ground-program frontend tests.
