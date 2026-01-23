# Architecture

## Source references
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)
- [src/include/souffle/problog/ForwardCompilation.h](src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)


## Overview
This fork extends upstream Souffle with a probabilistic pipeline and online incremental
execution. Online compilation is the default; there is no interpreter path.

## High-Level Flow
1. Parse Datalog into AST, lower to RAM, then apply the online translator.
2. Synthesize C++ for compiled programs and link against the precompiled runtime.
3. Full-mode probabilistic runs build a derivation graph, optionally rewrite it, and
   then perform forward compilation + weighted model counting.
4. Incremental runs use DRed-like delta relations with a turn-based CLI.

## Key Components
- `src/souffle.cpp` and `src/MainDriver.cpp`: CLI entry point and driver glue.
- `src/ast2ram/online/`: online translation for `_inc` strata and delta relations.
- `src/include/souffle/problog/`: derivation graph, pipeline, rewrite, and
  probabilistic evaluation.
- `src/include/souffle/cli/Cli.h`: interactive/batch CLI for online incremental runs.
- `src/synthesiser/Synthesiser.cpp`: emits generated C++ for compiled programs.

## Data and Artifacts
- Input facts: `-F <dir>` with `<rel>.facts` and optional `<rel>.prob`.
- Output probabilities:
  - Full runs write `facts.prob` to the output directory.
  - Incremental CLI runs write `fact-iter<N>-full.prob` and
    `fact-iter<N>-inc-{naive|regional}.prob`.
- Debug dumps: `--dumpjson`, `--dumpdot`, `--dumpstat` (written to output dir).
- Logs: `--logfile <name>` writes JSON reports into the output dir.

## Dependencies and Constraints
- CUDD is required for the BDD backend; SDD is optional for `-k sdd`.
- Runtime and testing constraints are documented in `docs/USAGE.md` and
  `docs/TESTING.md`.

## Further Reading
See `docs/INDEX.md` for the complete doc map, including rewrite and evaluation notes.

## Related commits
- `aaa18c137` — docs(repo): add core docs
