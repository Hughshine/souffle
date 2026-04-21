# Maintenance Notes

This file records invariants most likely to cause AE regressions if violated.

## Critical Invariants
- Artifact runs use full-mode generated binaries.
- Bare `--rewrite` is the only AE rewrite flag; implementation selection is
  internal to the dispatcher.
- `--det-opt` is part of the optimized and plain artifact command lines.
- `.prob` files are line-aligned with `.facts`; mismatches corrupt semantics.
- Symbolization comparison accepts only identical output keys and probability
  differences within `1e-8`.

## High-Risk Modules and What To Check

| Module | Risk | Minimum verification |
| --- | --- | --- |
| `src/problog/Pipeline.cpp` | Wrong dispatch or stage ordering changes artifact results | Build + regression + one CAV-FULL plain/rewrite case |
| `src/include/souffle/problog/DerivationGraph.h` | Incorrect prune/aggregate handling biases probabilities | Build + aggregate regression + symbolization smoke |
| `src/include/souffle/problog/ForwardCompilation.h` | FC/WMC correctness or performance regressions | Build + regression + inspect FC/WMC stage logs |
| `src/include/souffle/problog/GraphRewriter.h` and `src/problog/ImplicitSplitRewrite.cpp` | Rewrite correctness or overhead regressions | Regression plus side-channel/taint spot checks |
| `src/include/souffle/CompiledOptions.h` and `docs/USAGE.md` | Option docs drift from generated binary behavior | Verify `./compute -h` and update docs in the same change |

## Documentation Rule For Code Changes
- Behavior change in runtime flags: update `docs/USAGE.md`,
  `docs/topics/runtime/README.flag.md`, and `docs/INDEX.md`.
- Pipeline or algorithm change: update relevant `docs/topics/*` plus this file
  if invariants changed.
- Workflow change: update `README.md`, `docs/TESTING.md`, and `docs/RUNBOOK.md`.

## Source references
- [../../src/problog/Pipeline.cpp](../../src/problog/Pipeline.cpp)
- [../../src/include/souffle/CompiledOptions.h](../../src/include/souffle/CompiledOptions.h)
- [../../src/include/souffle/problog/DerivationGraph.h](../../src/include/souffle/problog/DerivationGraph.h)
- [../TESTING.md](../TESTING.md)

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
