# Dump and Debug Outputs Reference

## Source references
- [../../../src/MainDriver.cpp](../../../src/MainDriver.cpp)
- [../../../src/include/souffle/CompiledOptions.h](../../../src/include/souffle/CompiledOptions.h)
- [../../../src/problog/Pipeline.cpp](../../../src/problog/Pipeline.cpp)
- [../../../src/include/souffle/problog/DerivationGraph.h](../../../src/include/souffle/problog/DerivationGraph.h)
- [../../../src/include/souffle/problog/GraphRewriter.h](../../../src/include/souffle/problog/GraphRewriter.h)
- [../../../src/problog/debug/Debugger.cpp](../../../src/problog/debug/Debugger.cpp)
- [../../USAGE.md](../../USAGE.md)

## Scope
This document lists full-mode dump/debug outputs for generated `./compute`
binaries.  These flags are diagnostics, not standard AE commands.

## Common Outputs
- `facts.prob`: final output tuple probabilities.
- `<logfile>.json`: debugger/stage JSON report when `--logfile <name>` is
  passed.
- stdout timing lines: create graph, pruning, rewrite, FC, and WMC stages.

## Hidden Diagnostic Flags
- `--dumpdot`: write DOT graph snapshots such as `before_prune.dot` and
  `after_prune.dot`.
- `--dumpjson`: write derivation graph JSON snapshots.
- `--dumpstat`: write graph statistics.
- `--dumpconst`: write deterministic constant-analysis details.
- `--fc-profile`, `--profile-wmc`, `--profile-dep-graph`: print focused timing
  and backend diagnostics.

## Interpretation
- Dump/profile flags add overhead and should be kept out of artifact timing
  runs unless the run is explicitly diagnostic.
- Correctness comparisons should use `facts.prob`, not DOT/JSON dumps.

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
