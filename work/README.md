# Local Workspaces

This directory is the canonical home for heavy local-only assets that should
stay inside the repository checkout for tooling convenience without cluttering
the source tree.

## Layout
- `work/benchmarks/`: local benchmark repos and run workspaces
- `work/archive/`: bulky dated experiment batches and archived outputs
- `work/build/`: auxiliary build trees outside the canonical `build/`
- `work/deps-src/`: dependency source checkouts and solver trees
- `work/experiments/`: large experiment workspaces moved out of `experiments/`
- `work/scratch/`: ad hoc local scratch files generated at repo root

## Policy
- Keep the root `build/` directory as the canonical CI/default build tree.
- Use `work/` as the canonical local-heavy-workspace surface.
- Old top-level paths may remain as compatibility locations or symlinks for
  frozen docs, historical commands, and local tooling.
- Do not treat `work/` contents as source of truth; distill stable findings into
  `docs/`, `archive/README.md`, or maintained regression cases instead.

## Nested Repo Policy
- Treat `work/benchmarks/problog-benchmark/` as an external benchmark repo,
  not as a normal subtree of the main repository.
- Treat `work/deps-src/` as a local dependency/source cache.
- Keep `research/` separate and frozen for the concurrent research branch; do
  not fold it into `work/`.
- Do not add nested-repo payloads to the main repo index just because they live
  under the same checkout root.

## Retention Rules
- Keep small reproducible demos and maintained fixtures in the main source tree.
- Keep heavy local runs under `work/benchmarks/`, `work/experiments/`, or
  `work/archive/`.
- For archived experiment batches, prefer keeping:
  - `README.md`
  - `summary.json`
  - `summary.tsv`
  - `prepare.meta.json`
  - `compile.meta.json`
  - `run.meta.json`
  - at most a small number of representative `derivation.json` files
- By default, avoid retaining:
  - generated binaries and `*.cpp`
  - repeated rerun output trees
  - bulk `*.dot` dumps
  - duplicated input copies
  - full stdout/stderr log forests when a compact summary is sufficient

## Source references
- [README.md](../README.md)
- [archive/README.md](../archive/README.md)
- [docs/project/DOC_SYSTEM.md](../docs/project/DOC_SYSTEM.md)
- [docs/process/README.git.md](../docs/process/README.git.md)

## Related commits
- `UNCOMMITTED` — chore(layout): add canonical work/ hierarchy for local heavy assets
