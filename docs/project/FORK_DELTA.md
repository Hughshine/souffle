# Fork Delta and Contributions

This document explains what this AE branch contributes beyond baseline Souffle.

## Full-Mode Fork-Owned Areas

| Area | Key files | Contribution |
| --- | --- | --- |
| Probabilistic semantics | `src/problog/Pipeline.cpp`, `src/include/souffle/problog/Pipeline.h` | Full-mode probability execution pipeline |
| Derivation graph and pruning | `src/include/souffle/problog/DerivationGraph.h` | Graph construction, evidence/query handling, pruning |
| Forward compilation + WMC | `src/include/souffle/problog/ForwardCompilation.h`, `src/include/souffle/problog/formula/*` | BDD formula construction and weighted model counting |
| Rewrite pipeline | `src/include/souffle/problog/GraphRewriter.h`, `src/include/souffle/problog/GraphAnalyzer.h`, `src/problog/ImplicitSplitRewrite.cpp` | Graph and implicit-split rewrite before FC/WMC |
| Runtime option surface | `src/include/souffle/CompiledOptions.h`, `src/MainDriver.cpp` | Stable generated-program flags: `--det-opt`, `--rewrite`, `--logfile`, `-F`, `-D` |

## Source references
- [../../src/problog/Pipeline.cpp](../../src/problog/Pipeline.cpp)
- [../../src/include/souffle/problog/DerivationGraph.h](../../src/include/souffle/problog/DerivationGraph.h)
- [../../src/include/souffle/problog/ForwardCompilation.h](../../src/include/souffle/problog/ForwardCompilation.h)
- [../../src/include/souffle/CompiledOptions.h](../../src/include/souffle/CompiledOptions.h)

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
