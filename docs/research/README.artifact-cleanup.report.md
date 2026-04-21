# Artifact Cleanup Report

## Source references
- [src/problog/Pipeline.cpp](../../src/problog/Pipeline.cpp)
- [src/include/souffle/CompiledOptions.h](../../src/include/souffle/CompiledOptions.h)
- [src/include/souffle/problog/GraphRewriter.h](../../src/include/souffle/problog/GraphRewriter.h)
- [docs/research/README.rewrite.status.md](README.rewrite.status.md)
- [problog-benchmark/.worktree/clones/CAV-FULL/symbolization_benchmark/README.md](../../problog-benchmark/.worktree/clones/CAV-FULL/symbolization_benchmark/README.md)

This report records the current cleanup plan for turning the local
`full-artifact-opt-unified` checkpoint into an artifact-evaluation branch.

## Keep For Evaluation
- `--det-opt` remains part of all benchmark invocations.
- Bare `--rewrite` should be the artifact-facing rewrite flag.
- The smart dispatcher behind `--rewrite` should select the best maintained
  rewrite strategy per benchmark shape.
- Side-channel and taint should not require users to choose explicit vs implicit
  rewrite manually.
- Symbolization should use the deterministic no-split rewrite path while the
  rules remain deterministic and only input facts are probabilistic.
- `--knowledge bdd` remains the default trusted backend for these measurements.

## Keep As Diagnostic Controls
- `--implicit-rewrite` should remain available for direct implicit-lane
  debugging and historical comparison.
- `--split-mode=no-split` should remain available for no-split ablations.
- `--no-single-rand-fast` should remain available until the final dispatcher
  policy around component fast paths is stable.
- `--force-complete-siso-detect` and dirty-frontier related options should stay
  diagnostic-only.
- `SOUFFLE_DISABLE_REWRITE_*` and `SOUFFLE_SISO_*` environment toggles should
  not appear in artifact instructions except in a developer troubleshooting
  appendix.

## Remove Or Hide Before Artifact Freeze
- The temporary `--rewrite-compaction-only` CLI path has been removed locally and
  should stay removed.
- Old all-facts-on/off and implicit sub-variant instructions should be removed
  from benchmark READMEs unless they are explicitly labeled historical.
- CAV-FULL side-channel RQ2/RQ3 now encode `_r` as bare `--rewrite`; keep the
  older `--implicit-rewrite` spelling out of artifact-facing instructions.
- CAV-FULL taint still keeps the historical `implicit_rewrite` variant label,
  but that variant now calls bare `--rewrite`; the explicit diagnostic variant
  forces legacy rewrite via `--rewrite --split-mode=naive-split`.
- Avoid exposing local environment-variable probes as user-facing evaluation
  modes.

## Code Cleanup Targets
- Consolidate duplicated BDD/SDD component classification code paths before
  artifact freeze if time permits.
- Keep component timing fields, but label them as profiling metadata rather than
  benchmark outputs.
- Decide whether the no-split detector-mask heuristic should be permanent or
  guarded by a clearer dispatch-policy name.
- Do not enable dirty-only compaction in the artifact no-split path. On
  symbolization `troff`, the dirty-only restriction removed about `8.5k` fewer
  edges after rewrite and reintroduced a CUDD formula-build abort.
- Investigate `component_build_subgraphs_ms`; it is still a major cost on
  symbolization after rewrite.
- Investigate the symbolization `1e-8` last-digit differences before final
  correctness claims.
- Keep aggregation support small and isolated: it should remain a graph
  construction extension rather than a new rule-application protocol.

## Benchmark Cleanup Targets
- Use `problog-benchmark/.worktree/clones/CAV-FULL` as the packaged benchmark
  branch.
- Keep symbolization benchmark assets under `symbolization_benchmark/` only:
  `.dl`, `.facts`, `.prob`, and README are sufficient.
- Keep `troff` in the current symbolization performance table: plain still
  aborts, but the fixed bare `--rewrite` path completes.
- For symbolization tables, keep both categories visible:
  cases where plain and rewrite both complete, and cases where plain aborts but
  rewrite completes.
- For side-channel and taint, keep correctness comparison in the harness rather
  than relying on manual `cmp` commands.

## Worktree Cleanup Recommendation
- Use one canonical compiler worktree for the unified branch:
  `.worktree/clones/full-artifact-opt-unified-wt`.
- Keep one benchmark worktree:
  `problog-benchmark/.worktree/clones/CAV-FULL`.
- Archive or delete local-only clones only after confirming their unique commits
  are merged into the unified branch or intentionally abandoned.
- Do not preserve temporary `/tmp` experiment directories in git; cite their
  paths in research notes only when they explain a decision.

## Current Validation Snapshot
- Build:
  `cmake --build build -j2 --target souffle` succeeds.
- Symbolization:
  `/tmp/symbolization_head_fix_20260421/full_rerun/summary.tsv`.
- CAV-FULL side-channel and taint smoke after script migration to bare
  `--rewrite`:
  `/tmp/cavfull_smoke_smart_script_20260421`.

## Current Local Checks
- Final compiler check:
  `cmake --build build -j2 --target souffle` succeeded after the code cleanup.
- Source-reference check:
  `rg rewrite-compaction-only src docs/USAGE.md docs/topics` returns no source
  implementation references.
- Benchmark smoke checks:
  CAV-FULL smoke passes on side-channel `P1` and taint `angulo`; `_r` and
  `implicit_rewrite` both dispatch through bare `--rewrite` to
  `auto-implicit` and preserve output equality in the checked runs.

## Remaining Follow-Up
- Audit the symbolization `1e-8` output differences before making bitwise
  equality claims.
- Continue optimizing symbolization overhead, especially SISO detection
  scheduling and `component_build_subgraphs_ms`.
- Investigate whether SUM aggregate subgraphs need a specialized exact
  evaluator before making claims on the largest symbolization stress cases.

## Related commits
- `UNCOMMITTED` — perf(problog): make bare rewrite dispatch benchmark-aware
- `UNCOMMITTED` — docs(research): add artifact cleanup report
- `c33d371` — artifact: add DDisasm symbolization benchmark
