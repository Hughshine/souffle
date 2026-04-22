# Architecture

## Source References
- [src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp)
- [src/include/souffle/problog/DerivationGraph.h](../src/include/souffle/problog/DerivationGraph.h)
- [src/include/souffle/problog/ForwardCompilation.h](../src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/problog/GraphRewriter.h](../src/include/souffle/problog/GraphRewriter.h)
- [src/include/souffle/CompiledOptions.h](../src/include/souffle/CompiledOptions.h)

## Overview
This AE branch packages the full-mode probabilistic pipeline for generated
benchmark binaries.

## High-Level Flow
1. Parse Datalog into AST and lower it to RAM.
2. Synthesize C++ for compiled programs and link against the precompiled runtime.
3. Full-mode probabilistic runs build a derivation graph.
4. The graph is pruned against output/evidence requirements.
5. Optional bare `--rewrite` dispatches to the selected rewrite implementation.
6. Component-wise forward compilation and BDD weighted model counting compute
   output probabilities.

## Key Components
- `src/souffle.cpp` and `src/MainDriver.cpp`: compiler entry point and driver glue.
- `src/include/souffle/problog/`: derivation graph, pipeline, rewrite, and
  probabilistic evaluation.
- `src/synthesiser/Synthesiser.cpp`: emits generated C++ for compiled programs.
- `src/include/souffle/CompiledOptions.h`: generated-program runtime options.

## Data and Artifacts
- Input facts: `-F <dir>` with `<rel>.facts` and optional `<rel>.prob`.
- Output probabilities: full runs write `facts.prob` to the output directory.
- Logs: `--logfile <name>` writes JSON reports into the output directory.

## Dependencies and Constraints
- The artifact uses CUDD for BDD weighted model counting.
- Runtime and testing constraints are documented in [docs/USAGE.md](USAGE.md)
  and [docs/TESTING.md](TESTING.md).

## Further Reading
See [docs/INDEX.md](INDEX.md) for the complete AE doc map.
