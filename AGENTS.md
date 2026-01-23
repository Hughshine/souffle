# AGENTS

Short, executable constraints for Codex in this repo. Keep it lean; link to
`docs/*` and `README*.md` for details.

## Scope
- This file is the source of truth for agent constraints.
- Keep commands here aligned with CI/scripts; otherwise add a TODO.

## Setup Commands (from CI/scripts)
- Install deps (Ubuntu): `sudo sh/setup/install_ubuntu_deps.sh`
- Install deps (macOS Intel): `sh/setup/install_macos_deps.sh`
- Install deps (macOS ARM): `sh/setup/install_macos_arm_deps.sh`
- Build:
  - `cmake -S . -B build`
  - `cmake --build build -j${JOBS}` (set `JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)`)
- Format/style: `sh/run_test_format.sh`
- Tests (see `docs/TESTING.md` for status):
  - `ctest --test-dir build -I "<range>" --output-on-failure --progress -j${JOBS}`

## Change Discipline
- Keep changes minimal; avoid touching unrelated files and generated artifacts.
- Follow `README.git.md` for commit hygiene (outputs/logs stay local).
- Avoid introducing new dependencies unless explicitly requested.
- Keep documentation in the single source of truth and link, do not duplicate.
- Prefer repo-scoped skills: `repo-docs`, `verify-changes`, `pr-ready`, `git-commit-helper`.

## Verification
- If C++ changes: rebuild. Run `sh/run_test_format.sh` only when explicitly requested.
  TODO: Align this with CI expectations once clang-format is available by default.
- If CLI/runtime behavior changes: run the example script:
  `SOUFFLE_BIN=./build/src/souffle examples/running_example/run.sh`.
- If tests cannot run, state why and point to `docs/TESTING.md`.

## Pitfalls / Do & Don't
- `sh/run_test_format.sh` rewrites files in the current git diff; run it only when you intend to format changed C++/headers.
- `examples/running_example/run.sh` only auto-detects `cmake-build-release`; set `SOUFFLE_BIN` if you build elsewhere.
- Treat `README.codex.md` as historical; prefer `AGENTS.md`, `docs/*`, and `README*.md`.
- Keep `docs/INDEX.md` updated when adding or renaming documentation.

## Reference Docs
- `docs/ARCHITECTURE.md`
- `docs/TESTING.md`
- `docs/RUNBOOK.md`
- `docs/SECURITY.md`
- `docs/USAGE.md`
- `docs/INDEX.md`
- `README.codex.md` (historical context only)

## Source references
- [sh/setup/install_ubuntu_deps.sh](sh/setup/install_ubuntu_deps.sh)
- [sh/run_test_format.sh](sh/run_test_format.sh)
- [cmake/CTestDisabled.cmake](cmake/CTestDisabled.cmake)

## Related commits
- `UNCOMMITTED` — docs(agents): add git-commit-helper skill preference
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
- `812ea4081` — docs(repo): refine README narratives
