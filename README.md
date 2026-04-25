# Incremental Probabilistic Souffle Artifact

This branch is the incremental AE compiler/runtime branch. The compiler always
emits the interactive incremental runtime. The public AE surface is intentionally
small: `inc-naive`, `inc-regional`, `full`, canonical dump/profile output
selectors, and the maintained regression suite.

The companion benchmark repository is `problog-benchmark` on branch `CAV-INC`.
Use this repository for the compiler and generated runtimes, and use the
companion repository for side-channel case generation and result collection.

## Source References

- [src/MainDriver.cpp:636](src/MainDriver.cpp#L636): compiler mode/output options.
- [src/MainDriver.cpp:719](src/MainDriver.cpp#L719): compiler default canonicalization.
- [src/synthesiser/Synthesiser.cpp:673](src/synthesiser/Synthesiser.cpp#L673): generated runtime enters the online pipeline.
- [src/include/souffle/CompiledOptions.h:187](src/include/souffle/CompiledOptions.h#L187): supported mode syntax.
- [src/include/souffle/CompiledOptions.h:709](src/include/souffle/CompiledOptions.h#L709): generated runtime option parser.
- [src/include/souffle/cli/Cli.h:679](src/include/souffle/cli/Cli.h#L679): interactive update commands.
- [tests/regression/CMakeLists.txt:22](tests/regression/CMakeLists.txt#L22): maintained regression cases.

## Build

The build requires the system C++ toolchain, CMake, Flex/Bison, Python 3,
Readline headers/libraries, and CUDD. CMake now fails during configure if
Readline or CUDD is missing; set `CUDD_ROOT`, `CUDD_INCLUDE_DIR`, or
`CUDD_LIBRARY` for nonstandard CUDD installs.

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

## Run

Compile a probabilistic Datalog program:

```bash
./build/src/souffle -F input -D output compute.souffle.dl -o compute
```

Run an incremental session:

```bash
./compute -F input -D output --setmode inc-regional
insert 0.3::edge(1,2)
delete edge(3,4)
commit
q
```

Use `full` on the same delta stream as the exact recomputation oracle.

For a small interactive cycle-repair example, see
[examples/cycle_repair_path/README.md](examples/cycle_repair_path/README.md).

## Optional Outputs

Extra graph/profiling outputs are off by default. Enable them explicitly when
collecting AE debugging material:

```bash
./compute -F input -D output --setmode inc-regional \
  --dump=dot,json,stat --profile-stage=inc,wmc,fc
```

## Benchmark

From the companion `problog-benchmark` checkout on `CAV-INC`:

```bash
SOUFFLE_BIN=/path/to/inc-artifact-ae/build/src/souffle \
python3 benchmarks/side_channel/cli/side_channel_inc.py <command> ...
```

For the current AE subset, run cases `P13` through `P20`.

## Verification

```bash
ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}
cmake --build build --target check-regression
```

## Documentation

- [docs/INDEX.md](docs/INDEX.md): reading order.
- [docs/USAGE.md](docs/USAGE.md): compiler/runtime interface.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): control-flow map.
- [docs/TESTING.md](docs/TESTING.md): regression checks.
- [docs/SECURITY.md](docs/SECURITY.md): dependency and artifact hygiene.
- [docs/topics/evaluation/README.artifact.inc.md](docs/topics/evaluation/README.artifact.inc.md): AE benchmark workflow.
