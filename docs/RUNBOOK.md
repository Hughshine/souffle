# Runbook

## Source references
- [examples/running_example/run.sh](examples/running_example/run.sh)
- [sh/setup/install_ubuntu_deps.sh](sh/setup/install_ubuntu_deps.sh)
- [src/MainDriver.cpp](src/MainDriver.cpp)


## Start / Build
- Follow the Quickstart in `README.md` for dependency install and build steps.
- The bundled example uses `examples/running_example/run.sh` (set `SOUFFLE_BIN`
  if your build directory is not the default).
- CUDD is required for the BDD backend; SDD is optional for `-k sdd`.
- TODO (unconfirmed): add standardized CUDD/SDD install commands (no repo script or
  config currently defines them).

## Operate / Run Experiments
- Full-mode evaluation: see `docs/topics/evaluation/README.eval.md`.
- Incremental evaluation: see `docs/topics/evaluation/README.eval.inc.md`.
- Evaluation docs map and historical logs: `docs/topics/evaluation/README.md`.
- Interactive incremental runs use the turn-based CLI (`insert`, `delete`, `commit`).

## Rollback
- Keep the last known-good build directory and point `SOUFFLE_BIN` at it.
- For regressions, revert the commit and rebuild to restore previous behavior.

## Logs and Metrics
- `--logfile <name>` writes JSON reports to the output directory (`-D`).
- `--dumpjson`, `--dumpdot`, `--dumpstat` emit artifacts in the output directory.
- `-p <file>` enables profiling output when compiled with profiling.

## Troubleshooting
- `ctest` fails: see `docs/TESTING.md` for status and alternatives.
- `souffle` not found: set `SOUFFLE_BIN` or add the built binary to `PATH`.
- Missing BDD/SDD backend: ensure CUDD is installed (SDD optional for `-k sdd`).
- `--inc` not recognized: the legacy backend is removed; use online incremental modes.
- Large outputs/logs: avoid committing generated artifacts (see `docs/process/README.git.md`).

## Common Checks
- Compare `facts.prob` outputs across runs for consistency.
- Inspect stdout timing lines and JSON logs for hot stages before tuning.

## Related commits
- `UNCOMMITTED` — docs(process): update artifact-commit hygiene reference path
- `812ea4081` — docs(repo): refine README narratives
