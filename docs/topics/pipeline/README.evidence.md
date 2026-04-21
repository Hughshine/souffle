# Evidence Flow

## Source references
- [../../../src/include/souffle/problog/DerivationGraph.h](../../../src/include/souffle/problog/DerivationGraph.h)
- [../../../src/problog/Pipeline.cpp](../../../src/problog/Pipeline.cpp)
- [../../../src/parser/parser.yy](../../../src/parser/parser.yy)
- [../../../src/parser/ParserDriver.cpp](../../../src/parser/ParserDriver.cpp)
- [../../../src/ast/transform/EvidenceChecker.cpp](../../../src/ast/transform/EvidenceChecker.cpp)
- [../../../src/synthesiser/Synthesiser.cpp](../../../src/synthesiser/Synthesiser.cpp)

## Scope
Evidence is parsed, validated, attached to derivation graph nodes, and applied
during full-mode probability computation.

## Syntax
```souffle
evidence(atom, true).
evidence(atom, false).
```

## Pipeline
1. Parser stores evidence in `ast::Program`.
2. Semantic checks validate relation existence and arity.
3. Synthesizer emits evidence metadata into generated C++.
4. `DerivationGraph` resolves evidence tuples to graph nodes.
5. Pruning retains evidence nodes as anchors.
6. FC/WMC conditions component probabilities on component-local evidence.

## Limitations
- Evidence affects probabilistic conditioning, not core Datalog evaluation.
- Evidence must match a tuple present in the derivation graph.

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `21a083b93` — perf(problog): scope evidence conditioning by component
