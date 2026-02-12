# Project Documentation

Project-level docs answer three questions that topic docs do not:
- what the whole codebase is made of,
- what this fork adds on top of Souffle,
- and what maintainers must preserve when changing modules.

## Contents
- `docs/project/MODULES.md`: complete module map (core + fork extensions).
- `docs/project/FORK_DELTA.md`: fork-owned additions and contributions.
- `docs/project/MAINTENANCE.md`: invariants, risks, and change checklists.
- `docs/project/PROBLOG_EXTENSION_STACK.md`: end-to-end ProbLog extension implementation (driver -> parser -> AST -> AST2RAM -> RAM -> synthesiser -> pipeline/CLI).
- `docs/project/DOC_SYSTEM.md`: documentation architecture and placement rules.

## How To Use
- Start here when onboarding a new contributor.
- Use `docs/INDEX.md` for doc priority and recency tags.
- Use topic docs under `docs/topics/` for implementation details.

## Source references
- [docs/INDEX.md](docs/INDEX.md)
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- [src/MainDriver.cpp](src/MainDriver.cpp)

## Related commits
- `UNCOMMITTED` — docs(project): add project-level doc map for module ownership and maintenance
- `UNCOMMITTED` — docs(project): add detailed ProbLog extension stack implementation guide
