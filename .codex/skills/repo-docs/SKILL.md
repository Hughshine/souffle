---
name: repo-docs
description: Update README/CONTRIBUTING/AGENTS and docs/INDEX.md when behavior or workflows change.
---

# Repo Docs

Use this when changes affect user workflows, contributor steps, or agent constraints.

## Steps
1. Identify the primary doc:
   - `README.md` for user-facing entry points.
   - `CONTRIBUTING.md` for contributor workflow.
   - `AGENTS.md` for Codex constraints.
   - `docs/*` and `README*.md` for detailed architecture/testing/runbook/security.
2. Update only the primary source and link instead of duplicating rules.
3. Ensure any install/test/lint/format/build/dev commands come from repo configs
   (CI workflows or scripts). If not, add a TODO instead of inventing commands.
4. Sync `docs/INDEX.md` if you add, remove, or repurpose docs.
5. Record a brief docs-change checklist in the response (files touched + why).
6. Always keep Skills and docs up to date; if they diverge, update both.
7. Docs sync checkpoint: if the user hasn’t already asked for doc updates, explicitly ask
   whether to update docs. When updating, keep the same doc style:
   - every touched doc keeps `## Source references` with markdown links
   - every touched doc appends/refreshes a Related commits section (latest 3, or note no history)

## Related commits
- `e28b76ffe` — chore(repo): add codex metadata
