---
name: pr-ready
description: "Prepare changes for PR review: summary, risks, rollback, tests, and doc updates."
---

# PR Ready

Use this when preparing a change for review or handoff.

## Steps
1. Summarize the change (what/why) with file references.
2. Note risks and a simple rollback plan (revert commit + rebuild).
3. Include verification evidence (or link to the verify-changes summary).
4. Confirm docs and `docs/INDEX.md` are updated if behavior changed.
5. Ensure no generated artifacts are staged (see `README.git.md`).
6. Always keep Skills and docs up to date; if they diverge, update both.
7. Docs sync checkpoint: if doc updates are needed and not already requested,
   ask; when updating, keep `## Source references` as markdown links and add
   a Related commits section (latest 3).

## Related commits
- `e28b76ffe` — chore(repo): add codex metadata
