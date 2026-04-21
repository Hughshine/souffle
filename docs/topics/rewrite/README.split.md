# Derivation Graph Split

## Source references
- [../../../src/include/souffle/problog/GraphRewriter.h](../../../src/include/souffle/problog/GraphRewriter.h)
- [../../../src/include/souffle/problog/GraphAnalyzer.h](../../../src/include/souffle/problog/GraphAnalyzer.h)
- [README.rewrite.impl.md](README.rewrite.impl.md)

## Status
- Internal implementation note for the rewrite dispatcher.
- AE commands do not expose split policy flags.

## Current Behavior
- The artifact command uses bare `--rewrite`.
- The dispatcher selects the split policy internally:
  - probabilistic rule weights: implicit split with local split policy
  - no probabilistic rule weights: graph rewrite with no split
- Split duplicates eligible input fact nodes to expose independent downstream
  structure.  Facts with evidence or `needOutput` are skipped, and facts in
  evidence-affected components are excluded.
- Budget controls are active:
  `splitMaxNewNodesPerPass`, `splitMaxNewEdgesPerPass`,
  `splitMaxGroupsPerNode`, `splitMinGroupEdges`.

## Design Goal
Split tries to separate downstream reasoning that is independent at the random
variable level, so rewrite can detect more SISO regions and forward compilation
can exploit smaller components.

## Historical Note
Older manual split-policy ablations were used during development.  They are not
part of the AE command surface and should not be cited as artifact reproduction
steps.

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
