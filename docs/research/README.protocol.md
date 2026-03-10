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
