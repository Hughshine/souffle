# Fork Delta and Contributions

This document explains what this fork contributes beyond baseline Souffle.

## Likely Fork-Owned Areas

| Area | Key files | Contribution | Notes |
| --- | --- | --- | --- |
| Probabilistic semantics | `src/problog/Pipeline.cpp`<br>`src/include/souffle/problog/Pipeline.h` | Adds probabilistic execution pipeline on top of compiled Souffle runs | Central fork capability |
| Derivation graph and pruning | `src/include/souffle/problog/DerivationGraph.h` | Adds derivation graph construction, evidence/query handling, prune flow | Core data structure for full/inc modes |
| Forward compilation + WMC | `src/include/souffle/problog/ForwardCompilation.h`<br>`src/include/souffle/problog/formula/*` | Adds BDD/SDD-based formula construction and weighted model counting | Performance-sensitive, large impact on runtime |
| Rewrite/split pipeline | `src/include/souffle/problog/GraphRewriter.h`<br>`src/include/souffle/problog/GraphAnalyzer.h` | Adds graph-level rewrite/split passes for full mode | Must remain consistent with FC assumptions |
| Incremental regional update | `src/include/souffle/problog/RegionalIncremental.h`<br>`src/include/souffle/problog/IncRegionAnalyzer.h` | Adds `inc-regional` analysis/planning/rebuild pipeline | Most complex incremental extension |
| Online incremental translation | `src/ast2ram/online/*` | Adds `_inc` strata and delta-oriented lowering strategy | Connects compiler layer to incremental runtime semantics |
| Incremental CLI control plane | `src/include/souffle/cli/Cli.h` | Adds `insert/delete/commit`, `setmode`, per-turn outputs and instrumentation control | User-facing interface for incremental workflow |
| Runtime option surface | `src/include/souffle/CompiledOptions.h`<br>`src/MainDriver.cpp` | Adds fork-specific flags (`--det-opt`, `--inc-profile`, `--fc-profile`, etc.) and default behavior | Keep docs synchronized with code defaults |
| Benchmark and artifact tooling | `problog-benchmark/side_channel_inc.py`<br>`problog-benchmark/side_channel_full.py` | Adds reproducible evaluation pipelines and TSV/log collectors | Keep outputs archived under `archive/` |

## Upstream-Core Areas (with local touches)
- `src/parser/*`, `src/ast/*`, `src/ram/*`, `src/synthesiser/*`
- These remain structurally upstream modules, but this fork introduces probabilistic and incremental hooks in several points.

## Caveat
Ownership labels are based on code structure and behavior in this repository.
When rebasing onto upstream Souffle, confirm exact diffs with branch comparison.

## Source references
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)
- [src/include/souffle/problog/ForwardCompilation.h](src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/problog/RegionalIncremental.h](src/include/souffle/problog/RegionalIncremental.h)
- [src/ast2ram/online/TranslationStrategy.cpp](src/ast2ram/online/TranslationStrategy.cpp)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)

## Related commits
- `UNCOMMITTED` — docs(project): document fork-specific module contributions and boundaries
