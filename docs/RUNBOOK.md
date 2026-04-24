# Runbook

## Source References

- [../README.md:31](../README.md#L31): build command.
- [../src/MainDriver.cpp:636](../src/MainDriver.cpp#L636): compiler-facing incremental options.
- [../src/synthesiser/Synthesiser.cpp:673](../src/synthesiser/Synthesiser.cpp#L673): generated runtime entry.
- [../src/include/souffle/CompiledOptions.h:709](../src/include/souffle/CompiledOptions.h#L709): runtime flags.
- [topics/evaluation/README.artifact.inc.md:1](topics/evaluation/README.artifact.inc.md#L1): benchmark workflow.

## Build

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

CUDD must be available for generated probabilistic runtimes. The AE branch uses
the BDD backend as a fixed implementation default.

## Local Run

```bash
./build/src/souffle -F input -D output compute.souffle.dl -o compute
./compute -F input -D output --setmode inc-regional < delta.txt
```

Run `full` on the same delta stream as the recomputation oracle:

```bash
./compute -F input -D output --setmode full < delta.txt
```

## Benchmark Run

Use the companion `problog-benchmark` repository on branch `CAV-INC`:

```bash
SOUFFLE_BIN=/path/to/inc-artifact-ae/build/src/souffle \
python3 benchmarks/side_channel/cli/side_channel_inc.py <command> ...
```

The current AE subset is `P13` through `P20`.

## Troubleshooting

- Missing CUDD headers or library: install CUDD and rebuild.
- Generated binary link failure: confirm the generated compile script sees the intended CUDD installation.
- Incremental mismatch: compare the same delta stream under `full`.
- Regression failure: inspect the per-case work directory under `build/tests/regression`.
