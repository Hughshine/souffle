# Architecture

This branch packages the full-mode probabilistic runtime used by generated
benchmark binaries. A run starts from concrete facts, records the derivations
needed for requested outputs, and computes output probabilities.

## Runtime Flow

1. Load input facts and optional fact probabilities.
2. Execute the generated Souffle program.
3. Build a derivation graph from recorded rule applications.
4. Prune the graph to output and evidence requirements.
5. Apply graph rewrite when the run uses `--rewrite`.
6. Split the remaining graph into components.
7. Compile components to BDD formulas and run weighted model counting.
8. Write output tuple probabilities.

## Rewrite Role

Rewrite runs reduce probability-computation cost before BDD compilation. The
public run command uses `--rewrite`; the runtime chooses the concrete rewrite
strategy from rule metadata.

The current policy is:

- Probabilistic rules use implicit split rewrite.
- Deterministic rules use explicit graph rewrite with split disabled.

This keeps the benchmark command stable while allowing different workloads to
use the rewrite path that matches their graph shape.

## Main Components

- `src/MainDriver.cpp`: compiler driver and generated-binary option plumbing.
- `src/synthesiser/Synthesiser.cpp`: generated C++ emission and full-mode hooks.
- `src/problog/Pipeline.cpp`: full-mode runtime pipeline.
- `src/include/souffle/problog/DerivationGraph.h`: derivation graph data model.
- `src/include/souffle/problog/GraphRewriter.h`: explicit graph rewrite.
- `src/problog/ImplicitSplitRewrite.cpp`: implicit split rewrite.
- `src/include/souffle/problog/ForwardCompilation.h`: graph-to-formula
  compilation.

## Data Products

- Inputs: `<relation>.facts` and optional `<relation>.prob`.
- Outputs: `facts.prob`.
- Timing logs: JSON files controlled by `--logfile`.

## Further Reading

- [USAGE.md](USAGE.md): evaluator-facing commands and file formats.
- [TESTING.md](TESTING.md): regression and benchmark validation.
- [INDEX.md](INDEX.md): complete documentation map.
