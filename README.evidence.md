# Evidence: End-to-End Flow and Semantics

## Source references
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/parser/parser.yy](src/parser/parser.yy)
- [src/parser/ParserDriver.cpp](src/parser/ParserDriver.cpp)
- [src/ast/transform/EvidenceChecker.cpp](src/ast/transform/EvidenceChecker.cpp)
- [src/ast2ram/online/UnitTranslator.cpp](src/ast2ram/online/UnitTranslator.cpp)
- [src/synthesiser/Synthesiser.cpp](src/synthesiser/Synthesiser.cpp)
- [src/include/souffle/problog/GraphAnalyzer.h](src/include/souffle/problog/GraphAnalyzer.h)
- [src/include/souffle/problog/GraphRewriter.h](src/include/souffle/problog/GraphRewriter.h)


This document summarizes how `evidence(...)` is parsed, validated, compiled, and applied
in the probabilistic pipeline.

## Status
- Active behavior reference for evidence handling.

## 1. Syntax and AST

Accepted syntax:
```
evidence(atom, true).
evidence(atom, false).
```
Parser rules live in `src/parser/parser.yy` and create `ast::Evidence` nodes
(`src/ast/Evidence.h`, `src/ast/Evidence.cpp`).

## 2. Parsing and semantic checks

- The parser inserts evidence into `ast::Program` via `ParserDriver::addEvidence`
  (`src/parser/ParserDriver.cpp`).
- `EvidenceSemanticChecker` validates evidence directives:
  - relation exists
  - arity matches
  (`src/ast/transform/EvidenceChecker.cpp`)

## 3. RAM and codegen

- `UnitTranslator` collects evidence and stores it in `ram::Program` as
  `ram::Evidence` entries (`src/ast2ram/online/UnitTranslator.cpp`,
  `src/ram/Evidence.h`).
- `UnitTranslator::translateEvidence()` returns an empty statement: evidence does
  not generate RAM runtime operations.
- `Synthesiser` emits a C++ vector of `(UntypedTuple, bool)` and passes it into
  the pipeline (`src/synthesiser/Synthesiser.cpp`).

## 4. Derivation graph attachment

- `Node` tracks evidence flags (`hasEvidence`, `evidenceValue`) in
  `src/include/souffle/problog/DerivationGraph.h`.
- `DerivationGraph::attachEvidence()` resolves tuple -> node, sets evidence flags,
  records the evidence list on the graph, and logs if a tuple is missing.
- Evidence tags appear in `Node::toString()` as `[E:true]` or `[E:false]`.

## 5. Pruning and reachability

Incremental prune includes evidence nodes as anchors:
- Evidence nodes are inserted into `reachableNodes` and used to seed the backward
  BFS in `prune-inc` so they are retained even if not on a query path
  (`src/include/souffle/problog/DerivationGraph.h`).
- When `--prune-extra` is enabled, outputless components are dropped so isolated
  evidence-only subgraphs may be pruned.

## 6. Equivalence merging (eqrel/SCC)

When deterministic equivalence classes are merged:
- The representative inherits evidence/query flags.
- Conflicting evidence values prevent a merge (skip).
See `src/include/souffle/problog/DerivationGraph.h` and the notes in `README.eqrel.md`.

## 7. Rewrite and split constraints

Rewrite detectors and cleanup skip evidence/output facts in fast paths:
- Fast-path SISO detection excludes evidence/needOutput facts for
  `SingleHyperedge` and `AllFactsToSO`, and excludes evidence/needOutput mid nodes
  for `LinearTwoEdge` (`src/include/souffle/problog/GraphAnalyzer.h`).
- Edge compaction and isolated-node cleanup in `GraphRewriter` avoid
  evidence/needOutput facts (`src/include/souffle/problog/GraphRewriter.h`).
- Split only considers fact nodes that are not evidence and not needOutput
  (`src/include/souffle/problog/GraphRewriter.h`), and also skips facts whose
  component contains any evidence (from a snapshot computed at rewrite start).

## 8. Probabilistic conditioning (BDD/SDD pipeline)

Evidence is applied during probability computation in the pipeline:
- `CycleDependencyGraph` computes undirected connected components over the
  pruned derivation graph; each component collects its own evidence list.
- For each component, a conjunction of evidence literals is built as a BDD/SDD.
- `evidenceWeight` is computed per component; if it is zero, nodes in that
  component have probability 0. Otherwise, probabilities are conditioned by
  `Pr(node | evidence_component) = WMC(node AND evidence_component) / WMC(evidence_component)`.
- Timings are logged as `component evidence build` and `component evidence WMC`.

## 9. Incremental CLI behavior

When `--online` enables the interactive CLI (`IncrementalCLI`) after the initial
full run:

- The **initial full run** in `runPipeline()` is evidence-conditioned as described
  above.
- The CLI supports `insert`/`delete` of facts and `setmode` to switch between
  `inc-*` and `full-*` modes. There is **no CLI command to add/remove evidence**.
- In **incremental modes** (`inc`/`inc-regional`), evidence conditioning is
  preserved: evidence literals are rebuilt from the current formulas, and
  conditional probabilities are computed against the evidence list stored on the
  graph.
- In **full modes** inside the CLI, formulas are rebuilt from scratch after
  `graph->applyDelta(...)`; the graph's evidence list is reused, so conditioning
  is preserved.

Implication: evidence is **fixed at program load**, but **conditioning remains
active** across incremental/full updates in the CLI.

## 10. Notes and limitations

- Evidence affects probabilistic conditioning, not the core Datalog evaluation.
  The RAM execution does not process evidence; it is handled in the pipeline.
- Evidence must match an existing tuple in the derivation graph; missing tuples
  are reported as errors or runtime exceptions.

## 11. Evidence partitioning by connectivity (post-prune)

Goal: avoid conditioning every output on all evidence when parts of the pruned
derivation graph are disconnected. Evidence should only influence nodes in the
same connected subgraph (e.g., via shared random variables).

Implementation notes:
- `CycleDependencyGraph` now computes undirected components on top of SCCs.
- It exposes `node -> componentId` and `componentId -> evidence list`.
- WMC uses the component-local evidence formula for each node.

Expected effect: smaller evidence formulas and less redundant conditioning in
large benchmarks with multiple disconnected subgraphs.

## Related commits
- `812ea4081` — docs(repo): refine README narratives
- `21a083b93` — perf(problog): scope evidence conditioning by component
