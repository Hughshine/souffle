# Module Map (Whole Project)

This map is organized by code responsibility, not by experiment workflow.
`Type` indicates whether the area is mostly upstream Souffle, fork-specific,
or an upstream area extended by this fork.

| Module | Key paths | Type | Responsibility | Primary docs |
| --- | --- | --- | --- | --- |
| Driver and compiler entry | `src/souffle.cpp`<br>`src/MainDriver.cpp` | Upstream + fork extension | CLI entry, parse/build flow, default online behavior, compile/run mode gating | `docs/USAGE.md`<br>`docs/ARCHITECTURE.md`<br>`docs/project/PROBLOG_EXTENSION_STACK.md` |
| Parser and AST front-end | `src/parser/*`<br>`src/ast/*` | Upstream + fork extension | Parse Datalog, build AST, include probabilistic/evidence AST nodes | `docs/ARCHITECTURE.md`<br>`docs/project/PROBLOG_EXTENSION_STACK.md` |
| AST analyses and transforms | `src/ast/analysis/*`<br>`src/ast/transform/*` | Mostly upstream + fork touches | Semantic checks and rewrites before RAM lowering | `docs/ARCHITECTURE.md` |
| AST-to-RAM lowering (core) | `src/ast2ram/*` | Mostly upstream | Lower AST to RAM and apply translation strategy | `docs/topics/pipeline/README.souffle.opt.md` |
| AST-to-RAM lowering (online/incremental) | `src/ast2ram/online/*` | Fork extension | Generate incremental `_inc` strata and delta relations | `docs/topics/pipeline/README.dred.md`<br>`docs/project/PROBLOG_EXTENSION_STACK.md` |
| RAM IR and RAM transforms | `src/ram/*` | Mostly upstream + fork extension | Runtime algebra, incremental operators, execution plan IR | `docs/ARCHITECTURE.md`<br>`docs/project/PROBLOG_EXTENSION_STACK.md` |
| C++ synthesis and runtime glue | `src/synthesiser/*` | Mostly upstream + fork touches | Emit generated C++ and wire compiled runtime options | `docs/USAGE.md`<br>`docs/project/PROBLOG_EXTENSION_STACK.md` |
| Probabilistic pipeline | `src/problog/*`<br>`src/include/souffle/problog/*` | Fork extension | Derivation graph, prune, rewrite, FC/WMC, incremental/regional logic | `docs/topics/pipeline/README.inc.region.md`<br>`docs/topics/rewrite/README.rewrite.impl.md`<br>`docs/project/PROBLOG_EXTENSION_STACK.md` |
| Knowledge backends | `src/include/souffle/problog/formula/*` | Fork extension | BDD/SDD manager and formula operations used by FC/WMC | `docs/topics/backends/README.cudd.md`<br>`docs/topics/backends/README.sdd.md` |
| Incremental command interface | `src/include/souffle/cli/Cli.h` | Fork extension | Interactive/batch `insert/delete/commit`, mode switching, output naming | `docs/USAGE.md`<br>`docs/topics/pipeline/README.inc.region.md`<br>`docs/project/PROBLOG_EXTENSION_STACK.md` |
| Reporting and debug output | `src/reports/*`<br>`src/problog/debug/*` | Upstream + fork extension | JSON/DOT/stats dumps, runtime logs, diagnostics | `docs/topics/runtime/README.dump.md` |
| Tests and validation artifacts | `tests/regression/*`<br>`src/tests/*` | Mixed (regression + unit) | Maintained regression checks and runtime/unit validation helpers | `docs/TESTING.md` |
| Benchmarks and experiment tooling | `problog-benchmark/*`<br>`examples/*` | Fork extension | Reproducible benchmark generation, run orchestration, artifact collection | `docs/topics/evaluation/README.md`<br>`archive/README.md` |

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/ast2ram/online/UnitTranslator.cpp](src/ast2ram/online/UnitTranslator.cpp)
- [src/include/souffle/problog/Pipeline.h](src/include/souffle/problog/Pipeline.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)

## Related commits
- `UNCOMMITTED` — docs(project): add whole-project module inventory with ownership classification
- `UNCOMMITTED` — docs(project): wire ProbLog extension stack doc into module map
