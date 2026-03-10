# Rewrite Research Status

## Source references
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/problog/ImplicitSplitRewrite.cpp](src/problog/ImplicitSplitRewrite.cpp)
- [src/include/souffle/problog/ImplicitSplitRewrite.h](src/include/souffle/problog/ImplicitSplitRewrite.h)
- [docs/topics/rewrite/README.rewrite.impl.md](docs/topics/rewrite/README.rewrite.impl.md)
- [problog-benchmark/side_channel_full.py](problog-benchmark/side_channel_full.py)
- [problog-benchmark/.worktree/full-artifact/taint_full.py](problog-benchmark/.worktree/full-artifact/taint_full.py)


This file tracks the trusted current reading of rewrite work. It is the compact
status layer above the deeper implementation notes in
`docs/topics/rewrite/README.rewrite.impl.md`.

## Status
- Active curated status for legacy rewrite, implicit rewrite, and iterative
  implicit rewrite.

## Scope
- Trusted current results only.
- Raw run logs remain local; summarize only the conclusions that affect design
  or default choices.

## Trusted Current Conclusions
- Legacy `--rewrite` is end-to-end correct against `no rewrite` on checked
  `side_channel_full` cases.
- Current `--implicit-rewrite` is also end-to-end correct on the checked
  `side_channel_full` suite and is the best experimental rewrite variant in the
  latest trusted aggregate.
- Current `--implicit-iterate-split-rewrite` is correct on the checked cases but
  is still experimental; it is competitive and wins on some large mixed cases,
  but it is not yet consistently better than non-iterative implicit rewrite.

## Trusted Side-Channel Reading
Latest fresh full comparison against `side_channel_full` on the current build:
- `no rewrite`: `41.81s`
- legacy `--rewrite`: `23.17s`
- `--implicit-rewrite`: `22.24s`
- `--implicit-iterate-split-rewrite`: `22.32s`
- correctness: `19/19` exact matches against `no rewrite` on `facts.prob` and
  expected `.csv` outputs for legacy, implicit, and iterative variants
- aggregate ranking:
  - implicit vs legacy: better on `14/19`, worse on `5/19`
  - iterative vs legacy: better on `14/19`, worse on `5/19`
  - iterative vs implicit: better on `11/19`, worse on `8/19`

Representative full-runtime comparisons on checked cases:
- `P15`
  - legacy `--rewrite`: `1.167s`
  - `--implicit-rewrite`: `0.852s`
  - `--implicit-iterate-split-rewrite`: `0.897s`
- `P19`
  - legacy `--rewrite`: `6.183s`
  - `--implicit-rewrite`: `6.220s`
  - `--implicit-iterate-split-rewrite`: `5.805s`
- `P20`
  - legacy `--rewrite`: `6.415s`
  - `--implicit-rewrite`: `5.122s`
  - `--implicit-iterate-split-rewrite`: `6.044s`

Current reading:
- single-pass implicit rewrite is the best current experimental default among
  the rewrite prototypes
- iterative persistent-overlay rewrite is architecturally cleaner than the old
  overlay-to-graph outer loop and can already win on some large mixed cases
  (`P19`, `P16`), but it still regresses on others (`P20`, `P14`) and therefore
  does not yet replace single-pass implicit as the default experimental choice

## Trusted Taint Reading
Representative validated result:
- sampled taint `v2_semantic_subsetprob_noderv0 / and-roc / typefilter-dlog`
- shared matched stage input, stage-local comparison:
  - `no rewrite`: `71.01s`
  - legacy `--rewrite`: `195.96s`
  - `--implicit-rewrite`: `32.29s`
  - `--implicit-iterate-split-rewrite`: `32.19s`
  - exact `facts.prob` match (`968463` keys) for all variants
  - important interpretation: implicit and iterative both finish this stage as
    pure implicit rewrite (`implicit_graph_rewrite_ms = 0`)
- representative mixed stage:
  - sampled taint `v2_semantic_subsetprob_noderv0 / andors-trail / pt-obj-dlog`
  - shared matched stage input, stage-local comparison:
    - `no rewrite`: `45.25s`
    - legacy `--rewrite`: `27.36s`
    - `--implicit-rewrite`: `12.50s`
    - `--implicit-iterate-split-rewrite`: `12.38s`
    - exact `facts.prob` match (`111628` keys) for all variants
  - heavier mixed stage:
    - sampled taint `v2_semantic_subsetprob_noderv0 / and-roc / pt-obj-dlog`
    - shared matched stage input, stage-local comparison:
      - `no rewrite`: `93.17s`
      - legacy `--rewrite`: `46.84s`
      - `--implicit-rewrite`: `16.54s`
      - `--implicit-iterate-split-rewrite`: `16.75s`
      - exact `facts.prob` match (`184515` keys) for all variants

Current reading:
- on taint stage-local comparisons, both implicit variants are now clearly better
  than legacy rewrite on the checked `typefilter` and `pt-obj` stages
