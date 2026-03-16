# Experiment Archive

This folder stores local benchmark batches by date while keeping the repository clean.

## Policy
- Each batch keeps a tracked manifest under `archive/YYYY-MM-DD/README.md`.
- Bulky batch payloads now belong under `work/archive/YYYY-MM-DD/`.
- When older paths must keep working, leave compatibility symlinks from
  `archive/YYYY-MM-DD/*` into `work/archive/YYYY-MM-DD/*`.
- Keep one `README.md` in each batch with:
  - experiment goals and scope
  - key parameters
  - result summary and interpretation
  - unresolved issues/follow-ups
  - source pointers (logs, TSVs, related notes)
- Track only archive READMEs in git; bulky payload directories remain local-only.
- Default archive retention is compact:
  - keep `summary.json` / `summary.tsv`
  - keep `prepare.meta.json`, `compile.meta.json`, and `run.meta.json`
  - keep only representative `derivation.json` files when they are needed for
    debugging or replay
- Do not keep full output forests by default when the same run is already
  captured by compact summary/meta artifacts.

## Batches
- `archive/2026-02-11/README.md`: side-channel incremental benchmark batch (P12-P20 mix/standard runs, summary TSVs, plots).

## Source references
- [archive/2026-02-11/README.md](archive/2026-02-11/README.md)
- [problog-benchmark/README.side-channel-inc.md](problog-benchmark/README.side-channel-inc.md)

## Related commits
- `UNCOMMITTED` — docs(archive): add dated experiment archive index and batch README conventions
