# Refactor Scope and Roadmap

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/problog_graph_query.cpp](src/problog_graph_query.cpp)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)
- [src/include/souffle/problog/ForwardCompilation.h](src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/problog/RegionalIncremental.h](src/include/souffle/problog/RegionalIncremental.h)
- [docs/project/PROBLOG_EXTENSION_STACK.md](docs/project/PROBLOG_EXTENSION_STACK.md)
- [docs/topics/rewrite/README.rewrite.impl.md](docs/topics/rewrite/README.rewrite.impl.md)
- [docs/topics/pipeline/README.dred.md](docs/topics/pipeline/README.dred.md)
- [docs/topics/pipeline/README.inc.region.md](docs/topics/pipeline/README.inc.region.md)
- [docs/topics/backends/README.cudd.md](docs/topics/backends/README.cudd.md)
- [docs/topics/backends/README.sdd.md](docs/topics/backends/README.sdd.md)
- [docs/design/README.approx.pipeline.md](docs/design/README.approx.pipeline.md)
- [docs/design/README.scbf.pipeline.md](docs/design/README.scbf.pipeline.md)
- [docs/TESTING.md](docs/TESTING.md)
- [docs/topics/testing/README.regression.md](docs/topics/testing/README.regression.md)
- [docs/topics/evaluation/README.eval.md](docs/topics/evaluation/README.eval.md)
- [docs/topics/evaluation/README.eval.inc.md](docs/topics/evaluation/README.eval.inc.md)
- [docs/research/README.protocol.md](docs/research/README.protocol.md)
- [research/README.md](research/README.md)
- [problog-benchmark/runs/README.md](problog-benchmark/runs/README.md)


This document defines the full scope of a behavior-preserving refactor for the
online probabilistic fork. It is a plan and acceptance contract, not an
implementation commit. The existing [README.refactor.md](docs/design/README.refactor.md)
remains the lower-level opportunity list; this file defines what the refactor
should include, why it should exist, and how it should be validated.

## Status
- Active design proposal.
- Intended to guide staged refactor work without changing semantics first.

## Scope
- Exact full-mode probabilistic runtime.
- Full-only rewrite and split machinery, including implicit rewrite variants.
- Experimental SCBF runtime lane.
- Incremental online runtime and CLI mode matrix.
- Offline graph-query and approximate counting tooling.
- Source naming and source-tree organization.
- Documentation naming and documentation-system cleanup.
- Validation surfaces: smoke, regression, benchmark, and provenance checks.
- Coordination constraint: repository-root `research/` and `docs/research/`
  are excluded from this refactor and treated as separate actively developed
  lanes.

## Current Capability Baseline To Preserve
- Full exact runtime:
  - derivation graph build, prune, forward compilation, and weighted model counting
  - BDD/CUDD as the primary exact backend
  - SDD as an optional backend with narrower support
  - `--det-opt` default-on behavior and `--det-force` short-circuit behavior
  - evidence support only in full-only mode
- Full rewrite family:
  - legacy `--rewrite`
  - split modes `no-split`, `naive-split`, `complete-split`
  - `--implicit-rewrite`
  - `--implicit-iterate-split-rewrite`
  - rewrite remains a full-mode-only feature; a full run that performs rewrite
    does not also continue into online incremental CLI in that same execution
- Experimental evaluators:
  - `--scbf` is an experimental runtime branch inside the full pipeline
  - Approx/AMC support currently exists as offline graph-query tooling, not as
    the main runtime backend
- Incremental runtime:
  - online DRed-style path is the maintained incremental base
  - supported staged modes include `inc-naive`, `inc-regional`, `full-hard`,
    `full-soft`, and mixed full/FC combinations already accepted by `setmode`
  - `elastic` remains parser-visible and currently degrades to `inc-naive`
    while the true elastic scheduler stays unimplemented
