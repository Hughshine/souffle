# AGENTS

Repo-local constraints for Codex sessions on the incremental AE branch.

## Source References

- [README.md:1](README.md#L1): evaluator-facing branch scope.
- [CMakeLists.txt:93](CMakeLists.txt#L93): CMake testing option.
- [CMakeLists.txt:267](CMakeLists.txt#L267): regression subdirectory wiring.
- [tests/regression/CMakeLists.txt:33](tests/regression/CMakeLists.txt#L33): `check-regression` target.
- [docs/INDEX.md:1](docs/INDEX.md#L1): documentation reading order.

## Scope

- Keep changes focused on the incremental AE surface.
- Keep generated benchmark outputs, logs, and timing data out of git.
- Update [docs/INDEX.md](docs/INDEX.md) when adding, deleting, or renaming documentation.
- Do not modify unrelated local research workspaces unless explicitly requested.

## Build And Test

```bash
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake -S . -B build
cmake --build build -j${JOBS}
ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}
cmake --build build --target check-regression
```

## Change Discipline

- Use minimal patches.
- Use `rg` for search.
- Use `apply_patch` for manual edits.
- Rebuild after C++ changes.
- Run the regression label after compiler, runtime, or probabilistic semantics changes.

## Reference Docs

- [docs/USAGE.md](docs/USAGE.md)
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- [docs/TESTING.md](docs/TESTING.md)
- [docs/RUNBOOK.md](docs/RUNBOOK.md)
- [docs/INDEX.md](docs/INDEX.md)
