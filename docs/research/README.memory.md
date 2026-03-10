# Research Memory

## Source references
- [AGENTS.md](AGENTS.md)
- [docs/INDEX.md](docs/INDEX.md)
- [docs/project/DOC_SYSTEM.md](docs/project/DOC_SYSTEM.md)
- [docs/research/README.protocol.md](docs/research/README.protocol.md)
- [docs/research/README.rewrite.status.md](docs/research/README.rewrite.status.md)


This file internalizes the useful part of the external SER workflow for this
repository. It is written for Codex work, not Claude tooling hooks.

## Status
- Active session-memory and documentation-maintenance guide for experiment-heavy
  work.

## Scope
- How to reopen context, what to preserve across long rewrite/evaluation
  investigations, and where to write conclusions.

## Memory Layers
- `docs/topics/*`: current implementation behavior and semantics.
- `docs/research/*`: active hypotheses, trusted current conclusions, provenance
  rules, and recurring pitfalls.
- `docs/design/*`: proposals that are not yet current behavior.
- `docs/historical/*` and `archive/`: archived snapshots and curated historical
  context.
- local run directories: raw evidence only; keep them local and distill stable
  conclusions back into tracked docs.

## Session Open Checklist
When resuming a long-running line of work:
1. Read `docs/INDEX.md` for the current map.
2. Read `docs/project/DOC_SYSTEM.md` to place any new information correctly.
3. Read `docs/research/README.md` and the relevant status/protocol docs.
4. Read the topic doc that owns the behavior you are changing.
5. Read design or historical docs only if the current docs do not answer the
   question.

## Session Close Checklist
Before ending a meaningful experimental session:
1. Update the relevant topic doc if behavior or implementation changed.
2. Update `docs/research/README.rewrite.status.md` if trusted conclusions,
   current rankings, or pitfalls changed.
3. Update `docs/research/README.protocol.md` if a new provenance rule or invalid
   experiment pattern was discovered.
4. Keep raw artifacts local; do not commit them unless explicitly requested.

## What To Record Aggressively
- correctness mismatches and later fixes
- benchmark provenance mistakes
- comparisons that change which pipeline variant should be preferred
- trusted negative results that rule out a direction
- verification coverage and any missing checks

## Related commits
- `UNCOMMITTED` — docs(research): internalize Codex-oriented research memory workflow

