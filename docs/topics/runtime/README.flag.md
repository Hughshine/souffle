# Flags Reference

## Source references
- [../../USAGE.md](../../USAGE.md)
- [../../../src/MainDriver.cpp](../../../src/MainDriver.cpp)
- [../../../src/include/souffle/CompiledOptions.h](../../../src/include/souffle/CompiledOptions.h)
- [../../../src/problog/Pipeline.cpp](../../../src/problog/Pipeline.cpp)

## Scope
- Compiler flags apply to the `souffle` executable and control code generation.
- Runtime flags apply to generated `./compute` binaries.
- AE-facing instructions expose only the stable runtime surface.

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

Plain comparison runs omit only `--rewrite`.

## Rewrite Dispatch
Bare `--rewrite` chooses a policy in [Pipeline.cpp](../../../src/problog/Pipeline.cpp):
- If any rule has probability different from `1.0`, use implicit-split rewrite
  with the local split policy.
- If no rule has a probabilistic weight, use graph rewrite with no split.

The classification is intentionally rule-based.  It does not inspect
probabilistic `.prob` input facts.

## Hidden Diagnostics
Generated binaries still accept selected dump/profile/tuning flags for local
debugging.  They are intentionally omitted from generated help and from AE
commands because artifact results should be reproduced with the stable surface
above.

## Correctness Policy
- Side-channel and taint checks are exact.
- Symbolization checks require identical output keys and allow absolute
  probability differences up to `1e-8`.  This tolerance covers rare final-digit
  output-rounding boundaries only.
