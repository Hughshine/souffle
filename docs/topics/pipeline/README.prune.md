# Prune Stage Notes

## Source references
- [../../../src/include/souffle/problog/DerivationGraph.h](../../../src/include/souffle/problog/DerivationGraph.h)
- [../../../src/problog/Pipeline.cpp](../../../src/problog/Pipeline.cpp)

## Scope
This note covers full-mode `DerivationGraph::prune`.

## What Prune Computes
- A subgraph reachable from output relations and evidence nodes by backward
  reachability.
- Node/edge `pruned` flags and a live graph view.
- Optional outputless-component pruning when the internal diagnostic flag is
  enabled.
- Deterministic bi-implication merge when safe in the full-mode pipeline.

## Artifact Interaction
The rewrite dispatcher runs after prune.  Therefore graph size after prune is
the relevant input to graph rewrite, implicit split, and component-wise FC/WMC.

## Correctness Constraints
- Output nodes and evidence nodes must remain reachable anchors.
- Merging must preserve output/evidence flags and probability semantics.
- Aggregate contribution nodes must remain connected to the output derivations
  they justify.
