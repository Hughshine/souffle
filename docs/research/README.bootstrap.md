# Codex Bootstrap

## Source references
- [AGENTS.md](AGENTS.md)
- [README.md](README.md)
- [docs/INDEX.md](docs/INDEX.md)
- [docs/project/PROBLOG_EXTENSION_STACK.md](docs/project/PROBLOG_EXTENSION_STACK.md)
- [docs/research/README.protocol.md](docs/research/README.protocol.md)
- [docs/research/README.rewrite.status.md](docs/research/README.rewrite.status.md)
- [problog-benchmark/side_channel_full.py](problog-benchmark/side_channel_full.py)
- [problog-benchmark/side_channel_inc.py](problog-benchmark/side_channel_inc.py)
- [problog-benchmark/.worktree/full-artifact/taint_full.py](problog-benchmark/.worktree/full-artifact/taint_full.py)


This file is the Codex-specific startup pack for the repository. A new session
should be able to read this file and quickly recover the project model, the
active experimental landscape, and the trusted domain-specific constraints.

## Status
- Active bootstrap entry for Codex and other coding agents.

## Scope
- Fast session startup.
- Domain-specific understanding for the probabilistic/incremental fork.
- Pointers to the owning docs for deeper implementation, protocol, and current
  trusted conclusions.

## What This Project Is
- A local research fork of Souffle.
- The fork extends upstream Souffle with:
  - a probabilistic derivation-graph pipeline
  - full-mode probabilistic evaluation
  - online incremental evaluation
  - rewrite prototypes over derivation graphs
- The main runtime logic lives in:
  - driver/CLI plumbing
  - derivation graph construction and pruning
  - forward compilation / WMC backends
  - incremental update routing
  - experimental rewrite paths

If you need the end-to-end implementation stack, the owning doc is:
- [PROBLOG_EXTENSION_STACK.md](/home/hugh/research/datalog/souffle/docs/project/PROBLOG_EXTENSION_STACK.md)

## Core Domain Model
- `full` mode:
  - build derivation graph
  - prune
  - optionally rewrite
  - do forward compilation / WMC
- `incremental` mode:
  - online CLI / setmode path
  - compare `inc-naive` and `inc-regional` against `full`
- rewrite variants:
  - `no rewrite`
  - legacy `--rewrite`
  - `--implicit-rewrite`
  - `--implicit-iterate-split-rewrite`
- taint analysis is a chained multi-stage benchmark:
  - `cipt-cg-dlog`
  - `pre-dlog`
  - `typefilter-dlog`
  - `pt-obj-dlog`
  - `taint-lim-dlog`

Important distinction:
- stage-local compare:
  - compare multiple variants on the same matched stage input
  - useful for profiling and local correctness
- full-chain compare:
  - run the entire taint stage chain
  - only this gives true per-case end-to-end totals

## Active Experiment Families
- `side_channel_full`
  - script: [side_channel_full.py](/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_full.py)
  - one-stage full-mode benchmark
  - canonical place for full `no rewrite` / legacy rewrite / implicit rewrite comparisons
- `side_channel_inc`
  - script: [side_channel_inc.py](/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py)
  - two-turn incremental benchmark
  - canonical place for `inc-naive` vs `inc-regional` vs `full`
- `taint`
  - script: [taint_full.py](/home/hugh/research/datalog/souffle/problog-benchmark/.worktree/full-artifact/taint_full.py)
  - chained multi-stage benchmark
  - current sampled bundles are `v1` and `v2`
- derivation JSON benchmarks
  - standalone structural profiling / rewrite experiments
  - useful for rewrite diagnosis, but not a substitute for end-to-end probability validation

## Trusted Current Reading
The compact current status lives here:
- [README.rewrite.status.md](/home/hugh/research/datalog/souffle/docs/research/README.rewrite.status.md)

As of the latest trusted reading:
- legacy `--rewrite` is end-to-end correct on checked `side_channel_full` cases
- `--implicit-rewrite` is also end-to-end correct on checked cases and is the
  current best experimental rewrite default
- `--implicit-iterate-split-rewrite` is correct on checked cases, but still not
  consistently better than non-iterative implicit rewrite
- on taint full-chain totals:
  - implicit variants are often much faster than legacy rewrite
  - but they also push many sampled `v2` cases below `30s`, so they no longer
    preserve the original benchmark calibration goal

## Provenance Rules You Must Respect
- Do not compare runs across mixed artifact sets.
- For taint, do not mix:
  - `.worktree/full-artifact/taint_full/*`
  - and sampled bundle inputs under `problog-benchmark/taint_datasets/*`
  unless they are proven to come from the same generated set.
- Treat stage-local taint results as stage-local only.
- Raw graph-size comparisons are not end-to-end correctness evidence.

The owning protocol doc is:
- [README.protocol.md](/home/hugh/research/datalog/souffle/docs/research/README.protocol.md)

## New Session Read Order
For a new Codex session, use this minimum order:
1. [AGENTS.md](/home/hugh/research/datalog/souffle/AGENTS.md)
2. [README.bootstrap.md](/home/hugh/research/datalog/souffle/docs/research/README.bootstrap.md)
3. [README.protocol.md](/home/hugh/research/datalog/souffle/docs/research/README.protocol.md)
4. [README.rewrite.status.md](/home/hugh/research/datalog/souffle/docs/research/README.rewrite.status.md)
5. The owning topic doc for the subsystem being changed

Task-specific follow-up:
- rewrite / full probabilistic work:
  - [README.rewrite.impl.md](/home/hugh/research/datalog/souffle/docs/topics/rewrite/README.rewrite.impl.md)
- incremental pipeline work:
  - `docs/topics/pipeline/README.inc.region.md`
  - `docs/topics/pipeline/README.dred.md`
- broad implementation context:
  - [PROBLOG_EXTENSION_STACK.md](/home/hugh/research/datalog/souffle/docs/project/PROBLOG_EXTENSION_STACK.md)

## What Codex Should Record Aggressively
- correctness mismatches
- provenance mistakes
- changes in which variant is currently preferred
- negative results that rule out a direction
- timing-composition findings that change optimization priorities

## Session-Close Rule
Before ending a meaningful session:
- update the owning topic doc if implementation behavior changed
- update `docs/research/*` if trusted conclusions or protocol changed
- keep raw run artifacts local

## Related commits
- `UNCOMMITTED` — docs(codex): add Codex bootstrap pack for new sessions
