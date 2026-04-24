# Usage

## Source References

- [../src/MainDriver.cpp:619](../src/MainDriver.cpp#L619): compiler options.
- [../src/MainDriver.cpp:691](../src/MainDriver.cpp#L691): runtime-default canonicalization.
- [../src/synthesiser/Synthesiser.cpp:673](../src/synthesiser/Synthesiser.cpp#L673): generated online pipeline call.
- [../src/synthesiser/Synthesiser.cpp:4550](../src/synthesiser/Synthesiser.cpp#L4550): generated runtime defaults.
- [../src/include/souffle/CompiledOptions.h:191](../src/include/souffle/CompiledOptions.h#L191): mode and output syntax.
- [../src/include/souffle/CompiledOptions.h:700](../src/include/souffle/CompiledOptions.h#L700): generated runtime parser.
- [../src/include/souffle/cli/Cli.h:657](../src/include/souffle/cli/Cli.h#L657): online CLI commands.

## Input Format

For each `.input` relation `R`, provide `R.facts` in the fact directory. A
tuple is one tab-separated line matching the `.decl` order. An optional
`R.prob` file supplies one probability per fact line; absent `.prob` files make
the relation deterministic.

Rules may also carry ProbLog-style probabilities:

```souffle
0.7::path(x,y) :- edge(x,y).
```

## Compile

```bash
./build/src/souffle -F input -D output compute.souffle.dl -o compute
```

Compile-time `-F` and `-D` bake default runtime directories into the generated
binary. Runtime flags can override them.

AE-facing compiler options:

- `-F, --fact-dir <DIR>`: default fact directory.
- `-D, --output-dir <DIR>`: default output directory.
- `-o, --dl-program <FILE>`: generated executable.
- `--setmode=<MODE>`: default runtime mode, one of `inc-naive`, `inc-regional`, `full`.
- `--dump=<json|json-before-graph|json-before-prune|dot|stat>`: bake default graph dumps.
- `--profile-stage=<dred|inc|fc|wmc|inc-delete|inc-regional|dep-graph>`: bake default profiling stages.
- `--log-file=<FILE>`: default debugger log filename.

Inherited Souffle options such as `--jobs`, `--include-dir`, `--profile`,
`--show`, and warning controls remain available but are not incremental knobs.

## Runtime

AE-facing generated runtime options:

- `-F, --facts, --input-dir <DIR>`: fact directory.
- `-D, --output, --output-dir <DIR>`: output directory.
- `-m, --setmode=<MODE>`: turn mode, one of `inc-naive`, `inc-regional`, `full`.
- `--dump=<json|json-before-graph|json-before-prune|dot|stat>`: default-off graph dumps.
- `--profile-stage=<dred|inc|fc|wmc|inc-delete|inc-regional|dep-graph>`: default-off profiling output.
- `--logfile=<FILE>` or `--log-file=<FILE>`: debugger log filename.
- `-p, --profile=<FILE>`: profile output, only for binaries compiled with profiling enabled.
- `-j, --jobs=<N>`: runtime thread count when OpenMP is available.

There are no public backend, determinism, variable-index reuse, or reordering
switches in this AE branch. BDD, deterministic-relation analysis, variable index
reuse, and CUDD adaptive reordering are fixed implementation defaults.

## Online CLI

Commands:

- `insert [prob::]Rel(v1, v2, ...) [prob]`
- `delete Rel(v1, v2, ...)`
- `commit`
- `setmode inc-naive|inc-regional|full`
- `set dump <kind>` and `unset dump <kind>`
- `set profile-stage <stage>` and `unset profile-stage <stage>`
- `show config`
- `list`
- `help`
- `q`

Example:

```bash
./compute -F input -D output --setmode inc-regional
insert 0.3::edge(1,2)
delete edge(3,4)
commit
q
```

## Optional Outputs

The default run writes `facts.prob` and per-turn snapshots such as
`fact-iter2-inc-regional.prob`. To collect graph material:

```bash
./compute -F input -D output --setmode inc-regional \
  --dump=dot,json,stat --profile-stage=inc,wmc,fc
```

`dot` writes derivation graph DOT files, `json` writes post-prune graph JSON,
`json-before-graph` writes rule-application JSON before graph materialization,
`json-before-prune` writes the graph before pruning, and `stat` writes graph
statistics.
