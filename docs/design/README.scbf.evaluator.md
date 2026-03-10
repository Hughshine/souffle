# SCBF Evaluator Prototype Design (Independent)

## Status
- Independent prototype evaluator.
- Not connected to runtime `--scbf` pipeline yet.
- Evaluates SCBF strata/targets with a standalone fixed-point algorithm.

## Scope
- Input:
  - `DerivationGraphViewInterface`,
  - `ScbfProgram` / `ScbfFormulaArenaPlan`,
  - `ScbfStratumFormulaBundle`.
- Output:
  - per-target probability estimates,
  - export table for cross-stratum imports,
  - convergence/import diagnostics.

## File layout
- API:
  [src/include/souffle/problog/scbf/ScbfEvaluator.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfEvaluator.h)
- Implementation + smoke main (guarded):
  [src/problog/scbf/ScbfEvaluator.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfEvaluator.cpp)

## Core interfaces
1. `ScbfEvaluatorConfig`
- controls iteration budget and convergence threshold:
  - `maxIterations`
  - `epsilon`
  - `failOnMissingImports`
  - `clampToUnitInterval`

2. `ScbfEvaluatorResult`
- captures:
  - `nodeProbabilities` (target outputs),
  - `exportProbabilities` (cross-stratum export table),
  - per-target iteration/convergence stats,
  - aggregated missing-import count.

3. `evaluateScbfProgramProbability(...)`
- processes strata in topo order.
- for each target:
  - builds local fixed-point over stratum edges,
  - reads earlier-stratum imports from export table,
  - writes target probability back into export table.

4. `evaluateScbfProgramProbabilityViaFormulaBundle(...)`
- processes strata in topo order.
- builds one `ScbfStratumFormulaBundle` per cycle.
- optionally rewrites each bundle before evaluation when
  `ScbfEvaluatorConfig.rewriteBeforeEvaluate=true`.
- evaluates target-local equations directly from formula IR literals/rules.
- uses only earlier-stratum exports for imports.

## Semantics (prototype)
- This evaluator uses a noisy-or probability composition for local derivations:
  - edge probability = edge weight × product(body literal probabilities),
  - node probability = noisy-or over incoming edge probabilities.
- Facts are treated as fixed probabilities.
- Negated literals use `1 - p(input)`.

Notes:
- This is a prototype numerical evaluator for validating SCBF execution shape.
- It is not yet declared equivalent to existing exact DD/WMC semantics.
- For formula+rewrite path, current graph->formula reconstruction can still
  introduce semantic drift on some cases; smoke currently records this delta.

## Isolation strategy
- No runtime integration in `Pipeline.cpp`.
- Standalone executable target:
  - `souffle-scbf-evaluator-smoke`
- The smoke target bundles SCBF IR implementation in one translation unit to avoid
  ODR conflicts from current `DerivationGraph.h` layout.

## Smoke checks
- Constructs a toy graph with one recursive SCC feeding an output SCC.
- Builds SCBF IR and runs:
  - planner-based evaluator path,
  - formula-bundle-based evaluator path.
- If CUDD is available, also runs formula-bundle + graph-aligned rewrite path.
- Compares both against a whole-graph baseline fixed-point solver in the same
  file, and also checks both SCBF paths match each other.
- Fails if difference exceeds tight tolerance.

## How to run
1. `cmake -S . -B build`
2. `cmake --build build --target souffle-scbf-evaluator-smoke -j$(nproc || sysctl -n hw.ncpu || echo 2)`
3. `./build/src/souffle-scbf-evaluator-smoke`

## Next steps
1. Add deterministic/exact-mode evaluator over SCBF target plans.
2. Add regression fixtures for import-missing/fail-fast behavior.
3. Evaluate how to package SCBF IR and evaluator without translation-unit bundling.
4. Only after equivalence checks, bridge to `--scbf` runtime path.

## Source references
- [src/include/souffle/problog/scbf/ScbfEvaluator.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfEvaluator.h)
- [src/problog/scbf/ScbfEvaluator.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfEvaluator.cpp)
- [src/include/souffle/problog/scbf/ScbfIr.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfIr.h)
- [src/problog/scbf/ScbfIr.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfIr.cpp)
- [src/CMakeLists.txt](/home/hugh/research/datalog/souffle/src/CMakeLists.txt)

## Related commits
- `UNCOMMITTED` — design(scbf-eval): add independent SCBF evaluator prototype and standalone smoke
