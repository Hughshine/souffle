# Flags Reference

## Source references
- [../../USAGE.md](../../USAGE.md)
- [../../../src/MainDriver.cpp](../../../src/MainDriver.cpp)
- [../../../src/include/souffle/CompiledOptions.h](../../../src/include/souffle/CompiledOptions.h)
- [../../../src/problog/Pipeline.cpp](../../../src/problog/Pipeline.cpp)

## Scope
- Compiler flags apply to the `souffle` executable and control code generation.
- Runtime flags apply to generated `./compute` binaries.
- Artifact instructions should expose only the stable runtime surface.  Other
  flags are diagnostics and should not appear in artifact benchmark commands.

## Compiler Use
Use the repo-built `souffle` to generate benchmark `compute` binaries.  Benchmark
scripts may pass internal generation options, but AE-facing instructions should
not present those internal options as a stable user surface.

## Artifact Runtime Surface
Generated programs should be run with:
```bash
./compute -F <facts-dir> -D <output-dir> --det-opt --rewrite --logfile ae-run
```

Flags:
- `-F, --facts <DIR>`: input facts/probabilities directory.
- `-D, --output <DIR>`: output directory.
- `-l, --logfile <FILE>`: debugger JSON base name.
- `--det-opt`: deterministic-relation analysis and graph gating.
- `-r, --rewrite`: smart artifact rewrite dispatcher.
- `-k, --knowledge <bdd|sdd>`: backend selector; artifact runs use `bdd`.

Plain comparison runs omit only `--rewrite`.

## Rewrite Dispatch
Bare `--rewrite` chooses a policy in [Pipeline.cpp](../../../src/problog/Pipeline.cpp):
- If any rule has probability different from `1.0`, use implicit-split rewrite
  with the local split policy.
- If no rule has a probabilistic weight, use graph rewrite with no split.

The classification is intentionally rule-based.  It does not inspect
probabilistic `.prob` input facts.

Do not pass `--split-mode` in artifact commands.  An explicit split mode is a
diagnostic override and bypasses the smart dispatcher.

## Diagnostic Runtime Flags
These remain available for debugging or ablation, but are not AE commands:
- `--explicit-rewrite`
- `--implicit-rewrite`
- `--split-mode=<no-split|naive-split>`
- `-d, --derv-only[=<true|false>]`
- `-e, --merge-bi-imp`
- `--prune-extra`
- `-C, --fold-const`
- `--det-force`
- `--post-del`
- `--no-reuse-var-index`
- `--no-single-rand-fast`
- `--force-full-siso-detect`
- `--no-relax-compaction-dirty`
- dump/profile flags such as `--dumpjson`, `--dumpdot`, `--dumpstat`,
  `--dumpconst`, `--fc-profile`, `--profile-wmc`, and `--profile-dep-graph`

`--derv-only --rewrite` does not execute rewrite and should not be used as a
graph-only rewrite benchmark.

## Correctness Policy
- Side-channel and taint checks are exact.
- Symbolization checks require identical output keys and allow absolute
  probability differences up to `1e-8`.  This tolerance covers rare final-digit
  output-rounding boundaries only.

## Related commits
- `f78f1cade` — docs(rewrite): record artifact smoke verification
- `beb581c24` — perf(problog): reduce symbolization rewrite overhead
- `2d1434976` — feat(problog): add explicit rewrite flag
