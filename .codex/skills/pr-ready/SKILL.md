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
