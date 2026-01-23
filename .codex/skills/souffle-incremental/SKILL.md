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
5. Link to existing docs instead of duplicating: `README.eval.inc.md`, `README.inc.region.md`, and `problog-benchmark/README.side-channel-inc.md`.
6. Always compile with the repo-built Soufflé binary: set `PATH=/home/hugh/research/datalog/souffle/build/src:$PATH` so `side_channel_inc.py compile` does not pick up `/usr/local/bin/souffle`.
7. Use `side_channel_inc.py compile --jobs <N>` (e.g., `--jobs $(nproc || sysctl -n hw.ncpu || echo 2)`) for parallel per-case compilation.
8. Always keep Skills and docs up to date; if they diverge, update both.
9. Docs sync checkpoint: if incremental behavior or outputs change, ask whether to
   update docs; when updating, keep `## Source references` as markdown links and
   add a Related commits section (latest 3).
10. Inc experiment playbook (script-driven):
    - Generate inputs: `python3 problog-benchmark/side_channel_inc.py --base-dir <base> generate --cases 1-20 --cleanup`
    - (Optional) Strengthen: `... strengthen --cases 12-20 --augment-derivations 200`
    - Deltas: `... delta --cases 1-20 --cleanup --delta-cluster assign --seed <N> --change-spec inc0p1=0.001,inc0p3=0.003,inc0p5=0.005`
    - Compile: `... compile --cases 1-20 --jobs <N> --timeout 300 --souffle-arg=<extra>`
      (script always passes `souffle --online`; do **not** add `--full-only` for incremental runs)
    - Run: `... run --cases 1-20 --delta-labels inc0p1,inc0p3,inc0p5 --delta-samples 1 --compare-all --run-arg=--det-opt`
      (with `--compare-all`, any `--setmode` passed via `--run-arg` is ignored)
    - Collect: `... collect --cases 1-20` → `results-souffle-inc.tsv` + per-delta JSON.
11. Script CLI behaviors to remember:
    - `generate` honors `--rule-set` (trimmed/trimmed_plus/full) and `--force-smt`.
    - `delta` controls size via `--change-spec`/`--change-cap` and sampling via `--delta-cluster` + `--sets` + `--seed`.
    - `run` supports `--delta-root`, `--delta-shuffle`, and `--delta-seed` for delta selection.
    - `compile` passes extra flags via `--souffle-arg` (repeatable); `run` uses `--run-arg`.
    See `references/incremental-context.md` for the full CLI map.
12. Output naming expectations (script coupling):
    - Baseline: `output/facts.full.prob`, `output/facts.inc.prob`
    - Per-delta: `output/delta-<label>-<sample>-<mode>-fact-iterN-*.prob`

## Related commits
- `e214cd028` — docs(repo): refresh incremental docs and index
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
- `619e52197` — fix(inc): track explicit deletes and log deltas
