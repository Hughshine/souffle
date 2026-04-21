# ProbLog Extension Stack

This document is the implementation-level map for the full-mode probabilistic
artifact, from parsing to runtime probability evaluation.

## Source references
- [../../src/MainDriver.cpp](../../src/MainDriver.cpp)
- [../../src/parser/parser.yy](../../src/parser/parser.yy)
- [../../src/synthesiser/Synthesiser.cpp](../../src/synthesiser/Synthesiser.cpp)
- [../../src/problog/Pipeline.cpp](../../src/problog/Pipeline.cpp)
- [../../src/include/souffle/problog/DerivationGraph.h](../../src/include/souffle/problog/DerivationGraph.h)
- [../../src/include/souffle/problog/ForwardCompilation.h](../../src/include/souffle/problog/ForwardCompilation.h)

## End-to-End Flow
1. `src/MainDriver.cpp`: parse compiler CLI and synthesize generated C++.
2. `src/parser/parser.yy` and `src/parser/ParserDriver.cpp`: parse `query`,
   `evidence`, probabilistic facts, and probabilistic heads.
3. `src/ast/*`: store probabilistic metadata in `ast::Program` and run semantic
   checks.
4. `src/ram/*`: hold RAM-level probabilistic metadata and derivation recording.
5. `src/synthesiser/Synthesiser.cpp`: generate runtime C++ glue, build
   `RuleManager`/`QueryManager`, and wire `runPipeline(...)`.
6. `src/problog/Pipeline.cpp`: build/prune/rewrite the derivation graph, then
   run FC/WMC.

## Runtime Pipeline
The generated binary performs:
1. load facts and aligned probabilities
2. execute the Datalog program and record rule applications
3. create the full derivation graph
4. prune the graph
5. optionally run bare-`--rewrite` dispatch
6. compute output probabilities with component-wise FC/WMC
7. write `facts.prob`

## Rewrite Dispatcher
Bare `--rewrite` is the only AE rewrite entry:
- probabilistic rule weights select `implicit_split`
- deterministic rules select graph rewrite with no split

Diagnostic controls are intentionally not part of the generated help page or AE
commands.

## Aggregate Support
Aggregate contributions are represented in the derivation graph so probabilistic
dependencies are preserved through full-mode FC/WMC.  The maintained regression
suite includes aggregate round-trip checks.

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
