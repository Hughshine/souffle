# SISO Region Detection

## Source references
- [../../../src/include/souffle/problog/GraphAnalyzer.h](../../../src/include/souffle/problog/GraphAnalyzer.h)
- [README.rewrite.impl.md](README.rewrite.impl.md)

## Scope
This note covers the fast-path SISO detectors used by graph rewrite.

## Active Patterns
- Single hyperedge
- Linear two-edge chain
- Parallel edges
- Fan-out converge
- All-facts single hyperedge

Detected regions are deduplicated before graph rewrite applies reductions.

## Artifact Reading
SISO detection is an implementation detail behind bare `--rewrite`.  AE runs
should rely on dispatcher metadata and `facts.prob` consistency, not standalone
SISO debug logs.

## Legacy Detector
A dominance-based detector remains in `GraphAnalyzer` for reference but is not
the artifact path.

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
