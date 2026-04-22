# Architecture

This artifact evaluates probabilistic Datalog programs by extending Souffle's
compiled execution path. The two implementation contributions are:

1. Faster derivation graph generation. The generated binary records compact rule
   applications while Souffle's semi-naive evaluator runs, then constructs only
   the graph needed for requested outputs and evidence.
2. Faster probabilistic solving. The backend can either compile the graph
   directly to a decision diagram or first rewrite and decompose the graph so
   weighted model counting runs on smaller or simpler components.

The evaluator-facing output is `facts.prob`.

## 0. Compile

`souffle` first compiles the input `.dl` program. This uses Souffle's normal AST
pipeline, including component checks, alias resolution, relation dependency
analysis, join planning, and other Datalog optimizations before C++ emission.

The probabilistic extension adds generated metadata for full-mode execution:
rule probabilities, query/output relations, evidence relations, relation SCCs,
and deterministic/probabilistic relation classification used by `--det-opt`.

Key sources:

- [src/MainDriver.cpp](../src/MainDriver.cpp#L445): builds the AST
  transformation pipeline.
- [src/synthesiser/Synthesiser.cpp](../src/synthesiser/Synthesiser.cpp#L324):
  computes relation dependency metadata for deterministic optimization.
- [src/synthesiser/Synthesiser.cpp](../src/synthesiser/Synthesiser.cpp#L4018):
  emits the generated binary's command-line options and relation metadata.
- [src/synthesiser/Synthesiser.cpp](../src/synthesiser/Synthesiser.cpp#L4113):
  emits the probability-aware fact loader and deterministic relation prepass.

## 1. Evaluate

The generated binary runs Souffle's semi-naive evaluator. The emitted code is
adapted to probabilistic semantics by recording which rule applications derive
which tuples, while preserving Souffle's compiled evaluation strategy and join
order.

With `--det-opt`, the binary first reads `<relation>.facts` and matching
`<relation>.prob` files. It marks relations as deterministic when they cannot
depend on probabilistic facts or probabilistic rules. The semi-naive run still
computes relation contents; the probabilistic pipeline uses the recorded rule
applications and deterministic metadata after evaluation.

Key sources:

- [src/synthesiser/Synthesiser.cpp](../src/synthesiser/Synthesiser.cpp#L2421):
  emits rule-application recording for derivations.
- [src/synthesiser/Synthesiser.cpp](../src/synthesiser/Synthesiser.cpp#L4291):
  emits the semi-naive `runAll` stage.
- [src/synthesiser/Synthesiser.cpp](../src/synthesiser/Synthesiser.cpp#L819):
  hands the generated program to the probabilistic runtime pipeline.

## 2. Build The Derivation Graph

After semi-naive evaluation, the runtime builds a derivation graph from recorded
rule applications. Nodes are derived or input tuples. Hyperedges are rule
applications, with edge probabilities for probabilistic rules and node
probabilities for probabilistic facts.

The graph construction is compact because it consumes recorded rule
applications instead of reconstructing derivations by re-running joins. The
construction step also restores aggregate witness edges, attaches queries and
evidence, and records graph statistics. The pruning step keeps only nodes and
edges that can affect requested outputs or evidence.

Key sources:

- [src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp#L1943): top-level
  runtime pipeline.
- [src/include/souffle/problog/DerivationGraph.h](../src/include/souffle/problog/DerivationGraph.h#L1992):
  builds the working derivation graph from recorded rule applications.
- [src/include/souffle/problog/DerivationGraph.h](../src/include/souffle/problog/DerivationGraph.h#L2012):
  indexes aggregate witnesses during graph construction.
- [src/include/souffle/problog/DerivationGraph.h](../src/include/souffle/problog/DerivationGraph.h#L2073):
  prunes the graph to output and evidence requirements.

## 3. Solve Probabilities

The plain backend compiles the pruned graph directly into formulas and performs
weighted model counting. This is the baseline path.

The optimized backend is enabled by `--rewrite`. The runtime chooses the
concrete rewrite path from rule metadata:

- Programs with probabilistic rules use implicit split rewrite. It keeps split
  information in an overlay, applies local rewrites and compaction, and commits
  only the residual graph needed by the solver.
- Programs whose rules are deterministic use explicit graph rewrite with split
  disabled. This avoids split overhead and keeps the component-wise backend
  optimizations.

After rewrite, the backend decomposes the graph into components. Components with
one random variable, zero-random-variable conjunction structure, or simple
conjunction structure can bypass full decision diagram construction. Remaining
components are compiled to a decision diagram and solved by weighted model
counting.

Key sources:

- [src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp#L69): selects the
  rewrite implementation used by `--rewrite`.
- [src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp#L564): BDD backend
  pipeline, including component fast paths and WMC.
- [src/problog/Pipeline.cpp](../src/problog/Pipeline.cpp#L1380): SDD backend
  pipeline.
- [src/include/souffle/problog/GraphRewriter.h](../src/include/souffle/problog/GraphRewriter.h#L99):
  explicit SISO graph rewrite.
- [src/problog/ImplicitSplitRewrite.cpp](../src/problog/ImplicitSplitRewrite.cpp#L2269):
  implicit split rewrite pipeline.
- [src/include/souffle/problog/PipelineComponents.h](../src/include/souffle/problog/PipelineComponents.h#L32):
  component classification and fast-path data structures.
- [src/include/souffle/problog/PipelineComponents.h](../src/include/souffle/problog/PipelineComponents.h#L144):
  zero-random-variable conjunction evaluation.

## 4. Decision Diagram Interface

The formula backend is parameterized by a decision diagram manager interface.
This artifact ships BDD and SDD managers. The benchmark path uses BDD by
default because the CUDD implementation has the most mature engineering support
for this workload.

Key sources:

- [src/include/souffle/problog/formula/FormulaManager.h](../src/include/souffle/problog/formula/FormulaManager.h#L9):
  common formula and weighted model counting interface.
- [src/include/souffle/problog/formula/CuddManager.h](../src/include/souffle/problog/formula/CuddManager.h#L161):
  CUDD-backed BDD manager.
- [src/include/souffle/problog/formula/SddManager.h](../src/include/souffle/problog/formula/SddManager.h#L182):
  SDD manager.
- [src/include/souffle/problog/ForwardCompilation.h](../src/include/souffle/problog/ForwardCompilation.h#L474):
  graph-to-formula compilation.
- [src/include/souffle/problog/ForwardCompilation.h](../src/include/souffle/problog/ForwardCompilation.h#L1125):
  component-wise formula compilation.

## 5. Output

The runtime writes output tuple probabilities to `<output-dir>/facts.prob`.
Timing and graph statistics are written to the JSON log selected by `--logfile`.

Key sources:

- [src/include/souffle/problog/DerivationGraph.h](../src/include/souffle/problog/DerivationGraph.h#L2613):
  writes `facts.prob`.
- [src/problog/debug/Debugger.cpp](../src/problog/debug/Debugger.cpp#L1):
  records stage timing and profiling metadata.