- iterative can edge out single-pass implicit on some taint stages
  (`and-roc/typefilter`, `andors-trail/pt-obj`), but it can also lose narrowly
  on others (`and-roc/pt-obj`), so the margin is currently small and not yet
  enough to override the broader
  side-channel picture

Full-pipeline target-range check (`problog-benchmark/runs/taint_target_range_report_20260310`):
- `v1`
  - `no rewrite`: only `2/15` complete, so it is no longer a reliable
    `30s-300s` benchmark
  - legacy `--rewrite`: `12/15` complete, `9/15` in range, but still `3`
    completed cases above `300s`
  - `--implicit-rewrite`: `13/15` complete, `8/15` in range, with the tail
    shifted from `>300s` to `<30s`
  - `--implicit-iterate-split-rewrite`: `13/15` complete, `7/15` in range,
    same remaining failures as non-iterative implicit (`tuio-droid`,
    `video-game`)
  - reading: implicit variants improve completeness and wall time, but they do
    not restore the original target-range calibration
- `v2`
  - `no rewrite`: `15/15` complete, `13/15` in `30s-300s`
  - legacy `--rewrite`: `15/15` complete, `12/15` in range, with `and-roc`
    above `300s`
  - `--implicit-rewrite`: `15/15` complete, `7/15` in range
  - `--implicit-iterate-split-rewrite`: `15/15` complete, `6/15` in range
  - reading: implicit variants are operationally better full evaluators than
    legacy rewrite, but on this sampled bundle they are too fast for the
    original `30s-300s` target
- speed comparisons on complete full-chain overlaps:
  - `v1`: implicit beats legacy on `12/12`; iterative beats legacy on `11/12`
    and beats implicit on `11/13`
  - `v2`: both implicit variants beat legacy on `15/15`; iterative beats
    non-iterative implicit on `11/15`
- important: full-chain rewrite timing must not be approximated by summing
  stage-local rewrite runs on no-rewrite inputs; the two can differ
  substantially in either direction

Full-chain qualitative reading:
- Iterative implicit rewrite does not currently change the benchmark class of
  taint cases.
- `v1`
  - implicit: `13/15` complete, `8/15` in `30s-300s`
  - iterative: `13/15` complete, `7/15` in `30s-300s`
  - remaining failures are identical: `tuio-droid`, `video-game`
- `v2`
  - implicit: `15/15` complete, `7/15` in `30s-300s`
  - iterative: `15/15` complete, `6/15` in `30s-300s`
- reading: iterative is still a same-regime optimization, not a qualitatively
  different evaluator. It shifts constants and wins on many cases, but it does
  not unlock a new class of taint full-chain behavior.

Implicit full-chain time composition (`v2`, `15/15` complete cases):
- total wall time across all cases: `484.86s`
- by analysis stage:
  - `typefilter-dlog`: `237.32s` (`48.95%`)
  - `pt-obj-dlog`: `173.63s` (`35.81%`)
  - `cipt-cg-dlog`: `73.13s` (`15.08%`)
  - `pre-dlog`: `0.25s` (`0.05%`)
  - `taint-lim-dlog`: `0.53s` (`0.11%`)
- implicit rewrite internals summed from stage logs:
  - `implicit rewrite total`: `96.41s` (`19.88%` of full-chain time)
  - `overlay_prep`: `87.34s` (`18.01%` of full-chain time, `90.59%` of rewrite time)
  - `overlay_fastpath`: `49.18s` (`10.14%` of full-chain time, `51.02%` of rewrite time)
  - `overlay_split`: `20.88s` (`4.31%` of full-chain time, counted inside `overlay_prep`)
  - `materialize`: `0.093s`
  - `graph_rewrite`: `0.073s`
- reading:
  - full-chain time is dominated by `typefilter` and `pt-obj`
  - current implicit overhead is no longer residual legacy graph rewrite
  - the dominant rewrite-side cost is now overlay preparation and its fast-path
    bookkeeping

## Known Pitfalls
- Taint provenance is easy to invalidate by mixing generated artifact sets.
- A previous invalid rerun mixed
  `.worktree/full-artifact/taint_full/*` with sampled bundle inputs under
  `problog-benchmark/taint_datasets/final-bundles/*`; it produced empty or
  mismatched outputs and must not be reused as evidence.
- Rewrite-only derivation JSON benchmarks are useful for structural profiling but
  do not replace end-to-end probability validation.

## Active Questions
- Can iterative implicit rewrite maintain split state incrementally enough to
  become consistently better than both legacy rewrite and non-iterative
  implicit rewrite?
- Can mixed-pattern residual costs be reduced without reintroducing the old
  explicit-split graph blow-up?
- Which additional taint stages, with matched provenance, should be used as the
  next trusted validation set?

## Related commits
- `UNCOMMITTED` — docs(research): add curated rewrite status and benchmark provenance warnings
