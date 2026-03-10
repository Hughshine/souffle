# Research Documentation

## Source references
- [docs/project/DOC_SYSTEM.md](docs/project/DOC_SYSTEM.md)
- [docs/topics/rewrite/README.rewrite.impl.md](docs/topics/rewrite/README.rewrite.impl.md)
- [docs/topics/evaluation/README.md](docs/topics/evaluation/README.md)
- [docs/process/README.git.md](docs/process/README.git.md)


This folder is the tracked, repo-local replacement for ad hoc research notebooks
and the external SER workspace. It captures active experimental context,
provenance rules, and trusted current conclusions. It does not store raw run
logs or generated outputs.

## Status
- Active source of truth for research protocol, session memory, and curated
  rewrite status.

## Scope
- Current experimental process and trusted active findings for this repository.
- Not a dump of raw benchmark runs; those stay local under
  `problog-benchmark/runs/`, `/tmp`, or other ignored locations unless
  explicitly curated.

## Contents
- `docs/research/README.protocol.md`: required experiment metadata, provenance,
  and validity rules.
- `docs/research/README.memory.md`: Codex-oriented session and memory
  maintenance rules for experiment-heavy work.
- `docs/research/README.rewrite.status.md`: trusted current rewrite conclusions,
  pitfalls, and open questions.

## How To Use
- Start here for experiment-heavy work, rewrite tuning, or benchmark
  interpretation.
- Update a topic doc under `docs/topics/` when implementation behavior changes.
- Update a status doc here when trusted conclusions, pitfalls, or benchmark
  readings change.
- Keep raw artifacts local and distill only stable findings into tracked docs.

## Related commits
- `UNCOMMITTED` — docs(research): internalize experiment protocol and rewrite status into repo docs

