# Usage

This document describes the evaluator-facing interface. The normal workflow has
two phases: compile a Datalog program into a benchmark binary, then run that
binary on a fact directory.

## Build the Compiler

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

Use `./build/src/souffle` from this build tree for artifact runs.

## Generate a Benchmark Binary

```bash
./build/src/souffle -F <facts-dir> -D <output-dir> compute.souffle.dl -o compute
```

The `-F` and `-D` arguments set default input and output directories in the
generated binary. Runtime arguments can override them.

## Run a Benchmark Binary

Plain run:

```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --logfile ae-plain
```

Rewrite run:

```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --rewrite --logfile ae-rewrite
```

`--det-opt` enables deterministic-relation analysis used by the benchmark
protocol. `--rewrite` enables the rewrite path used for the optimized artifact
comparison.

## Fact and Probability Files

Each input relation reads facts from `<relation>.facts` in the directory passed
with `-F`. Probabilities are optional and use `<relation>.prob` files. The two
files align by line number.

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
directory passed with `-D`. The `--logfile` argument controls the JSON timing log
base name in the same output directory.

## Rewrite Selection

`--rewrite` is the public switch for artifact runs. The runtime selects the
concrete rewrite path from the program metadata. Programs with probabilistic
rules use implicit split rewrite. Programs whose rules are deterministic use
explicit graph rewrite with split disabled.

## Source Map

- [../src/MainDriver.cpp](../src/MainDriver.cpp): compiler driver options.
- [../src/include/souffle/CompiledOptions.h](../src/include/souffle/CompiledOptions.h):
  generated-binary options.
- [../src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp): exact inference
  runtime pipeline.
- [../src/synthesiser/Synthesiser.cpp](../src/synthesiser/Synthesiser.cpp):
  generated C++ emission.
