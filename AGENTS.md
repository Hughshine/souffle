# AGENTS

Short, executable constraints for Codex in this repo. Keep it lean; link to
`docs/*` and `README*.md` for details.

## Scope
- This file is the source of truth for agent constraints.
- Keep commands here aligned with CI/scripts; otherwise add a TODO.

## Session Bootstrap
- New Codex sessions should read:
  1. `AGENTS.md`
  2. `docs/research/README.bootstrap.md`
  3. `docs/research/README.protocol.md`
  4. `docs/research/README.rewrite.status.md`
  5. the owning topic doc for the subsystem being changed
- If the task is experiment-heavy, keep `docs/research/*` current as part of the
  work, not as an afterthought.

## Setup Commands (from CI/scripts)
- Install deps (Ubuntu): `sudo sh/setup/install_ubuntu_deps.sh`
- Install deps (macOS Intel): `sh/setup/install_macos_deps.sh`
- Install deps (macOS ARM): `sh/setup/install_macos_arm_deps.sh`
- Build:
  - `cmake -S . -B build`
  - `cmake --build build -j${JOBS}` (set `JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)`)
- Format/style: `sh/run_test_format.sh`
- Regression tests (see `docs/TESTING.md`):
  - `ctest --test-dir build -L regression --output-on-failure --progress -j${JOBS}`
  - `cmake --build build --target check-regression`
  - `sh/run_regression_tests.sh`

## Change Discipline
- Keep changes minimal; avoid touching unrelated files and generated artifacts.
- Follow `docs/process/README.git.md` for commit hygiene (outputs/logs stay local).
- Avoid introducing new dependencies unless explicitly requested.
- Keep documentation in the single source of truth and link, do not duplicate.
- For experiment-heavy work, update `docs/research/*` when trusted conclusions,
  provenance rules, or recurring pitfalls change.
- Prefer repo-scoped skills: `repo-docs`, `verify-changes`, `pr-ready`, `git-commit-helper`, `souffle-test-case`.

## Verification
- If C++ changes: rebuild. Run `sh/run_test_format.sh` only when explicitly requested.
  TODO: Align this with CI expectations once clang-format is available by default.
- If CLI/runtime behavior changes: run the example script:
  `SOUFFLE_BIN=./build/src/souffle examples/running_example/run.sh`.
- If incremental/probabilistic behavior changes: run `ctest -L regression` using
  the repo-built binary from `build/src/souffle`.
- If tests cannot run, state why and point to `docs/TESTING.md`.

## Pitfalls / Do & Don't
- `sh/run_test_format.sh` rewrites files in the current git diff; run it only when you intend to format changed C++/headers.
- `examples/running_example/run.sh` only auto-detects `cmake-build-release`; set `SOUFFLE_BIN` if you build elsewhere.
- Treat `docs/historical/README.codex.md` as historical; prefer `AGENTS.md`, `docs/*`, and `README*.md`.
- Keep `docs/INDEX.md` updated when adding or renaming documentation.

## Reference Docs
- `docs/ARCHITECTURE.md`
- `docs/TESTING.md`
- `docs/RUNBOOK.md`
- `docs/SECURITY.md`
- `docs/USAGE.md`
- `docs/INDEX.md`
- `docs/project/DOC_SYSTEM.md`
- `docs/research/README.md`
- `docs/research/README.bootstrap.md`
- `docs/topics/testing/README.regression.md`
- `docs/project/README.md`
- `docs/process/README.git.md`
- `docs/historical/README.codex.md` (historical context only)

## Source references
- [sh/setup/install_ubuntu_deps.sh](sh/setup/install_ubuntu_deps.sh)
- [sh/run_test_format.sh](sh/run_test_format.sh)
- [sh/run_regression_tests.sh](sh/run_regression_tests.sh)
- [cmake/CTestDisabled.cmake](cmake/CTestDisabled.cmake)

## Related commits
- `UNCOMMITTED` — docs(codex): add explicit Codex session bootstrap order
- `UNCOMMITTED` — docs(system): point agent workflow to research docs for experiment-heavy work
- `UNCOMMITTED` — test(regression): add maintained CTest regression workflow
- `UNCOMMITTED` — docs(testing): document regression labels and cmake targets
- `UNCOMMITTED` — docs(repo): move commit hygiene guidance to `docs/process/README.git.md`
- `UNCOMMITTED` — docs(agents): add git-commit-helper skill preference
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