- Validation and benchmark surface:
  - example smoke run under `examples/running_example/`
  - maintained CTest regression suite under `tests/regression/`
  - full side-channel workflow
  - incremental side-channel workflow
  - taint chained benchmark workflow with strict provenance rules

## What Must Be Refactored
1. Capability boundaries.
   The current runtime mixes exact production behavior, experimental evaluators,
   and offline tooling in the same conceptual lane. Refactor scope includes
   separating exact runtime, experimental runtime, and offline query tooling
   into explicit modules and docs.
2. Runtime configuration and mode plumbing.
   String-based mode parsing and repeated interpretation across driver, compiled
   options, pipeline, and CLI must be consolidated into one typed contract.
3. Full exact pipeline orchestration.
   The full path in `Pipeline.cpp` must be made stage-oriented so graph build,
   prune, rewrite, forward compilation, WMC, and output routing are explicit
   phases rather than ad hoc branches.
4. Rewrite subsystem boundaries.
   Legacy rewrite, implicit rewrite, iterative implicit rewrite, and split-mode
   policy must be isolated behind a clearer rewrite interface. Their full-only
   constraints must remain explicit and testable.
5. Experimental evaluator boundaries.
   SCBF should remain an opt-in experimental evaluator. Approx/AMC should remain
   an offline toolchain until it is mature enough to become a runtime backend.
   Refactor scope includes separating these lanes so experiments stop distorting
   the main exact runtime structure.
6. Incremental execution and CLI layering.
   Parsing, command queueing, mode selection, delta materialization, graph
   mutation, formula update, WMC, and output dumping must stop living in one
   large CLI implementation. The CLI shell should become a thin interface over a
   reusable turn executor.
7. Shared runtime state and object ownership.
   Static or header-level state around derivations, probability maps, debugging,
   and graph identifiers must move toward explicit runtime context ownership.
   Graph node/edge ownership must be made explicit enough to avoid hidden
   lifetime problems.
8. Source naming and source-tree organization.
   Runtime library code, standalone tools, smoke mains, benchmark mains, and
   experiment helpers should no longer be mixed casually. The refactor must
   define a target layout for runtime code, tool entrypoints, and experimental
   code, then migrate toward it in stages.
9. Documentation naming and documentation-system cleanup.
   The current doc system has the right high-level layers, but file naming is
   inconsistent. Refactor scope includes a naming policy and a migration path
   for docs, not just code.
10. Validation and benchmark ownership.
    The maintained regression suite, example workflow, benchmark scripts, and
    provenance rules are part of the refactor surface because they define what
    behavior the runtime is allowed to preserve or change.

## Goals
- Preserve existing supported semantics while lowering coupling.
- Make the exact full and incremental runtime the primary stable product line.
- Keep experimental SCBF and offline Approx/AMC lanes available without letting
  them define the main runtime structure.
- Make pipeline stages, mode policy, and output policy explicit.
- Make source layout and naming reflect capability boundaries instead of
  historical accumulation.
- Make documentation discoverable enough that new work no longer starts by
  reverse-engineering naming accidents.

## Non-Goals
- No algorithmic replacement just because code is being moved.
- No immediate rewrite of `inc-regional`, SCBF, or Approx/AMC semantics.
- No benchmark recalibration or claim refresh unless results are rerun under the
  existing research protocol.
- No big-bang directory move in the first phase.
- No silent behavior changes to output naming, `setmode` aliases, evidence
  policy, or rewrite/full-only constraints.
- No reorganization, renaming, or cleanup under repository-root `research/` or
  `docs/research/` as part of this refactor.

## Target Refactor Shape
The runtime should eventually read as a small number of explicit layers.

- `ExecutionConfig`
  - one authoritative parse of runtime modes, backend kind, rewrite policy,
    dump policy, and output policy
- `RunProfile`
  - explicit distinction among exact runtime, experimental runtime, and offline
    graph replay tooling
- `RuntimeContext`
  - explicit ownership of derivation stores, precomputed outputs, profiling
    flags, and other execution-scoped state
