# Souffle Probabilistic Artifact

This branch packages the full-mode probabilistic Souffle artifact used for
rewrite evaluation.  The artifact-facing path is intentionally narrow: build the
repo, generate the benchmark `compute` binaries with the repo-built `souffle`,
then run generated binaries with deterministic analysis and the default rewrite
dispatcher.

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [docs/USAGE.md](docs/USAGE.md)
- [docs/topics/rewrite/README.rewrite.impl.md](docs/topics/rewrite/README.rewrite.impl.md)
- [docs/TESTING.md](docs/TESTING.md)

## Artifact Scope
- Full-mode probabilistic evaluation with derivation graphs, pruning,
  component-wise forward compilation, and BDD weighted model counting.
- The optimized artifact command uses bare `--rewrite`.  Do not pass
  `--split-mode` in artifact runs; explicit split controls are diagnostic only
  and bypass the smart default dispatcher.
- `--det-opt` is part of the artifact command.  It enables deterministic
  relation analysis and graph gating used by the packaged benchmarks.
- `--knowledge bdd` is the default backend and the expected artifact backend.

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

Plain comparison runs omit only `--rewrite`:

```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --logfile ae-plain
```

## Correctness Checks
- Side-channel and taint benchmark comparisons should match exactly.
- Symbolization comparisons use the same output key set and allow an absolute
  probability difference of at most `1e-8`.  This tolerance is for rare
  last-digit output-rounding boundary cases, not for semantic drift.

## Documentation
- [docs/USAGE.md](docs/USAGE.md): user-facing compiler/runtime usage.
- [docs/topics/rewrite/README.rewrite.impl.md](docs/topics/rewrite/README.rewrite.impl.md):
  artifact rewrite behavior and diagnostics.
- [docs/topics/runtime/README.flag.md](docs/topics/runtime/README.flag.md):
  detailed flag reference, with artifact defaults separated from diagnostics.
- [docs/TESTING.md](docs/TESTING.md): verification commands.
- [docs/INDEX.md](docs/INDEX.md): maintained documentation index.

## Verification
- Build after C++ changes: `cmake --build build -j${JOBS}`.
- Run regression tests when probabilistic behavior changes:
  `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`.
- Run the basic example smoke:
  `SOUFFLE_BIN=./build/src/souffle examples/running_example/run.sh`.

## Related commits
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
- `2d1434976` — feat(problog): add explicit rewrite flag
