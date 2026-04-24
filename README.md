# Incremental Probabilistic Souffle Artifact

This branch contains the Souffle compiler and generated runtime used by the
incremental probabilistic inference artifact. It compiles a Datalog program
into an online binary. That binary computes a baseline from a fact directory,
then applies incremental turns through the online CLI.

The benchmark inputs and orchestration scripts live in the companion
`problog-benchmark` artifact on branch `CAV-INC`.

## Artifact Workflow

Build the compiler:

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

Generate a benchmark binary:

```bash
./build/src/souffle -F <facts-dir> -D <output-dir> compute.souffle.dl -o compute
```

Run an incremental session:

```bash
./compute -F <facts-dir> -D <output-dir> --setmode inc-naive
insert 0.3::edge(1,2)
delete edge(3,4)
commit
q
```

Or use the companion benchmark helper:

```bash
sh/run_artifact_inc.sh
```

## Inputs and Outputs

Input facts live in `<facts-dir>` as `<relation>.facts`. Optional
`<relation>.prob` files align line-for-line with the facts. Missing probability
files mean probability `1.0`.

The generated binary writes output tuple probabilities to `facts.prob` in
`<output-dir>`. Incremental runs also emit per-turn probability snapshots and
JSON timing logs whose base name comes from `--logfile`.

## Expected Comparisons

Artifact comparisons use `full-hard` as the oracle and compare it against
incremental modes:

- `inc-naive`
- `inc-regional`
- staged mixed modes such as `sem=full fc=inc-regional`

The compared outputs must have the same tuple keys. Probability values are
checked with a small absolute tolerance.

## Documentation

- [docs/USAGE.md](docs/USAGE.md): incremental compiler/runtime interface.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): incremental pipeline map.
- [docs/TESTING.md](docs/TESTING.md): build and regression checks.
- [docs/topics/testing/README.regression.md](docs/topics/testing/README.regression.md):
  maintained incremental regression suite.
- [docs/topics/evaluation/README.artifact.inc.md](docs/topics/evaluation/README.artifact.inc.md):
  artifact runner and side-channel workflow.
- [docs/topics/pipeline/README.dred.md](docs/topics/pipeline/README.dred.md):
  delete/rederive mechanics.
- [docs/topics/pipeline/README.inc.region.md](docs/topics/pipeline/README.inc.region.md):
  regional incremental compilation path.