- `GraphSession`
  - derivation graph build, prune, delta apply, and graph-lifecycle operations
- `RewriteEngine`
  - legacy graph rewrite, implicit rewrite, iterative implicit rewrite, and
    split policy under one full-only interface
- `FormulaEngine`
  - exact full BDD, exact full SDD, exact incremental naive, exact incremental
    regional, and experimental evaluator adapters
- `TurnExecutor`
  - staged incremental/full commit execution independent of interactive I/O
- `CliShell`
  - command parsing and user interaction only
- `OfflineQueryTooling`
  - derivation JSON query slicing, symbolic formula extraction, weighted-to-
    unweighted conversion, and Approx/AMC integration

These names are working names, not a requirement to ship those exact class or
file names.

## Source and Naming Reorganization Scope
The refactor should include a physical organization plan for code and docs.

- Source-tree direction:
  - runtime library code grouped by capability line such as `graph`,
    `rewrite`, `fc`, `incremental`, and `runtime`
  - standalone tools moved out of runtime implementation directories
  - smoke and benchmark mains separated from reusable library code
  - approximate reusable helpers moved out of `experiments/` once they become
    shared implementation rather than one-off probes
- Source naming direction:
  - file names should communicate whether a unit is runtime library code, a
    standalone tool, a benchmark entrypoint, or an experiment
  - large capability modules should have predictable header/implementation
    pairing
- Documentation naming direction:
  - keep top-level doc layers (`project`, `topics`, `research`, `design`,
    `historical`)
  - use `README.md` for landing pages and descriptive file names for leaf docs
  - stop relying on context-dependent names such as `FINAL.md`, `OPT.md`, or
    `Plan.md` for active documentation
  - keep `docs/INDEX.md` as the source-of-truth directory during migration

## Refactor Plan
The work should be executed in phases, with each phase independently shippable.

## Branch Coordination Constraint
- Treat repository-root `research/` and `docs/research/` as frozen boundaries
  for this refactor.
- Assume a parallel branch may keep changing repository-root `research/` for
  auto-research work.
- Do not fold research-area cleanup, naming normalization, or structure changes
  into the refactor branch.
- If a code or doc change outside those frozen areas would normally require a
  research-area update, defer that update or coordinate it as a separate merge,
  rather than broadening the refactor scope.

### Phase 0: Baseline and Invariants
- Write down the runtime invariants that cannot change during refactor.
- Confirm the supported mode matrix, evidence guard, rewrite/full-only rule,
  output naming families, and benchmark provenance rules.
- Identify which docs own current behavior, which own research conclusions, and
  which own proposals.

### Phase 1: Config, Modes, and Output Policy
- Introduce one typed configuration path for runtime mode, backend, rewrite
  selection, dump policy, and output naming.
- Eliminate repeated string parsing and duplicated alias tables.
- Keep current external CLI flags and `setmode` spellings stable.

### Phase 2: Runtime Context and Shared State
- Pull derivation stores, probability outputs, profiling flags, and other
  execution-scoped state into an explicit runtime context.
- Stop relying on header-level or globally scattered runtime state as the main
  coordination mechanism.

### Phase 3: Pipeline Stage Extraction
- Make full-mode execution stages explicit:
  - graph build
  - prune
  - rewrite
  - formula build
  - counting
  - output emission
- Keep exact runtime and experimental runtime branches distinct inside that
  staged model.

### Phase 4: CLI and Turn Executor Split
- Extract command parsing, command queueing, and turn execution from the current
  CLI implementation.
- Make batch and interactive incremental execution share the same executor.
- Keep the existing command language and mode behavior stable.

### Phase 5: Graph Ownership and Lifecycle
- Clarify ownership of graph nodes, hyperedges, adjacency data, and graph-level
  caches.
- Remove lifetime ambiguity and reduce risk from ownership cycles or hidden
  shared state.

### Phase 6: Formula/Backend Layer Cleanup
- Separate exact full engines, exact incremental engines, and experimental
  evaluators behind clearer interfaces.
