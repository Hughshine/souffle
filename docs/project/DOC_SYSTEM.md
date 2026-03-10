# Documentation System

This file defines the documentation architecture for the repository. The goal is
to separate current behavior, active research, proposals, and historical logs so
new information has an obvious home.

## Layers
- Layer 1: entry surfaces
  - `README.md`, `CONTRIBUTING.md`, `AGENTS.md`, `docs/INDEX.md`
- Layer 2: operational docs
  - `docs/USAGE.md`, `docs/TESTING.md`, `docs/RUNBOOK.md`, `docs/SECURITY.md`,
    `docs/process/*`
- Layer 3: project model
  - `docs/ARCHITECTURE.md`, `docs/project/*`
- Layer 4: current implementation and semantics
  - `docs/topics/*`
- Layer 5: active research and curated current findings
  - `docs/research/*`
- Layer 6: design proposals and unimplemented ideas
  - `docs/design/*`
- Layer 7: historical records and curated archives
  - `docs/historical/*`, `archive/*`
- Layer 8: local-only raw artifacts
  - run directories, generated dumps, raw TSV/JSON outputs under local ignored
    locations such as `problog-benchmark/runs/` and `/tmp`

## What Each Layer Owns
- Entry surfaces tell readers where to start.
- Operational docs define how to build, run, test, and maintain the repo.
- Project docs explain module boundaries, ownership, and documentation policy.
- Topic docs explain current implementation behavior.
- Research docs capture trusted current conclusions, experiment protocol,
  provenance pitfalls, and active open questions.
- Design docs hold proposals that are not yet current behavior.
- Historical and archive docs hold superseded notes or curated dated snapshots.
- Local artifacts are evidence, not source of truth.

## Placement Rules
- If a change affects how users run the binary, update Layer 2.
- If a change affects module boundaries or ownership, update Layer 3.
- If a change affects current implementation behavior, update the owning topic
  doc in Layer 4.
- If an experiment changes a trusted conclusion, benchmark ranking, or
  provenance rule, update Layer 5.
- If content is proposal-only, keep it in Layer 6 even if it is actively being
  discussed.
- If content is a dated snapshot or superseded note, move it to Layer 7.
- Do not commit raw logs or generated outputs just to preserve context; distill
  the stable conclusion into Layers 4 or 5 and keep raw evidence local.

## Research-Specific Rules
- `docs/research/README.protocol.md` owns experiment validity and provenance
  rules.
- `docs/research/README.memory.md` owns the session-open/session-close rules for
  long-running experimental work.
- `docs/research/README.rewrite.status.md` owns the compact trusted reading of
  current rewrite results.
- Deep algorithmic details still belong in `docs/topics/*`; research docs should
  summarize conclusions and link down rather than duplicate implementation text.

## Naming and Linking Rules
- Every top-level doc subtree should have a landing `README.md` or equivalent
  map.
- `docs/INDEX.md` is the global directory of record for current doc locations.
- Prefer linking to an owning document rather than copying the same rule into
  multiple places.

## Root Directory Policy
- Keep root minimal for discoverability.
- Keep `AGENTS.md` at root because agent tooling expects it there.
- Keep workflow and hygiene rules under `docs/process/`.
- Keep active research synthesis under `docs/research/`, not in root-level
  scratch files.

## Source references
- [README.md](README.md)
- [AGENTS.md](AGENTS.md)
- [docs/INDEX.md](docs/INDEX.md)
- [docs/research/README.md](docs/research/README.md)
- [archive/README.md](archive/README.md)

## Related commits
- `UNCOMMITTED` — docs(system): add explicit research layer and local-artifact policy
- `UNCOMMITTED` — docs(project): define repository-wide doc architecture and placement rules
