# Evaluation Docs

Current evaluation docs are split by workflow and by lifecycle stage (current vs historical).

## Current Workflows
- `docs/topics/evaluation/README.eval.inc.md`: incremental side-channel workflow (`side_channel_inc.py`).
- `docs/topics/evaluation/README.eval.md`: full-only side-channel workflow (`side_channel_full.py`).
- `docs/topics/evaluation/README.artifact.inc.md`: minimal artifact reproduction path.
- `docs/topics/evaluation/OPT.md`: optimization-oriented inc-regional run checklist.
- `docs/topics/evaluation/README.eval.final.md`: latest curated batch summary.

## Historical Logs
Large experiment logs and old snapshots are archived under `docs/historical/`:
- `docs/historical/README.eval.inc.log.md`
- `docs/historical/README.eval.full.log.md`
- `docs/historical/README.eval.final.2026-01-21.md`
- `docs/historical/OPT.md`

For dated run artifacts and TSV/plot outputs, use `archive/`.

## Source references
- [problog-benchmark/README.side-channel-inc.md](problog-benchmark/README.side-channel-inc.md)
- [problog-benchmark/README.side-channel-full.md](problog-benchmark/README.side-channel-full.md)
- [archive/README.md](archive/README.md)

## Related commits
- `UNCOMMITTED` — docs(evaluation): split current workflow docs from historical run logs
