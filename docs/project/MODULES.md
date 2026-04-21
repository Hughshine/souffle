# Module Map

This map is scoped to the full-mode probabilistic artifact in this AE branch.

| Module | Key paths | Responsibility | Primary docs |
| --- | --- | --- | --- |
| Driver and compiler entry | `src/souffle.cpp`, `src/MainDriver.cpp` | CLI entry, parse/build flow, generated binary synthesis | `docs/USAGE.md`, `docs/ARCHITECTURE.md` |
| Parser and AST front-end | `src/parser/*`, `src/ast/*` | Parse Datalog plus probabilistic/evidence syntax | `docs/project/PROBLOG_EXTENSION_STACK.md` |
| RAM IR and transforms | `src/ram/*`, `src/ast2ram/*` | Lowering and runtime algebra used by generated programs | `docs/ARCHITECTURE.md` |
| C++ synthesis | `src/synthesiser/*` | Emit generated C++ and wire compiled runtime options | `docs/USAGE.md` |
| Probabilistic pipeline | `src/problog/*`, `src/include/souffle/problog/*` | Derivation graph, pruning, rewrite, FC/WMC | `docs/topics/rewrite/README.rewrite.impl.md` |
| Knowledge backend | `src/include/souffle/problog/formula/*` | BDD manager and formula operations used by FC/WMC | `docs/topics/backends/README.cudd.md` |
| Reporting and debug output | `src/reports/*`, `src/problog/debug/*` | JSON/DOT/stats dumps and runtime logs | `docs/topics/runtime/README.dump.md` |
| Tests | `tests/regression/*`, `src/tests/*` | Maintained regression checks and runtime/unit helpers | `docs/TESTING.md` |

## Source references
- [../../src/MainDriver.cpp](../../src/MainDriver.cpp)
- [../../src/problog/Pipeline.cpp](../../src/problog/Pipeline.cpp)
- [../../src/include/souffle/problog/Pipeline.h](../../src/include/souffle/problog/Pipeline.h)
- [../../src/include/souffle/CompiledOptions.h](../../src/include/souffle/CompiledOptions.h)

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
