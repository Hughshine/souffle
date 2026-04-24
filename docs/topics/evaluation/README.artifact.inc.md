# Incremental Artifact Evaluation

This document is the AE run guide for the compiler branch `inc-artifact-ae`.
The side-channel benchmark driver lives in the companion `problog-benchmark`
repository on branch `CAV-INC`.

## Source References

- [../../../README.md:1](../../../README.md#L1): branch scope.
- [../../../src/MainDriver.cpp:636](../../../src/MainDriver.cpp#L636): AE compiler parameters.
- [../../../src/include/souffle/CompiledOptions.h:187](../../../src/include/souffle/CompiledOptions.h#L187): runtime mode syntax.
- [../../../src/include/souffle/cli/Cli.h:679](../../../src/include/souffle/cli/Cli.h#L679): update commands.
- [../../../tests/regression/run_regression_case.py:153](../../../tests/regression/run_regression_case.py#L153): maintained compile invocation.

## Scope

Current AE scope is side-channel cases `P13` through `P20`. The benchmark grid
is 3 by 5:

- delta sizes: `0.5%`, `1.0%`, `1.5%`;
- delete ratios: `0`, `0.25`, `0.5`, `0.75`, `1`.

Keep the 3 by 5 table shape even when a case has fewer valid measurements; use
empty or `NA` cells rather than changing the grid.

## Build Compiler

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

## Benchmark Driver

From the companion `problog-benchmark` checkout on `CAV-INC`:

```bash
SOUFFLE_BIN=/path/to/inc-artifact-ae/build/src/souffle \
python3 benchmarks/side_channel/cli/side_channel_inc.py --help
```

The compiler invocation should not pass removed algorithm switches. The
generated runtime defaults to BDD, deterministic-relation analysis, CUDD
variable-index reuse, and adaptive reordering.

## Run Shape

The expected sequence is:

1. generate or verify inputs for `P13` through `P20`;
2. generate mixed deltas for the 3 by 5 grid;
3. compile with this branch's `souffle` binary;
4. run incremental modes and compare against `full`;
5. collect TSV/JSON summaries for paper tables and figures.

Use `inc-naive` and `inc-regional` for incremental runs. Use `full` only
as the exact oracle.

## Optional Outputs

For debugging a specific cell, enable default-off outputs explicitly:

```bash
./compute -F input -D output --setmode inc-regional \
  --dump=dot,json,stat --profile-stage=inc,fc,wmc,inc-regional
```

Do not enable these outputs in timing runs unless the timing protocol requires
them; they add I/O and instrumentation overhead.

## Result Files

The benchmark driver should produce per-turn probability files, per-run logs,
per-cell summaries, and a collected `results-souffle-inc.tsv`. A mismatch
against `full` is a correctness failure, not a timing datapoint.
