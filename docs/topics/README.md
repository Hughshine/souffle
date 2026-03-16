# Topics Documentation

Topical docs for currently supported behavior and workflows.

Use `docs/research/README.md` for active measurements, current rankings, and
experiment protocol. Topic docs should describe behavior, not act as raw
research notebooks.

## Subfolders
- `docs/topics/runtime/`: flags, dump/debug output references.
- `docs/topics/pipeline/`: online/incremental semantics and pruning behavior.
- `docs/topics/rewrite/`: rewrite and graph-transform behavior.
- `docs/topics/backends/`: backend-specific notes (CUDD/SDD).
- `docs/topics/profiling/`: profiling fields and interpretation.
- `docs/topics/evaluation/`: current benchmark workflows and latest batch summaries.
- `docs/topics/testing/`: maintained regression workflow and case coverage.

## Entry Point
- Use `docs/INDEX.md` for canonical priority and recency tags.
- Use `docs/research/README.md` when you need current trusted conclusions rather
  than implementation details.
- For implicit rewrite implementation details specifically, use
  `docs/topics/rewrite/README.implicit.md`.

## Source references
- [docs/INDEX.md](docs/INDEX.md)
- [docs/research/README.md](docs/research/README.md)
- [docs/topics/rewrite/README.implicit.md](docs/topics/rewrite/README.implicit.md)
- [docs/topics/pipeline/README.inc.region.md](docs/topics/pipeline/README.inc.region.md)
- [docs/topics/evaluation/README.md](docs/topics/evaluation/README.md)
- [docs/topics/testing/README.regression.md](docs/topics/testing/README.regression.md)

## Related commits
- `UNCOMMITTED` — docs(rewrite): route implicit rewrite readers to the dedicated implementation doc
- `UNCOMMITTED` — docs(system): separate topic behavior docs from active research docs
- `UNCOMMITTED` — docs(testing): add topical regression workflow reference
- `UNCOMMITTED` — docs(evaluation): add topical evaluation map and current/historical split
