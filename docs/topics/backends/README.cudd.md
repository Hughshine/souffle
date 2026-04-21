# CUDD BDD Backend Notes

## Source references
- [../../../src/include/souffle/problog/formula/CuddManager.h](../../../src/include/souffle/problog/formula/CuddManager.h)
- [../../../src/include/souffle/problog/ForwardCompilation.h](../../../src/include/souffle/problog/ForwardCompilation.h)
- [../../../src/include/souffle/CompiledOptions.h](../../../src/include/souffle/CompiledOptions.h)
- [../../../src/problog/Pipeline.cpp](../../../src/problog/Pipeline.cpp)

## Scope
- CUDD-backed BDD is the artifact backend.
- This is not a general CUDD tutorial.

## Key Code Locations
- `WeightedBDDManager` in `CuddManager.h` owns the `DdManager` and implements
  BDD operations plus weighted model counting.
- `Pipeline.cpp` estimates BDD variable count and builds CUDD initialization
  parameters.
- `ForwardCompilation.h` builds formulas over derivation graph components.

## Manager Initialization
The manager is created with:
`Cudd_Init(numVars, numVarsZ, numSlots, cacheSize, maxMemory)`.

Current initialization is code-driven.  The pipeline computes a configuration
from the estimated BDD variable count and passes it to `WeightedBDDManager`.

## Common Failure Modes
- `Failed to initialize CUDD manager`: missing or incorrectly linked CUDD.
- Performance cliffs:
  - manager growth from insufficient variable-count headroom
  - cache pressure on large components
  - expensive dynamic reordering on difficult components

## Diagnostics
`--fc-profile` and `--profile-wmc` expose backend timing, but they are hidden
diagnostic flags and should not be used in standard AE timing commands.

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
