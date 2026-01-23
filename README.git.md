# Git Commit Notes

## Source references
- [.gitignore](.gitignore)
- [sh/run_test_format.sh](sh/run_test_format.sh)


This repo mixes source, experiments, and generated artifacts. Keep commits small and avoid
checking in outputs or logs unless explicitly asked.

## Status
- Active repo hygiene guidance for commits and diffs.

## Scope
- Local repo hygiene guidance only; does not affect runtime behavior.

## What to Commit

Include:
- Source changes under `src/`, `src/include/`, `cmake/`, `tools/` that implement the change.
- Documentation updates under `README*.md` that explain the change.
- Hand-written scripts needed for reproducibility (explicitly requested).

Exclude (keep local):
- Anything under `experiments/` by default (see `README.eval.inc.md`).
- Generated outputs/logs: `output*`, `*.prob`, `*.csv`, `*.dot`, `*.json`,
  `run_*.time`, `run_*.stdout`, `log_*.json`.
- Build artifacts/binaries: `cmake-build-*`, `compute`, `*.o`, `*.a`, `*.so`.

If unsure, aim for `git status -s` to show only the code/docs you intended to change.

## Commit Message Format

Format:
`<type>(<scope>): <summary>`

Types: `fix`, `perf`, `refactor`, `docs`, `build`, `chore`, `test`.
Guidelines:
- Use imperative, present tense in summary.
- Keep the summary <= 72 characters, no trailing period.
- Scope is a concise component label (e.g., `inc-region`, `problog`, `cli`, `docs`).

Optional body:
- Explain rationale and include key metrics if relevant.
- Use footers for issue links if needed.

Examples:
- `perf(inc-region): cache delta-insert reachability`
- `refactor(problog): switch impacted maps to unordered_set`
- `docs(readme): add P12 profile5 results`

## Related commits
- `812ea4081` — docs(repo): refine README narratives
- `4c4bd26b2` — docs(readme): restructure online incremental docs
- `9be703353` — perf(inc-region): add regional pipeline and profiling
