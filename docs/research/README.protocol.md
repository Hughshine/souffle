# Research Protocol

## Source references
- [docs/process/README.git.md](docs/process/README.git.md)
- [docs/topics/evaluation/README.eval.md](docs/topics/evaluation/README.eval.md)
- [docs/topics/evaluation/README.eval.inc.md](docs/topics/evaluation/README.eval.inc.md)
- [docs/topics/rewrite/README.rewrite.impl.md](docs/topics/rewrite/README.rewrite.impl.md)
- [problog-benchmark/side_channel_full.py](problog-benchmark/side_channel_full.py)
- [problog-benchmark/side_channel_inc.py](problog-benchmark/side_channel_inc.py)
- [problog-benchmark/.worktree/full-artifact/taint_full.py](problog-benchmark/.worktree/full-artifact/taint_full.py)


This file defines the minimum protocol for trustworthy experiments in this
repository. The goal is to keep active research reproducible without checking in
raw logs and generated artifacts.

## Status
- Active protocol for benchmark provenance, validity, and documentation updates.

## Scope
- Full-mode, incremental, and taint benchmark work.
- Current research comparisons and local experiment runs.

## Current Incremental Trust Boundary
- The current `inc-artifact` branch should be treated as having two trusted
  incremental repairs:
  - non-recursive mixed-update timestamp repair via explicit
    `@post_delete_*` views
  - staged `sem=full + fc=inc-*` repair via stable fact `semanticFactId`
    assignment before post-prune diff/remap
- Current validation status:
  - deterministic and probabilistic tiny repros agree with `full`
  - fresh all-case core sweep (`P1`, `P3-P20`; one `Δ=1.0%` five-alpha mixed
    grid; `sets=1`, `delta-runs=1`, `--det-opt`, `--enable-inc-reord`,
    `--no-compare-full-inc`) completed with:
    - `95/95` aggregate JSONs reporting `compare.ok = true`
    - `95/95` aggregate JSONs reporting
      `inc_naive_final_vs_full_final.ok = true`
  - fresh staged all-case sweep on the same workload completed with:
    - `95/95` staged aggregate JSONs checked
    - `0` staged `full_inc_*` mismatches
  - after a clean rebuild, targeted staged reruns on
    `P17/P20 × {mix-d50-i50,mix-d0-i100}` still matched exactly
  - maintained regression suite passes cleanly (`19/19`)
- Remaining artifact risk is now workload/provenance curation for paper-facing
  runtime claims rather than a known open core or staged correctness bug.

## Required Record For Every Trusted Experiment
Record these facts in your working notes before treating a result as real:
- hypothesis or question being tested
- repository revision or dirty-worktree note
- binary path actually executed
- benchmark family (`side_channel_full`, `side_channel_inc`, `taint`, or
  standalone derivation JSON benchmark)
- dataset, case, and stage if applicable
- exact input directory
- exact output directory
- exact command line or script invocation
- comparison target (`no rewrite`, `--rewrite`, `--implicit-rewrite`,
  `--implicit-iterate-split-rewrite`, `inc-naive`, `inc-regional`, and so on)
- correctness criterion (`facts.prob`, per-query `.csv`, stage totals,
  `FC_WMC_HYBRID`, etc.)
- timing label being compared (wall time, JSON turn total, stage time, rewrite
  phase time)

## Validity Rules
- A comparison is only valid if the executed binary, input directory, and output
  directory belong to the same generated artifact set.
- For taint, do not mix `.worktree/full-artifact/taint_full/*` binaries or
  inputs with sampled bundle inputs under `problog-benchmark/taint_datasets/`
  unless you have proven they are from the same generated set.
- If provenance is mixed or uncertain, mark the run invalid and do not reuse its
  numbers as evidence.
- Keep invalid runs visible in notes so the same pitfall is not repeated.

## Comparison Rules
- Compare like with like:
  - same binary
  - same input data
  - same output target relation set
  - same interpretation of timing fields
- For incremental side-channel timing comparisons:
  - do not compare top-level mode `elapsed_s`
  - compare per-turn `log.stages.turns[1].time_seconds`
  - `turn 1` is the shared initial full-hard setup run
  - `turn 2` is the actual delta-step runtime:
    - `FULL-HARD` for `full`
    - `INC` for `inc-naive` / `inc-regional`
- For taint stage-local comparisons, it is valid to compare variants on a shared
  stage input only if that stage input was produced by a matched chain from the
  same generated base. Treat those results as stage-local only; they do not by
  themselves prove full-pipeline equivalence.
- For full-mode correctness checks, prefer diffing `facts.prob` and any
  query-specific `.csv` outputs that are expected to change.
- For rewrite-only derivation benchmarks, do not present graph-size comparisons
  as end-to-end probability validation.
- When a result changes a design decision, rerun the comparison at least once on
  the same setup before documenting it as trusted.

## Where To Record Findings
- Implementation behavior: update the relevant `docs/topics/*` file.
- Trusted active research conclusion or pitfall: update `docs/research/*`.
- Proposal only, not implemented: update `docs/design/*`.
- Raw logs, generated tables, dumpjson output, and ad hoc run directories stay
  local per `docs/process/README.git.md`.

## Related commits
- `UNCOMMITTED` — docs(research): codify benchmark provenance and experiment validity rules
