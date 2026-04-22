# Architecture

This artifact evaluates probabilistic Datalog programs. The evaluator supplies a
`.dl` program, a fact directory, and optional `.prob` files. The compiler
produces a benchmark binary. That binary writes output tuple probabilities to
`facts.prob`.

The public comparison is plain exact inference versus exact inference with
`--rewrite`. Both runs should produce the same tuple keys. The optimized run
uses graph rewrite and component-level shortcuts before invoking the decision
diagram backend.

The two implementation contributions are:

1. Faster derivation graph generation. The generated binary records compact rule
   applications while Souffle's semi-naive evaluator runs, then constructs only
   the graph needed for requested outputs.
2. Faster probabilistic solving. The backend can either compile the graph
   directly to a decision diagram or first rewrite and decompose the graph so
   weighted model counting runs on smaller or simpler components.

The evaluator-facing output is `facts.prob`.

## 0. Compile

`souffle` first compiles the input `.dl` program. This uses Souffle's normal AST
pipeline, including component checks, alias resolution, relation dependency
analysis, join planning, and other Datalog optimizations before C++ emission.

The probabilistic extension adds generated metadata for exact inference: rule
probabilities, query/output relations, relation dependency components, and
deterministic/probabilistic relation classification used by `--det-opt`.

Key sources:

- [src/MainDriver.cpp:440](../src/MainDriver.cpp#L440): builds the AST
  transformation pipeline.
- [src/synthesiser/Synthesiser.cpp:324](../src/synthesiser/Synthesiser.cpp#L324):
  computes relation dependency metadata for deterministic optimization.
- [src/synthesiser/Synthesiser.cpp:4020](../src/synthesiser/Synthesiser.cpp#L4020):
  emits the generated binary's command-line options and relation metadata.
- [src/synthesiser/Synthesiser.cpp:4091](../src/synthesiser/Synthesiser.cpp#L4091):
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

- [src/synthesiser/Synthesiser.cpp:2421](../src/synthesiser/Synthesiser.cpp#L2421):
  emits rule-application recording for derivations.
- [src/synthesiser/Synthesiser.cpp:3737](../src/synthesiser/Synthesiser.cpp#L3737):
  emits the semi-naive `runAll` stage.
- [src/synthesiser/Synthesiser.cpp:819](../src/synthesiser/Synthesiser.cpp#L819):
  hands the generated program to the probabilistic runtime pipeline.

## 2. Build The Derivation Graph

After semi-naive evaluation, the runtime builds a derivation graph from recorded
rule applications. Nodes are derived or input tuples. Hyperedges are rule
applications, with edge probabilities for probabilistic rules and node
probabilities for probabilistic facts.

The graph construction is compact because it consumes recorded rule
applications instead of reconstructing derivations by re-running joins. The
construction step also restores aggregate witness edges, attaches requested
queries, and records graph statistics. The pruning step keeps only nodes and
edges that can affect requested outputs.

Key sources:

- [src/problog/Pipeline.cpp:1947](../src/problog/Pipeline.cpp#L1947): top-level
  runtime pipeline.
- [src/include/souffle/problog/DerivationGraph.h:1972](../src/include/souffle/problog/DerivationGraph.h#L1972):
  builds the working derivation graph from recorded rule applications.
- [src/include/souffle/problog/DerivationGraph.h:1273](../src/include/souffle/problog/DerivationGraph.h#L1273):
  indexes aggregate witnesses during graph construction.
- [src/include/souffle/problog/DerivationGraph.h:2053](../src/include/souffle/problog/DerivationGraph.h#L2053):
  prunes the graph to output requirements.

## 3. Solve Probabilities

The baseline solver compiles the pruned graph directly into a formula and runs
weighted model counting.

The optimized solver is enabled by `--rewrite`. The runtime inspects rule
metadata and chooses a graph reduction strategy automatically:

- Programs with probabilistic rules keep split choices in a compact overlay
  while local reductions and compaction shrink the graph that reaches exact
  inference.
- Programs whose rules are deterministic skip split bookkeeping and use direct
  graph reduction.

The reduced graph is then decomposed into independent components. Components
with no random variables, one random variable, or simple conjunction structure
are solved directly. The remaining components are compiled to a decision
diagram and solved by weighted model counting.

Key sources:

- [src/problog/Pipeline.cpp:71](../src/problog/Pipeline.cpp#L71): selects the
  rewrite implementation used by `--rewrite`.
- [src/problog/Pipeline.cpp:566](../src/problog/Pipeline.cpp#L566): default
  exact-inference path, including direct component solvers and WMC.
- [src/include/souffle/problog/GraphRewriter.h:99](../src/include/souffle/problog/GraphRewriter.h#L99):
  explicit SISO graph rewrite.
- [src/problog/ImplicitSplitRewrite.cpp:2288](../src/problog/ImplicitSplitRewrite.cpp#L2288):
  implicit split rewrite pipeline.
- [src/include/souffle/problog/PipelineComponents.h:32](../src/include/souffle/problog/PipelineComponents.h#L32):
  component classification and fast-path data structures.
- [src/include/souffle/problog/PipelineComponents.h:144](../src/include/souffle/problog/PipelineComponents.h#L144):
  zero-random-variable conjunction evaluation.

## 4. Decision Diagram Interface

The formula layer uses a decision diagram manager interface. The artifact
evaluation path uses the CUDD-backed BDD manager because it is the supported
and reproducible backend for these workloads.

Key sources:

- [src/include/souffle/problog/formula/FormulaManager.h:8](../src/include/souffle/problog/formula/FormulaManager.h#L8):
  common formula and weighted model counting interface.
- [src/include/souffle/problog/formula/CuddManager.h:160](../src/include/souffle/problog/formula/CuddManager.h#L160):
  CUDD-backed BDD manager.
- [src/include/souffle/problog/ForwardCompilation.h:78](../src/include/souffle/problog/ForwardCompilation.h#L78):
  graph-to-formula compilation.
- [src/include/souffle/problog/ForwardCompilation.h:1098](../src/include/souffle/problog/ForwardCompilation.h#L1098):
  component-wise formula compilation.

## 5. Output

The runtime writes output tuple probabilities to `<output-dir>/facts.prob`.
Timing and graph statistics are written to the JSON log selected by `--logfile`.

Key sources:

- [src/include/souffle/problog/DerivationGraph.h:2467](../src/include/souffle/problog/DerivationGraph.h#L2467):
  writes `facts.prob`.
- [src/problog/debug/Debugger.cpp:133](../src/problog/debug/Debugger.cpp#L133):
  records stage timing and profiling metadata.
