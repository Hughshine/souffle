# Usage

## Source references
- [../src/MainDriver.cpp](../src/MainDriver.cpp)
- [../src/include/souffle/CompiledOptions.h](../src/include/souffle/CompiledOptions.h)
- [../src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp)
- [../src/synthesiser/Synthesiser.cpp](../src/synthesiser/Synthesiser.cpp)

## Program Syntax
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
- Probabilities are read from `<rel>.prob` with line-for-line alignment; if a
  probability file is missing, all tuples in that relation default to `1.0`.
- This fork also accepts ProbLog-style probability prefixes on rules, for
  example `0.7::path(x,y) :- edge(x,y).`.

## Compile Generated Programs
Build the compiler first:
```bash
cmake -S . -B build
cmake --build build -j${JOBS}
```

Generate a benchmark binary:
```bash
./build/src/souffle -F <facts-dir> -D <output-dir> compute.souffle.dl -o compute
```

Compiler notes:
- `-F` and `-D` at compile time set default input/output directories baked into
  the generated binary.
- `-o` controls the output binary name.
- Generated programs link against the precompiled runtime library built by
  CMake; keep the build tree available for generated binaries.

## Runtime Options
Artifact benchmark commands use only the stable generated-program surface below:
- `-F, --facts <DIR>`: input directory.
- `-D, --output <DIR>`: output directory.
- `-l, --logfile <FILE>`: debugger JSON base name.
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

Diagnostic runtime flags such as dumps and profiling remain accepted by
generated binaries for debugging, but they are not part of the AE command line.
The generated help page intentionally shows only the stable artifact surface.

## Benchmark Artifact
Benchmark scripts, generated facts, and case metadata are in the companion
benchmark artifact branch `CAV-FULL` at commit `76b4799`.

Use this compiler branch with that benchmark branch unless a run explicitly
records a different provenance pair.

## Differences From Upstream Souffle
- Probabilistic semantics: derivation graph construction, pruning, forward
  compilation, and weighted model counting.
- Optional artifact rewrite dispatcher (`--rewrite`) for full-mode runs.
- Deterministic-first derivation gating (`--det-opt`) to skip recording
  derivations for deterministic relations.
- Debug/profiling outputs are available for implementation diagnosis, but are
  not required for standard artifact reproduction.
