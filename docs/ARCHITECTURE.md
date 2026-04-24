# Architecture

This artifact evaluates probabilistic Datalog programs under incremental
updates. The evaluator supplies a `.dl` program, a fact directory, and a stream
of online updates. The compiler produces an online binary. That binary computes
the baseline, applies incremental turns, and writes output tuple probabilities.

The artifact-facing comparison is `full-hard` versus incremental modes
(`inc-naive`, `inc-regional`, and staged mixed modes). All compared runs should
produce the same tuple keys and probabilities.

The two implementation contributions are:

1. Incremental derivation maintenance for mixed insert/delete turns.
2. Incremental forward compilation with naive and regional update paths.

## 0. Compile

`souffle` first compiles the input `.dl` program through the normal AST and RAM
pipeline, then emits an online binary with incremental relation metadata and CLI
controls.

Key sources:

- [../src/MainDriver.cpp](../src/MainDriver.cpp)
- [../src/synthesiser/Synthesiser.cpp](../src/synthesiser/Synthesiser.cpp)
- [../src/include/souffle/CompiledOptions.h](../src/include/souffle/CompiledOptions.h)

## 1. Baseline Turn

The generated binary reads `<relation>.facts` and optional `<relation>.prob`
files, runs the compiled semi-naive evaluator, records rule applications, and
builds the baseline derivation graph. This gives the persistent state consumed
by later turns.

Key sources:

- [../src/synthesiser/Synthesiser.cpp](../src/synthesiser/Synthesiser.cpp)
- [../src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp)
- [../src/include/souffle/problog/DerivationGraph.h](../src/include/souffle/problog/DerivationGraph.h)

## 2. Commit a Delta Turn

Each `commit` applies queued insertions and deletions. The runtime updates the
derivation graph view, performs delete/rederive where needed, and maintains the
`@post_delete_*` snapshot family required for exact mixed-update semantics in
non-recursive upper strata.

Key sources:

- [../src/include/souffle/cli/Cli.h](../src/include/souffle/cli/Cli.h)
- [../src/include/souffle/problog/DerivationGraph.h](../src/include/souffle/problog/DerivationGraph.h)
- [../src/ast2ram/online/UnitTranslator.cpp](../src/ast2ram/online/UnitTranslator.cpp)

## 3. Incremental Forward Compilation

After the graph delta is ready, the runtime solves probabilities in one of two
incremental forward-compilation modes:

- `inc-naive`: rebuild on the delta-reachable scope.
- `inc-regional`: reuse unaffected outside-of-region formulas and rebuild only
  the selected regional subgraph.

`full-hard` remains the exact oracle and resets the formula state each turn.
`full-soft` keeps the formula manager but still recomputes from full semantics.

Key sources:

- [../src/include/souffle/problog/ForwardCompilation.h](../src/include/souffle/problog/ForwardCompilation.h)
- [../src/include/souffle/problog/RegionalIncremental.h](../src/include/souffle/problog/RegionalIncremental.h)

## 4. Multi-Turn Regional State Machine

Regional reuse is not always safe to compose across turns. The runtime
classifies the persistent forward-compilation state as normalized versus
regionalized. When a requested regional consumer would read unsafe incoming
state, only the FC side falls back to `inc-naive`; semantic mode is preserved.

This is the maintained multi-turn regional guard used by the artifact branch.

Key sources:

- [../src/include/souffle/CompiledOptions.h](../src/include/souffle/CompiledOptions.h)
- [../src/include/souffle/cli/Cli.h](../src/include/souffle/cli/Cli.h)

## 5. Output and Logs

The runtime writes output tuple probabilities to `facts.prob` and per-turn
snapshots such as `fact-iter2-inc-regional.prob`. JSON logs and graph statistics
are controlled by `--logfile` and dump/profile flags.

Key sources:

- [../src/include/souffle/problog/DerivationGraph.h](../src/include/souffle/problog/DerivationGraph.h)
- [../src/problog/debug/Debugger.cpp](../src/problog/debug/Debugger.cpp)
