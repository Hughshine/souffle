# Rewrite Pipeline

## Source references
- [../../../src/problog/Pipeline.cpp](../../../src/problog/Pipeline.cpp)
- [../../../src/include/souffle/problog/GraphRewriter.h](../../../src/include/souffle/problog/GraphRewriter.h)
- [../../../src/include/souffle/problog/ImplicitSplitRewrite.h](../../../src/include/souffle/problog/ImplicitSplitRewrite.h)
- [../../../src/include/souffle/problog/ForwardCompilation.h](../../../src/include/souffle/problog/ForwardCompilation.h)
- [../../USAGE.md](../../USAGE.md)

## Artifact Contract
- Full-mode runs do not rewrite unless `--rewrite` is passed.
- Artifact optimized runs use bare `--rewrite` with `--det-opt`.
- The generated binary chooses the rewrite implementation internally; AE
  commands do not pass explicit rewrite or split policy flags.
- BDD is the artifact backend and generated-program default.

## Smart Dispatch
`--rewrite` calls the dispatcher in [Pipeline.cpp](../../../src/problog/Pipeline.cpp).

The dispatcher records stable metadata:
- `rewrite_strategy`: `default`.
- `rewrite_impl`: `implicit_split` or `graph_rewrite`.
- `rewrite_reason`: why the implementation was selected.
- `rewrite_split_policy`: `local` or `none`.

Selection rule:
- If any rule has a non-`1.0` probability, use `implicit_split` with the local
  split policy.
- If no rule has a probabilistic weight, use `graph_rewrite` with no split.

The rule is based on rule probabilities only.  Probabilistic input facts in
`.prob` files do not by themselves select implicit split.

## Implementations
- `implicit_split`: rewrites in an overlay view, commits safe overlay changes to
  the graph, carries precomputed tuple probabilities, and then hands the graph
  to the shared component-wise FC/WMC machinery.
- `graph_rewrite`: detects local SISO regions in the derivation graph,
  summarizes each accepted region, applies the reduced graph view, and then uses
  the same component-wise FC/WMC machinery.
- Component-wise FC/WMC uses fast paths for eligible simple components and BDD
  managers for slow components.

## Diagnostics
Dump/profile/tuning flags are for local diagnosis and should not appear in AE
benchmark commands.  `--derv-only --rewrite` does not run the rewrite pipeline
and should not be used as a graph-only rewrite benchmark.

## Correctness Policy
- Side-channel and taint benchmark outputs should match exactly.
- Symbolization outputs should have the same key set and may differ by at most
  `1e-8` in probability.  The tolerance is for rare final-digit output-rounding
  boundaries observed in a small number of cases.
