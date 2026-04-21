# Documentation System (Recommended Structure)

This is the documentation architecture for this repository.

## Layers
- Layer 1 (entry): `README.md`, `CONTRIBUTING.md`, `AGENTS.md`.
- Layer 2 (operations): `docs/USAGE.md`, `docs/TESTING.md`, `docs/RUNBOOK.md`, `docs/SECURITY.md`, `docs/process/*`.
- Layer 3 (project-level): `docs/project/*` for module map, ownership, and maintenance invariants.
- Layer 4 (topic deep dives): `docs/topics/*` for implementation details by subsystem.
- Layer 5 (experiment records): keep raw run logs out of the AE branch.

## Placement Rules
- If a change affects how users run the binary: update Layer 2.
- If a change affects module boundaries or ownership: update `docs/project/MODULES.md` and `docs/project/FORK_DELTA.md`.
- If a change affects algorithm details: update one topic doc in Layer 4.
- If content is proposal-only or an old run snapshot, keep it out of the AE
  branch unless it is needed to reproduce the packaged artifact.

## Root Directory Policy
- Keep root minimal for discoverability.
- Keep `AGENTS.md` at root (agent discovery and execution constraints depend on this location).
- Keep process docs under `docs/process/` instead of root-level miscellaneous READMEs.

## Source references
- [README.md](../../README.md)
- [AGENTS.md](../../AGENTS.md)
- [docs/INDEX.md](../INDEX.md)
- [docs/TESTING.md](../TESTING.md)

## Related commits
- `ef4b7796c` — chore(artifact): prune AE rewrite surface
