# Souffle (Local Research Fork)

This repo extends upstream Souffle with a probabilistic pipeline and online
incremental evaluation. It focuses on the online compiler path and adds
DRed-like incremental updates, derivation-graph-based inference, and rewrite
prototypes.

## Audience
- Researchers and engineers working on probabilistic Datalog evaluation.
- Contributors modifying the online incremental compiler and rewrite pipeline.

## Status / Scope
- Online incremental path is the only supported backend; the legacy `--inc`
  backend is removed.
- Online compilation is the default; `--online` is optional and auto-enabled.
- No interpreter mode; `souffle file.dl` defaults to compile-only `-o <basename>`.
- Rewrite runs in full mode only; incremental modes skip rewrite.

## Quickstart

### 1) Install dependencies (scripts used in CI)
- Ubuntu: `sudo sh/setup/install_ubuntu_deps.sh`
- macOS Intel: `sh/setup/install_macos_deps.sh`
- macOS Apple Silicon: `sh/setup/install_macos_arm_deps.sh`

### 2) Build
```
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
```

### 3) Run a minimal example
```
SOUFFLE_BIN=./build/src/souffle examples/running_example/run.sh
```

### 4) Verify (style)
```
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
- `docs/TESTING.md`: verification strategy and CI command sources.
- `docs/RUNBOOK.md`: run/rollback/troubleshooting guide.
- `docs/SECURITY.md`: data handling and dependency hygiene.
- `docs/USAGE.md`: program syntax, CLI, and runtime options.
- `docs/INDEX.md`: index of all research notes and evaluation docs.

## FAQ / Common Issues
- **ctest fails**: this fork intentionally disables `ctest`.
  See `docs/TESTING.md`.
- **Need rewrite/incremental details**: start at `docs/INDEX.md`.
- **Commit hygiene**: see `README.git.md` for what to include and exclude.
