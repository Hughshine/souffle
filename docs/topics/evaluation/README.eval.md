# Full-Mode Evaluation Workflow

## Scope
This compiler branch is paired with the companion benchmark artifact:
- branch: `CAV-FULL`
- current commit: `76b4799`

The benchmark tree is intentionally not vendored into this compiler branch.
Run benchmark scripts from the companion artifact and point them at the
repo-built `souffle` from this branch.

## Build Compiler
```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
export PATH="$(pwd)/build/src:$PATH"
```

## Generated Binary Command Shape
Plain run:
```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --logfile ae-plain
```

Optimized run:
```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --rewrite --logfile ae-rewrite
```

## Outputs To Inspect
- `facts.prob`
- `<logfile>.json` under the output directory
- stdout timing lines for create-graph, pruning, rewrite, FC, and WMC stages

## Correctness Policy
- Side-channel and taint comparisons should match exactly.
- Symbolization comparisons require identical output keys and allow absolute
  probability difference up to `1e-8` for rare output-rounding boundaries.

## Source references
- [../../../README.md](../../../README.md)
- [../../USAGE.md](../../USAGE.md)
- [../rewrite/README.rewrite.impl.md](../rewrite/README.rewrite.impl.md)
- [../../../src/problog/Pipeline.cpp](../../../src/problog/Pipeline.cpp)

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
