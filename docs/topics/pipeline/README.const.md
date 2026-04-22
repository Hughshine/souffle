# Deterministic-Relation Analysis

## Source references
- [../../../src/include/souffle/CompiledOptions.h](../../../src/include/souffle/CompiledOptions.h)
- [../../../src/synthesiser/Synthesiser.cpp](../../../src/synthesiser/Synthesiser.cpp)
- [../../../src/include/souffle/problog/DerivationGraph.h](../../../src/include/souffle/problog/DerivationGraph.h)
- [../../../src/Derivation.cpp](../../../src/Derivation.cpp)

## Scope
`--det-opt` computes relation-level determinism before generated-program
execution and gates derivation recording for deterministic relations.

## Motivation
Only probabilistic choices need derivation edges.  Deterministic relations can be
evaluated normally while avoiding derivation tracking, reducing graph size before
prune/rewrite/FC/WMC.

## Classification
A relation is treated as probabilistic if:
- it has any input fact with probability strictly between `0` and `1`
- it is derived by any rule with probability strictly between `0` and `1`
- it depends on another probabilistic relation through the relation dependency graph

All other relations are deterministic for the run.

## Runtime Contract
Artifact commands include `--det-opt` for both plain and rewrite runs:
```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --logfile ae-plain
./compute -F <facts-dir> -D <output-dir> --det-opt --rewrite --logfile ae-rewrite
```

## Outputs
When enabled, the analysis writes deterministic-relation summaries used for
debugging.  These files are diagnostics, not correctness or performance oracles.
