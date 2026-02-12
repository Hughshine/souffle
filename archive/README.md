# Experiment Archive

This folder stores local benchmark batches by date while keeping the repository clean.

## Policy
- Each batch lives under `archive/YYYY-MM-DD/`.
- Keep one `README.md` in each batch with:
  - experiment goals and scope
  - key parameters
  - result summary and interpretation
  - unresolved issues/follow-ups
  - source pointers (logs, TSVs, related notes)
- Keep bulky generated artifacts (case outputs, binaries, generated C++) inside the batch directory only.
- Track only archive READMEs in git; other archive contents are ignored.

## Batches
- `archive/2026-02-11/README.md`: side-channel incremental benchmark batch (P12-P20 mix/standard runs, summary TSVs, plots).

## Source references
- [archive/2026-02-11/README.md](archive/2026-02-11/README.md)
- [problog-benchmark/README.side-channel-inc.md](problog-benchmark/README.side-channel-inc.md)

## Related commits
- `UNCOMMITTED` — docs(archive): add dated experiment archive index and batch README conventions
