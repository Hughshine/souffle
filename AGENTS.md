# AGENTS

This file gives repo-local constraints for Codex sessions on the artifact
branch. Read [README.md](README.md) first for the evaluator workflow.

## Scope

- Keep changes focused on the artifact branch.
- Keep generated benchmark outputs and timing logs out of git.
- Update [docs/INDEX.md](docs/INDEX.md) when adding, deleting, or renaming
  documentation.

## Build and Test

Set the job count:

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
```

Build:

```bash
cmake -S . -B build
cmake --build build -j${JOBS}
```

Regression:

```bash
ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}
```

## Change Discipline

- Use minimal patches.
- Avoid unrelated source formatting.
- Use `rg` for search.
- Use `apply_patch` for manual edits.
- Rebuild after C++ changes.
- Run the regression label after compiler, runtime, or probabilistic semantics
  changes.

## Reference Docs

- [docs/USAGE.md](docs/USAGE.md)
- [docs/TESTING.md](docs/TESTING.md)
- [docs/RUNBOOK.md](docs/RUNBOOK.md)
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- [docs/INDEX.md](docs/INDEX.md)