- Keep BDD and SDD exact behavior as the correctness baseline.
- Keep SCBF experimental and keep Approx/AMC offline until integration is
  justified by correctness and operational maturity.

### Phase 7: Source-Tree and Entry-Point Reorganization
- Move standalone tools, smoke binaries, and benchmark mains out of runtime
  implementation locations.
- Reorganize runtime implementation files by capability line.
- Perform file and directory renames only after interfaces are stable enough to
  avoid churn-heavy rename noise.

### Phase 8: Documentation and Naming Migration
- Normalize doc naming across `docs/`, benchmark docs, and historical notes.
- Keep active behavior docs, research docs, design docs, and historical docs in
  clearly different layers.
- Update `docs/INDEX.md` and the relevant landing pages as each rename lands.

## Acceptance
The refactor should be accepted only when both behavior and repository hygiene
improve together.

### Semantic Acceptance
- Full exact runtime produces the same `facts.prob` and expected query outputs
  as before on maintained checks.
- Rewrite variants preserve current checked equivalence against no-rewrite on
  the maintained suites.
- Incremental modes preserve their current checked relationships against
  `full-hard` and preserve current output naming contracts.
- Evidence remains guarded to full-only mode unless a separate feature change
  explicitly expands support.
- SCBF and Approx/AMC remain correctly labeled as experimental or offline until
  a later project decision changes that status.

### Verification Acceptance
- Build succeeds with the repo-supported commands in [docs/TESTING.md](docs/TESTING.md):
  - `cmake -S . -B build`
  - `cmake --build build -j${JOBS}`
- Example smoke run succeeds:
  - `SOUFFLE_BIN=./build/src/souffle examples/running_example/run.sh`
- Maintained regression suite succeeds:
  - `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`
- Any runtime-facing change that touches probabilistic or incremental behavior is
  checked against the maintained regression suite before being treated as safe.
- Side-channel full and incremental workflows are rerun through their current
  documented entry points when refactor phases touch their owned behavior.
- Taint or other research conclusions are updated only under the provenance
  rules in [README.protocol.md](docs/research/README.protocol.md).

### Repository Acceptance
- Runtime library code, tools, benchmarks, and experiments have clearer physical
  separation than before.
- Newly created or renamed docs appear in `docs/INDEX.md` and the owning landing
  page.
- Active docs stop accumulating new context-dependent names that hide purpose.
- The refactor does not leave the repository in a state where current behavior,
  research conclusions, and proposals are indistinguishable.

## Why This Refactor Is Necessary
- The main runtime is doing too many jobs at once.
  Exact runtime behavior, experimental evaluators, and offline tooling are all
  present, but their boundaries are not explicit enough in either code or docs.
- Full and incremental support are both important first-class capabilities.
  The repository should be organized around that reality instead of treating
  them as incidental branches.
- Rewrite and split machinery now matter operationally.
  They are not just experiments anymore, so their lifecycle and constraints need
  to be explicit in both code and docs.
- Approx/AMC is still an active direction.
  If the main runtime stays structurally tangled, adding an approximate backend
  later will become harder and riskier than it needs to be.
- Hidden state and oversized control surfaces make correctness work fragile.
  This is especially risky for incremental semantics, output naming, and mixed
  full/inc evaluation modes.
- The documentation system is conceptually right but operationally noisy.
  Naming inconsistencies make it too easy to put stable guidance, design notes,
  and historical scratch material side by side without enough separation.
- Benchmark and provenance rules are part of correctness here.
  This fork is research-heavy, so a refactor that ignores benchmark ownership
  and provenance discipline will create false confidence.

## Implementation Rule Of Thumb
- Refactor interface boundaries first.
- Refactor execution state second.
- Refactor physical file layout third.
- Refresh benchmark conclusions only after the refactor stops moving semantics.

## Related commits
- `UNCOMMITTED` — docs(design): add complete refactor scope, phases, and acceptance contract
