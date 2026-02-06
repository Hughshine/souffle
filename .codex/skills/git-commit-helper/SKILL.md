---
name: git-commit-helper
description: "Draft and prepare comprehensive git commits: review `git status`, confirm diffs, and write commit messages per README.git.md when asked to stage/commit."
---

# Git Commit Helper

Use this skill whenever the user asks to stage/commit, or wants a commit message that follows `README.git.md`.

## Workflow

1. **Inspect changes**
   - Run `git status -sb` to list modified/untracked files.
   - Review diffs: `git diff` and `git diff --staged` (if anything is staged).
   - If there are changes you did **not** make or do not understand, pause and ask the user before staging/committing.

2. **Light review before commit**
   - Confirm changes are expected and limited to the requested scope.
   - If you need context, open the relevant files.
   - If unrelated changes are present, avoid staging them.

3. **Commit message (per README.git.md)**
   - Read `README.git.md` for formatting rules.
   - Compose a **comprehensive** message covering:
     - What changed (code + docs)
     - Why it changed (intent / rationale)
     - Tests/verification run (or “not run” + reason)
     - Notable risks or follow-ups if required

4. **Staging rules**
   - Stage only relevant files (`git add -p` if needed).
   - Never stage generated artifacts or logs unless explicitly requested.

5. **Commit only on request**
   - If the user asked to commit: proceed after confirming the commit message.
   - Otherwise, provide the message draft and wait for approval.

## References

- `README.git.md` (commit hygiene and message format)
