# Runbook

## Source references
- [README.md](../README.md)
- [docs/USAGE.md](USAGE.md)
- [docs/TESTING.md](TESTING.md)
- [src/MainDriver.cpp](../src/MainDriver.cpp)
- [src/include/souffle/CompiledOptions.h](../src/include/souffle/CompiledOptions.h)

## Start / Build
- Follow the Quickstart in [README.md](../README.md).
- CUDD is required for the BDD backend used by the artifact.

## Run Artifact Commands
- Plain generated-binary run:
  `./compute -F <facts-dir> -D <output-dir> --det-opt --logfile ae-plain`
- Optimized generated-binary run:
  `./compute -F <facts-dir> -D <output-dir> --det-opt --rewrite --logfile ae-rewrite`
- Companion benchmark scripts live in `CAV-FULL` commit `76b4799`.

## Logs and Metrics
- `--logfile <name>` writes JSON reports to the output directory (`-D`).
- Hidden dump/profile flags exist for local diagnosis but are not required for
  AE reproduction.

## Troubleshooting
- `ctest` fails: see [docs/TESTING.md](TESTING.md).
- `souffle` not found: set `PATH` to include `build/src` or call
  `./build/src/souffle` explicitly.
- Missing BDD backend: ensure CUDD is available in the build environment.
- Large outputs/logs: avoid committing generated artifacts.

## Common Checks
- Compare `facts.prob` outputs across plain and rewrite runs.
- Inspect stdout timing lines and JSON logs for hot stages before tuning.

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
