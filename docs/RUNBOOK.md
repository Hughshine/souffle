# Runbook

## Source References

- [../README.md:31](../README.md#L31): build command.
- [../src/MainDriver.cpp:636](../src/MainDriver.cpp#L636): compiler-facing incremental options.
- [../src/synthesiser/Synthesiser.cpp:673](../src/synthesiser/Synthesiser.cpp#L673): generated runtime entry.
- [../src/include/souffle/CompiledOptions.h:709](../src/include/souffle/CompiledOptions.h#L709): runtime flags.

## Build

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

CUDD must be available for generated probabilistic runtimes. This branch uses
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

## Troubleshooting

- Missing CUDD headers or library: install CUDD and rebuild.
- Generated binary link failure: confirm the generated compile script sees the intended CUDD installation.
- Incremental mismatch: compare the same delta stream under `full`.
- Regression failure: inspect the per-case work directory under `build/tests/regression`.
