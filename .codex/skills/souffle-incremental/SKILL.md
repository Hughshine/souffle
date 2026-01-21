---
name: souffle-incremental
description: Work with Souffle incremental CLI implementation and the side-channel incremental benchmark. Use when tasks touch `src/include/souffle/cli/Cli.h`, incremental modes (`--setmode`/`--online`), IncrementalCLI behavior/output naming, or the benchmark runner at `/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py`.
---

# Souffle Incremental

## Overview

Provide a focused workflow for incremental CLI changes and the side-channel incremental benchmark, with a concise code map and coupling notes.

## Workflow

1. Identify scope (CLI, pipeline options, or benchmark tooling) and load `references/incremental-context.md` for the code map and output naming dependencies.
2. For CLI changes, review `src/include/souffle/cli/Cli.h` plus call sites in `src/problog/Pipeline.cpp` and option plumbing in `src/MainDriver.cpp` and `src/include/souffle/CompiledOptions.h`.
3. For benchmark changes, review `/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py` and `/home/hugh/research/datalog/souffle/problog-benchmark/README.side-channel-inc.md` before editing.
4. Preserve `fact-iter*` output naming and CLI command semantics (`insert/delete/commit/q`) because the benchmark runner parses them.
5. Link to existing docs instead of duplicating: `README.eval.inc.md`, `README.inc.region.md`, and the benchmark README.
6. Always compile with the repo-built Soufflé binary: set `PATH=/home/hugh/research/datalog/souffle/build/src:$PATH` so `side_channel_inc.py compile` does not pick up `/usr/local/bin/souffle`.
7. Use `side_channel_inc.py compile --jobs <N>` (e.g., `--jobs $(nproc || sysctl -n hw.ncpu || echo 2)`) for parallel per-case compilation.

## References

- `references/incremental-context.md`
