# Derivation Graph -> Weighted AMC Pipeline Demo

This document describes the standalone demo:
- source: `experiments/approxmc_demo/dg_wamc_pipeline_demo.cpp`
- binary target: `souffle-amc-pipeline-demo`

It demonstrates a functional end-to-end path for probabilistic query evaluation:
1. build a toy derivation graph in C++
2. extract a boolean formula for each query
3. encode formula to CNF (Tseitin)
4. convert weighted CNF to unweighted CNF
5. run ApproxMC on the unweighted CNF
6. reconstruct weighted probability and compare with exact baselines

## Scope

What this demo is for:
- validate pipeline functionality and API shape
- show how weighted AMC can be driven from C++ without Python I/O
- provide output diagnostics useful for Souffle backend integration

What this demo is not:
- a full Souffle production backend
- an optimized implementation for large graphs
- a replacement for DD-based inference paths

## Build And Run

From repo root:

```bash
cmake -S . -B build
cmake --build build --target souffle-amc-pipeline-demo -j$(nproc || sysctl -n hw.ncpu || echo 2)
APPROXMC_BIN=/path/to/approxmc ./build/src/souffle-amc-pipeline-demo --epsilon 0.1 --delta 0.05 --seed 1
```

`APPROXMC_BIN` can also be omitted if `--approxmc-bin` is passed.  
Default fallback path in code is `/tmp/approxmc-bin/approxmc`.

## CLI Options

- `--approxmc-bin <path>`: path to ApproxMC executable
- `--epsilon <float>`: ApproxMC epsilon (must be `> 0`)
- `--delta <float>`: ApproxMC delta (must be `0 < delta < 1`)
- `--seed <uint>`: random seed used by ApproxMC
- `-h`, `--help`: usage

## Pipeline Stages (Code Map)

- Formula extraction from derivation graph:
  - `GraphFormulaBuilder::buildNodeFormula`
  - `GraphFormulaBuilder::buildEdgeFormula`
- Baseline graph-semantics evaluator:
  - `GraphSemanticsEvaluator`
- Formula -> CNF:
  - `TseitinCnfEncoder`
- Weighted -> unweighted conversion:
  - `approxmc_demo::convertWeightedToUnweighted` from `weighted_conversion.cpp`
- ApproxMC invocation and parse:
  - `runApproxmc`
- Weighted count reconstruction:
  - `applyMultiplier`

## Output Interpretation

The demo prints one block per query node.

Example fields:

- `formula`
  - extracted query formula over random variables (`v1`, `v2`, ...)
- `P(formula exact enum)`
  - exact weighted probability via full assignment enumeration over random vars
- `P(graph semantics enum)`
  - exact probability computed directly from derivation-graph semantics
- `P(weighted AMC approxmc)`
  - result from ApproxMC path:
    unweighted count from ApproxMC, then scaled back to weighted value
- `approxmc detail`
  - `unweighted`: ApproxMC `s mc` estimate
  - `multiplier`, `divideExp`: reconstruction factors from weighted->unweighted conversion
- `P(manual expected)`
  - hard-coded value for the toy graph
- `|amc-expected|`
  - absolute difference between AMC result and manual expected
- `|amc-exact|`
  - absolute difference between AMC result and exact formula enumeration
- `approx tolerance`
  - acceptance threshold used by demo:
    `max(1e-9, epsilon * max(|exact|, 1e-12))`
- `|formula-graph|`
  - sanity check that extracted formula matches graph semantics

The run ends with:
- `AMC pipeline check passed.` when all query blocks satisfy tolerance
- or failure message if a query violates tolerance or a stage errors out

## Accuracy Notes

ApproxMC is approximate with probabilistic guarantees controlled by `(epsilon, delta)`.  
This demo validates against exact baselines because the toy graph is tiny.

For this toy case, one query may still differ slightly from exact value.  
That is expected and acceptable if it stays within the configured tolerance.

## Performance Caveat (Important)

This demo's weighted path is not native weighted counting in ApproxMC.
It works by:
- converting weighted CNF to unweighted CNF
- running unweighted ApproxMC
- reconstructing weighted probability with `multiplier/divideExp`

So runtime is often dominated by conversion + larger SAT workload after conversion.

Main reasons:
- each non-trivial weighted variable can introduce auxiliary variables/clauses
- conversion size growth is tied to weight quantization precision
- larger converted CNF leads to more SAT search effort (conflicts/restarts/rounds)

Implication:
- good for validating functionality and interfaces
- may be inefficient for large derivation graphs with many weighted variables
- if performance is critical, compare against DD-based WMC and/or pursue native weighted approximate counting

## Temporary Files

For each query, the demo writes an intermediate unweighted DIMACS file to:
- `/tmp/souffle_amc_demo_<query>.cnf`

The file is deleted after ApproxMC returns.

## Troubleshooting

- `ApproxMC binary not found`
  - set `APPROXMC_BIN` or pass `--approxmc-bin`
- `ApproxMC parse error`
  - verify DIMACS compatibility and ApproxMC version
- `ApproxMC output did not contain 's mc <count>'`
  - check ApproxMC CLI output format and flags
- `AMC pipeline check failed`
  - try larger `epsilon` first to confirm behavior, then inspect conversion diagnostics
