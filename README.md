# Souffle Probabilistic Artifact

This branch packages the compiler side of the full-mode probabilistic Souffle
artifact for artifact evaluators. The run path has three steps: build this repo,
generate benchmark `compute` binaries with the repo-built `souffle`, then run
those binaries with deterministic analysis and the rewrite dispatcher.

## Source References
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [docs/USAGE.md](docs/USAGE.md)
- [docs/TESTING.md](docs/TESTING.md)

## Artifact Scope
- Full-mode probabilistic evaluation with derivation graphs, pruning,
  component-wise forward compilation, and BDD weighted model counting.
- Optimized artifact runs use bare `--rewrite`.  The generated binary chooses
  the rewrite implementation internally.
- `--det-opt` is part of the artifact command.  It enables deterministic
  relation analysis and graph gating used by the packaged benchmarks.
- Generated programs use the BDD backend by default.
- Benchmark data and scripts live in the companion benchmark artifact:
  `CAV-FULL` at commit `76b4799`.

## Quickstart
```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

Use the built compiler to generate benchmark binaries, then run generated
programs with:

```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --rewrite --logfile ae-run
```

Plain comparison command:

```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --logfile ae-plain
```

## Correctness Checks
- Side-channel and taint benchmark comparisons should match exactly.
- Symbolization comparisons use the same output key set and allow an absolute
  probability difference of at most `1e-8`.  This tolerance is for rare
  last-digit output-rounding boundary cases.

## Documentation
- [docs/USAGE.md](docs/USAGE.md): user-facing compiler/runtime usage.
- [docs/TESTING.md](docs/TESTING.md): verification commands.
- [docs/RUNBOOK.md](docs/RUNBOOK.md): concise build/run/troubleshooting guide.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): high-level pipeline map.
- [docs/INDEX.md](docs/INDEX.md): maintained documentation index.

## Verification
- Build after C++ changes: `cmake --build build -j${JOBS}`.
- Run regression tests when probabilistic behavior changes:
  `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`.
- Run a generated benchmark binary from the companion `CAV-FULL` artifact with
  the commands above for end-to-end AE validation.
