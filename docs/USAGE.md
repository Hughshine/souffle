# Usage

This document describes the incremental artifact interface. The normal workflow
has two phases: compile a Datalog program into an online binary, then run that
binary on a fact directory and incremental turns.

## Build the Compiler

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

Use `./build/src/souffle` from this build tree for artifact runs.

## Generate an Online Binary

```bash
./build/src/souffle -F <facts-dir> -D <output-dir> compute.souffle.dl -o compute
```

The `-F` and `-D` arguments set default input and output directories in the
generated binary. Runtime arguments can override them.

## Run the Incremental CLI

Start the binary in a selected mode:

```bash
./compute -F <facts-dir> -D <output-dir> --setmode inc-naive
```

The first turn loads `<facts-dir>` and computes the baseline. Subsequent turns
apply queued deltas through the CLI:

```text
insert 0.4::edge(1,2)
delete edge(3,4)
commit
q
```

The maintained runtime modes are:

- `inc-naive`
- `inc-regional`
- `full-hard`
- `full-soft`
- staged combinations via `--sem-mode` and `--fc-mode`

`elastic` is accepted as a compatibility mode and currently falls back to
`inc-naive`.

## Input Files

Each input relation reads facts from `<relation>.facts` in the directory passed
with `-F`. Optional `<relation>.prob` files align line-for-line with those
facts.

Example:

```text
edge.facts
1	2
2	3

edge.prob
0.91
0.74
```

Rules may also carry ProbLog-style probabilities:

```souffle
0.7::path(x,y) :- edge(x,y).
```

## Output Files

The generated binary writes output tuple probabilities to `facts.prob` in the
directory passed with `-D`. Incremental turns also write per-turn probability
snapshots such as `fact-iter2-inc-naive.prob` and JSON timing logs controlled by
`--logfile`.

## Runtime Flags

The artifact-facing runtime flags are:

- `-F`, `--facts`, `--input-dir`
- `-D`, `--output`
- `-l`, `--logfile`, `--log-file`
- `-m`, `--setmode`
- `--sem-mode`, `--fc-mode`
- `--det-opt`, `--no-det-opt`, `--det-mode`
- `--dumpjson`, `--dumpdot`, `--dumpstat`, `--dump=<...>`
- `--profile-stage=<inc,fc,wmc,inc-delete,inc-regional,dep-graph>`
- `--post-del`
- `--no-reuse-var-index`
- `--trace-inc-regional=<tuple,...>`

The online CLI also supports:

- `setmode`
- `set sem-mode`
- `set fc-mode`
- `set dump` / `unset dump`
- `set profile-stage` / `unset profile-stage`
- `show config`
- `list`
- `commit`
- `help`
- `q`

## Companion Benchmark Workflow

For the side-channel artifact runs, use the companion `CAV-INC` benchmark tree
with:

```bash
sh/run_artifact_inc.sh
```

That helper drives generate/delta/compile/run/collect on the benchmark side.

## Source Map

- [../src/MainDriver.cpp](../src/MainDriver.cpp): compiler driver options.
- [../src/include/souffle/CompiledOptions.h](../src/include/souffle/CompiledOptions.h):
  generated-binary options.
- [../src/include/souffle/cli/Cli.h](../src/include/souffle/cli/Cli.h):
  online CLI and per-turn mode handling.
- [../src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp):
  incremental runtime pipeline.
