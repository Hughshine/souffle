# SCBF IR Module Design (Independent Prototype)

## Status
- Independent prototype module.
- Not wired into default full/inc runtime pipeline.
- Intended for isolated design iteration and smoke testing.

## Goals
- Provide a first-class `SCBF` data structure, instead of encoding behavior only
  as execution policy flags.
- Keep implementation isolated from old pipeline code paths.
- Enable quick semantic experiments on:
  - stratum planning,
  - boundary/import extraction,
  - per-target formula-arena planning.

## Non-goals (current prototype)
- No runtime FC/WMC execution on top of this IR yet.
- No evidence-conditioned SCBF evaluation path.
- No rewrite integration on SCBF IR yet.

## Module layout
- API/IR header:
  [src/include/souffle/problog/scbf/ScbfIr.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfIr.h)
- Builder implementation:
  [src/problog/scbf/ScbfIr.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfIr.cpp)
- Standalone smoke executable:
  - source: `src/problog/scbf/ScbfIr.cpp` (guarded by `SOUFFLE_SCBF_IR_SMOKE_MAIN`)
  - target: `souffle-scbf-ir-smoke`

## Core data structures
1. `ScbfProgram`
- Global IR container.
- Stores:
  - topological SCC order (`topoOrderCycleIds`),
  - per-cycle stratum records (`strata`),
  - index maps (`cycleIdToTopoIndex`, `nodeToCycleId`, `edgeToCycleId`).

2. `ScbfStratum`
- One SCC-level stratum.
- Stores:
  - local nodes/edges,
  - `boundaryInNodes` (dependencies from earlier strata),
  - `boundaryOutNodes` (values exported to later strata),
  - `outputNodes`,
  - dependency/reverse-dependency cycle ids.

3. `ScbfFormulaArenaPlan`
- Per-stratum planning artifact for formula construction.
- Enforces policy:
  - `allowCrossTargetSharing = false` (same-stratum cross-target sharing disabled),
  - each target has an explicit `imports` list from earlier strata.

## Builder APIs
1. `buildScbfProgram(view)`
- Builds SCC-topological strata from `CycleDependencyGraph`.
- Computes local/boundary/output partitions.

2. `buildScbfFormulaArenaPlan(view, program, cycleId)`
- Builds per-target planning for one stratum.
- Distinguishes:
  - local incoming edges (same cycle),
  - imported nodes from predecessor strata.

3. `validateScbfProgram(view, program, err)`
- Checks mapping/topology invariants.

4. `summarizeScbfProgram(program)`
- Produces compact summary counters for diagnostics.

## Invariants
1. Topology
- For every stratum dependency `dep -> cur`, `topo(dep) < topo(cur)`.

2. Ownership
- Each local node/edge has exactly one owner cycle id.

3. Boundary semantics
- `boundaryInNodes` are non-local inputs from predecessor strata.
- `boundaryOutNodes` are local nodes consumed by successor strata.

4. Sharing policy (planning level)
- Cross-target sharing inside one stratum is disallowed by plan contract.
- Earlier-stratum exports are allowed via `ScbfExportRef`.

## Why independent files
- Avoid unintended regressions in default FC/WMC pipeline while the IR design is
  still evolving.
- Allow isolated compile/run iteration on SCBF semantics.
- Keep migration path explicit: old runtime stays source-of-truth until SCBF IR
  execution path is verified.

## Standalone smoke test
- CMake target: `souffle-scbf-ir-smoke`
- Smoke builds a toy derivation graph with one recursive SCC and one downstream
  output SCC, then checks:
  - SCBF program validity,
  - multi-stratum split,
  - downstream target imports from prior stratum,
  - cross-target sharing disabled flag.

## Integration plan (next)
1. Add SCBF evaluator over `ScbfFormulaArenaPlan` without touching old runtime path.
2. Compare evaluator output against old full exact pipeline on small regression cases.
3. Only after equivalence validation, add opt-in runtime bridge (`--scbf`) to IR evaluator.

## Source references
- [src/include/souffle/problog/scbf/ScbfIr.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/scbf/ScbfIr.h)
- [src/problog/scbf/ScbfIr.cpp](/home/hugh/research/datalog/souffle/src/problog/scbf/ScbfIr.cpp)
- [src/CMakeLists.txt](/home/hugh/research/datalog/souffle/src/CMakeLists.txt)
- [src/include/souffle/problog/DerivationGraph.h](/home/hugh/research/datalog/souffle/src/include/souffle/problog/DerivationGraph.h)

## Related commits
- `UNCOMMITTED` — design(scbf-ir): add independent SCBF IR module and standalone smoke target
