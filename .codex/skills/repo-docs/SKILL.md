---
name: repo-docs
description: Update README/CONTRIBUTING/AGENTS/docs and docs/INDEX when behavior or workflows change.
---

# Repo Docs

Use this when changes affect user workflows, contributor steps, or agent constraints.

## Steps
1. Identify the primary doc:
   - `README.md` for user-facing entry points.
   - `CONTRIBUTING.md` for contributor workflow.
   - `AGENTS.md` for Codex constraints.
   - `docs/*` for detailed architecture/testing/runbook/security.
2. Update only the primary source and link instead of duplicating rules.
3. Ensure any install/test/lint/format/build/dev commands come from repo configs
   (CI workflows or scripts). If not, add a TODO instead of inventing commands.
4. Sync `docs/INDEX.md` if you add, remove, or repurpose docs.
5. Record a brief docs-change checklist in the response (files touched + why).
