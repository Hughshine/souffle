# Souffle (Local Research Fork)

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)


This repo extends upstream Souffle with a probabilistic pipeline and online
incremental evaluation. It focuses on the online compiler path and adds
DRed-like incremental updates, derivation-graph-based inference, and rewrite
prototypes.

## Status
- Active entry point for this fork; keep high-level and link to detailed docs.

## Audience
- Researchers and engineers working on probabilistic Datalog evaluation.
- Contributors modifying the online incremental compiler and rewrite pipeline.

## Scope and Defaults
- Online compilation is the default; `--online` is optional.
- The legacy `--inc` backend is removed.
- No interpreter path; `souffle file.dl` defaults to compile-only `-o <basename>`.
- Rewrite runs only in full-mode runs; if `--rewrite` is enabled, the incremental CLI is disabled after the full run.
- `--setmode full` maps to `full-hard`; `full-soft` is optional.

## Quickstart

### 1) Install dependencies (scripts used in CI)
- Ubuntu: `sudo sh/setup/install_ubuntu_deps.sh`
- macOS Intel: `sh/setup/install_macos_deps.sh`
- macOS Apple Silicon: `sh/setup/install_macos_arm_deps.sh`

### 2) Build
```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

### 3) Run a minimal example
```bash
SOUFFLE_BIN=./build/src/souffle examples/running_example/run.sh
```

### 4) Verify (style)
```bash
sh/run_test_format.sh
```
See `docs/TESTING.md` for test status and alternative verification paths.

## Configuration
- Copy `.env.example` to your own environment file and export variables in your
  shell. The repo does not auto-load `.env`.
- `SOUFFLE_BIN`, `SOUFFLE_COMPILE_OPTS`, and `SOUFFLE_RUN_OPTS` are used by
  example scripts.

## Documentation
- `CONTRIBUTING.md`: contributor workflow and review checklist.
- `AGENTS.md`: Codex constraints and verification expectations.
- `docs/ARCHITECTURE.md`: high-level system design.
- `docs/project/README.md`: project-level module map, fork delta, and maintenance invariants.
- `docs/project/PROBLOG_EXTENSION_STACK.md`: detailed ProbLog extension implementation from driver/parser through runtime pipeline.
- `docs/TESTING.md`: verification strategy and CI command sources.
- `docs/RUNBOOK.md`: run/rollback/troubleshooting guide.
- `docs/SECURITY.md`: data handling and dependency hygiene.
- `docs/USAGE.md`: program syntax, CLI, and runtime options.
- `docs/process/README.git.md`: commit hygiene and message conventions.
- `docs/INDEX.md`: index of research notes and evaluation docs.
- `docs/topics/README.md`: map of current topic docs (pipeline/rewrite/backends/profiling/eval).
- `docs/design/README.md`: design proposals not fully implemented.
- `docs/historical/README.md`: archived historical notes.

## FAQ / Common Issues
- ctest fails: this fork intentionally disables `ctest`; see `docs/TESTING.md`.
- Need rewrite or incremental details: start at `docs/INDEX.md`.
- Commit hygiene: see `docs/process/README.git.md` for what to include and exclude.

## Related commits
- `UNCOMMITTED` — docs(project): add detailed ProbLog extension stack guide and link it from README
- `UNCOMMITTED` — docs(repo): add project-level docs and move commit hygiene guide under docs/process
- `812ea4081` — docs(repo): refine README narratives
