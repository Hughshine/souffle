# Maintenance Notes (Module-Level)

This file records the invariants most likely to cause regressions if violated.

## Critical Invariants
- Online path is the default compilation path in this fork (`--online` accepted but redundant).
- Rewrite is full-mode only; rewrite-enabled full run disables subsequent incremental CLI flow in that execution.
- `full-only` compile mode disables incremental code paths and changes merge behavior expectations.
- `setmode full` maps to `full-hard`; aliases `inc/incr/incremental` map to `inc-naive`.
- `elastic` mode is accepted by option parsing but not implemented in runtime CLI.
- `.prob` files are line-aligned with `.facts`; mismatches corrupt probabilistic semantics.
- DRed/profile details require compile-time and runtime flags to match (`--profile`, `--dred-profile`, runtime profile flags).

## High-Risk Modules and What To Check

| Module | Risk | Minimum verification |
| --- | --- | --- |
| `src/ast2ram/online/*` | Wrong delta relation generation breaks incremental correctness | Build + run example + one side-channel incremental smoke run |
| `src/include/souffle/problog/DerivationGraph.h` | Incorrect prune/delta handling can silently bias probabilities | Build + compare `facts.prob` across modes on a representative case |
| `src/include/souffle/problog/ForwardCompilation.h` | FC/WMC performance and correctness regressions | Build + full-mode run (det-opt default on; optional explicit `--det-opt`) + inspect FC/WMC stage logs |
| `src/include/souffle/problog/RegionalIncremental.h` | `inc-regional` regressions on insert/delete boundaries | Build + `--compare-all` incremental run + check per-turn JSON consistency |
| `src/include/souffle/cli/Cli.h` | Mode/output naming and batch-command semantics drift | Build + scripted `insert/delete/commit` replay and output file checks |
| `src/include/souffle/CompiledOptions.h` and `docs/USAGE.md` | Option docs drift from runtime behavior | Verify `./compute -h` and update docs in same change |

## Documentation Rule For Code Changes
- Behavior change in runtime flags/CLI: update `docs/USAGE.md` and link from `docs/INDEX.md`.
- Pipeline or algorithm change: update relevant `docs/topics/*` plus this file if invariants changed.
- Workflow change (build/test/run): update `README.md`, `CONTRIBUTING.md`, `docs/TESTING.md`, `docs/RUNBOOK.md`.
- Experiment-only findings: archive in `archive/YYYY-MM-DD/README.md` and keep details out of core docs.

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [src/include/souffle/problog/RegionalIncremental.h](src/include/souffle/problog/RegionalIncremental.h)
- [docs/TESTING.md](docs/TESTING.md)

## Related commits
- `UNCOMMITTED` — docs(project): add module-level maintenance invariants and regression checks
