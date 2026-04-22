# Runbook

This page is the short operational path for a local artifact run.

## Build

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

## Generate

From a benchmark case directory:

```bash
<repo>/build/src/souffle -F <facts-dir> -D <output-dir> compute.souffle.dl -o compute
```

Use the companion `problog-benchmark` artifact for packaged cases.

## Run

Plain:

```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --logfile ae-plain
```

Rewrite:

```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --rewrite --logfile ae-rewrite
```

## Check Results

Compare `facts.prob` from the two output directories. Side-channel and taint
cases should match exactly. Symbolization uses the same tuple keys with
probability tolerance `1e-8`.

Use JSON logs from `--logfile` to inspect timing stages.

## Troubleshooting

- Build failure: rerun CMake configure, then rebuild with `-j1` if the error is
  interleaved.
- Missing CUDD symbols: check the CUDD installation used by CMake.
- Missing generated binary dependencies: keep the CMake build tree available
  when running generated benchmark binaries.
- Regression failure: rerun
  `ctest --test-dir build -L regression --output-on-failure --progress -j1`.

## Source Map

- [../README.md](../README.md): evaluator workflow.
- [USAGE.md](USAGE.md): command-line details.
- [TESTING.md](TESTING.md): validation commands.
