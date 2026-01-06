# Contributing

Thanks for helping improve this fork. Please keep changes small and focused,
and align documentation with behavior changes.

## Development Environment
- Use the dependency scripts under `sh/setup/` (see `README.md`).
- CMake is the build system; `clang-format` is required for style checks.

## Branch and PR Flow
- Branch from `master` and keep PRs scoped to a single topic.
- Explain the motivation, expected behavior change, and any performance data.
- Avoid committing generated artifacts (see `README.git.md`).

## Code Style and Formatting
- Run `sh/run_test_format.sh` before submitting.
- Formatting is governed by `.clang-format`.

## Testing Strategy
- See `docs/TESTING.md` for test status and recommended verification steps.
- For behavior changes, run a relevant experiment flow (`README.eval*.md`) and
  include the commands and outputs in your PR description.

## Commit Messages
- Follow the format and rules in `README.git.md`.

## Documentation Updates
- Update the primary doc for any user-facing or workflow change:
  - `README.md` for user entry points.
  - `CONTRIBUTING.md` for contributor workflow.
  - `AGENTS.md` for Codex constraints.
  - `docs/*` for detailed guidance.
- Keep `docs/INDEX.md` in sync with doc additions or deprecations.

## Review Checklist
- Build succeeds locally (or note why not).
- Style check passes (`sh/run_test_format.sh`).
- Documentation updated for behavior changes.
- No generated artifacts or experiment outputs are committed.
