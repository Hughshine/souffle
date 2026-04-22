# Probabilistic Souffle Artifact

This branch contains the Souffle compiler and runtime used by the exact
probabilistic inference artifact. It turns a Datalog program plus fact
probabilities into a generated benchmark binary. That binary computes output
tuple probabilities.

The benchmark inputs and runner scripts are kept in the companion
`problog-benchmark` artifact.

## Evaluator Workflow

Build the compiler:

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

Generate a benchmark binary with the compiler from this build tree:

```bash
./build/src/souffle -F <facts-dir> -D <output-dir> compute.souffle.dl -o compute
```

Run the generated binary in the plain configuration:

```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --logfile ae-plain
```

Run the same binary with rewrite enabled:

```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --rewrite --logfile ae-rewrite
```

## Inputs and Outputs

Input facts live in `<facts-dir>` as `<relation>.facts`. Probabilities use
matching `<relation>.prob` files with line-for-line alignment. A missing
probability file means probability `1.0` for every tuple in that relation.

The generated binary writes tuple probabilities to `facts.prob` in
`<output-dir>`. It also writes a JSON timing log whose base name comes from
`--logfile`.

## Expected Comparisons

Plain and rewrite runs must produce the same output tuple keys. Side-channel
and taint probabilities should match exactly. Symbolization cases allow
absolute probability error up to `1e-8`; this covers rare decimal rounding
boundaries in printed probabilities.

## Documentation

- [docs/USAGE.md](docs/USAGE.md): command-line interface and file formats.
- [docs/LANGUAGE_EXAMPLES.md](docs/LANGUAGE_EXAMPLES.md): small programs
  that show the language features covered by regression tests.
- [docs/TESTING.md](docs/TESTING.md): build and regression checks.
- [docs/RUNBOOK.md](docs/RUNBOOK.md): compact operational guide.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): exact inference pipeline map.
- [docs/INDEX.md](docs/INDEX.md): documentation index.
