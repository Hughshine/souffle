# CUDD Reordering

## Source references
- [../../../src/include/souffle/problog/formula/CuddManager.h](../../../src/include/souffle/problog/formula/CuddManager.h)
- [../../../src/include/souffle/problog/ForwardCompilation.h](../../../src/include/souffle/problog/ForwardCompilation.h)
- [../../../src/problog/Pipeline.cpp](../../../src/problog/Pipeline.cpp)

## Scope
This note covers dynamic variable reordering in the full-mode BDD path.

## Manager Initialization
`WeightedBDDManager::initManager()` calls `Cudd_Init(...)`.  The config is built
in `makeCuddInitConfig()` using the estimated BDD variable count.

## Dynamic Reordering
Dynamic reordering is enabled by `adaptiveReorder(...)`.  The heuristic is
chosen from the current BDD node count and may be disabled on very large graphs.

## Profiling
Hidden diagnostic flags can expose:
- `CUDD_PRECONFIG`
- `CUDD_CREATEVAR`
- `CUDD_CREATEVAR_STATS`
- reordering runtime deltas

These diagnostics are useful for local tuning but are not part of artifact
timing commands.

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
