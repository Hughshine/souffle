# Schlandals Stratified-Negation Checks

This directory keeps small, explicit experiment cases for the experimental
`--backend schlandals` path in
[`src/problog_graph_query.cpp`](/home/hugh/research/datalog/souffle/src/problog_graph_query.cpp).

These are not wired into `CMake` or `ctest`. They are meant to be lightweight,
repo-local checks while the backend is still experimental.

## Coverage

- positive no-negation regression on an existing side-channel bundle
- legal acyclic stratified negation
- explicit rejection of negative recursion / non-stratified negation
- explicit rejection of probabilistic negated lower-stratum predicates

## Runner

From the repo root:

```bash
./experiments/approxmc_demo/schlandals_negation/run_schlandals_negation_tests.sh
```

The runner expects:

- `build/src/souffle-problog-graph-query` to exist
- the side-channel regression bundle
  `problog-benchmark/.worktree/full-artifact/side_channel_full/P6/output/derivation.json`
  to exist

## Cases

- `cases/stratified_negation.json`
  - `pass(1)` should evaluate to `0.7`
  - `blocked_pass(1)` should evaluate to `0`
- `cases/negative_recursion.json`
  - `a(1)` should be rejected as non-stratified / recursion through negation
- `cases/probabilistic_negation.json`
  - `a(1)` should be rejected because the negated lower-stratum relation is probabilistic
