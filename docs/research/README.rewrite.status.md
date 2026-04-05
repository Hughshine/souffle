# Rewrite Research Status

## Source references
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/problog/ImplicitSplitRewrite.cpp](src/problog/ImplicitSplitRewrite.cpp)
- [docs/topics/rewrite/README.rewrite.impl.md](docs/topics/rewrite/README.rewrite.impl.md)
- [problog-benchmark/.worktree/clones/CAV-FULL/README.md](problog-benchmark/.worktree/clones/CAV-FULL/README.md)
- [problog-benchmark/.worktree/clones/CAV-FULL/README.side_channel_full_evaluation.md](problog-benchmark/.worktree/clones/CAV-FULL/README.side_channel_full_evaluation.md)
- [problog-benchmark/.worktree/clones/CAV-FULL/side_channel_full_evaluation.py](problog-benchmark/.worktree/clones/CAV-FULL/side_channel_full_evaluation.py)
- [problog-benchmark/.worktree/clones/CAV-FULL/taint_appendix_compare.py](problog-benchmark/.worktree/clones/CAV-FULL/taint_appendix_compare.py)
- [problog-benchmark/.worktree/clones/CAV-FULL/example/results/20260328_local_full_artifact/SUMMARY.md](problog-benchmark/.worktree/clones/CAV-FULL/example/results/20260328_local_full_artifact/SUMMARY.md)
- [problog-benchmark/.worktree/clones/CAV-FULL/example/results/20260328_local_full_artifact/appendix-taint/STATUS.md](problog-benchmark/.worktree/clones/CAV-FULL/example/results/20260328_local_full_artifact/appendix-taint/STATUS.md)


This file tracks the trusted current reading of rewrite work. It is the compact
status layer above the deeper implementation notes in
`docs/topics/rewrite/README.rewrite.impl.md`.

## Status
- Active curated status for the current full-artifact rewrite defaults and
  appendix claim.

## Scope
- Trusted current results only.
- Raw run logs remain local; summarize only the conclusions that affect design,
  default choices, or artifact packaging.

## Current Trusted Artifact Pair
- benchmark side:
  - `problog-benchmark/.worktree/clones/CAV-FULL`
  - branch `CAV-FULL`
  - packaged tip `00565d05fb576b66ec624bc601e63c94299e020a`
- compiler side:
  - `.worktree/clones/full-artifact`
  - branch `full-artifact`
  - paired tip `7d9d45b72fb59657062d07348e7361d1ecdc20f8`

## Trusted Current Conclusions
- `--rewrite` is the explicit rewrite pipeline.
- `--implicit-rewrite` is the implicit-split rewrite pipeline.
- The maintained full artifact now defaults to implicit rewrite for the
  side-channel rewrite comparison.
- `RQ3` is no longer treated as a separate workload; it is derived from the
  validated `RQ2` standard runs.
- Iterative implicit rewrite remains experimental and is not part of the current
  packaged full artifact contract.

## Trusted Side-Channel Reading
From the shipped reference bundle
`example/results/20260328_local_full_artifact`:
- `RQ1`: consistent with the paper.
  - `with_opt / with_det` geometric-mean speedup on completed cases: `47.32x`
  - qualitative reading: `pSouffle` stays much faster than ProbLog on larger
    cases
- `RQ2` standard: consistent overall with one extra boundary deviation at `P12`
  - geometric-mean speedup over all measured cases: `2.04x`
  - geometric-mean speedup on `P12-P20`: `1.80x`
  - slower-under-rewrite cases: `P7`, `P8`, `P9`, `P12`
  - interpretation: the slower cases are short-running boundary cases where
    fixed engineering overhead dominates
- `RQ2` stress: strongly consistent with the paper
  - geometric-mean speedup over all measured cases: `4.93x`
  - geometric-mean speedup on `P12-P20`: `24.37x`
  - `P19` plain timed out while rewrite still completed
- `RQ3`: consistent with the paper and derived from the standard `RQ2` runs
  - geometric-mean speedup over all measured cases: `2.28x`
  - geometric-mean speedup on `P12-P20`: `1.93x`

Correctness reading:
- all comparable side-channel rewrite runs in the shipped bundle were consistent
  with their matching baseline on `output/facts.prob`
- the consistency record is part of the main evaluation pipeline and is emitted
  in the collected TSVs

## Trusted Appendix Taint Reading
From the shipped reference bundle
`example/results/20260328_local_full_artifact/appendix-taint`:
- sampled bundle:
  - `taint_datasets/final-bundles/v2_semantic_subsetprob_noderv0`
- variants:
  - `no_rewrite`
  - `explicit_rewrite`
  - `implicit_rewrite`
- trusted result:
  - `implicit_rewrite` completed `15/15` cases
  - final-stage consistency vs `no_rewrite`: `15/15`
  - `no_rewrite / implicit_rewrite` geometric-mean speedup: `4.35x`
  - every sampled case favored `implicit_rewrite`
  - `explicit_rewrite` completed `3/15` cases and timed out on `12/15`
  - on the completed overlap, `explicit_rewrite` was slower than
    `no_rewrite` (`0.27x` geometric mean)

Current reading:
- the appendix claim should stay narrow:
  implicit rewrite is the only practical rewrite mode on this sampled taint
  bundle, and it preserves correctness on the completed cases

## Known Pitfalls
- The old `plp-bench:latest` Docker image is no longer a trusted `CAV-FULL`
  reproduction environment.
- Taint provenance is easy to invalidate by mixing generated artifact sets,
  prepared workspaces, and sampled bundles.
- Fixed-seed side-channel timing from a detached baseline worktree is not
  stable enough to serve as a strict regression oracle by itself. On
  `2026-04-04`, the detached baseline at `1620245ba` and a clean rebuild of the
  same source in the active `full-artifact-opt` worktree already diverged on
  `P19/P20` (`P19 bdd_r 0.9398s -> 1.2372s`, `P20 bdd_r 1.4443s -> 1.7739s`).
  Performance judgments for local optimization attempts should therefore be
  made within the same binary lineage or same-worktree rebuild, not only
  against an older detached build.
- On taint-heavy comparisons, `no_rewrite / implicit_rewrite` speedup alone is
  not a sufficient regression oracle. On `2026-04-04`, a contemporaneous serial
  detached-baseline comparison showed a helper-only refactor to be slightly
  faster on implicit absolute time (`impl / baseline_impl = 0.9949x`) even
  though the derived `no_rewrite / implicit_rewrite` speedup row moved
  downward (`2.8618x -> 2.7644x`) because the matched `no_rewrite` runs
  drifted. For taint optimization work, compare implicit absolute time and
  internal profiling deltas in addition to the no-vs-implicit speedup ratio.
- Small-case rewrite regressions do not carry the same weight as large-case
  trends; the important cases are the larger ones where inference dominates
  fixed pipeline overhead.
- Rewrite-only derivation JSON benchmarks are useful for structural profiling but
  do not replace end-to-end probability validation.

## Active Questions
- Should iterative implicit rewrite stay as a research-only prototype, or is
  there still a concrete path to making it competitive enough for renewed
  artifact consideration?
- For the final appendix presentation, how small should the taint section
  become while still preserving the claim, setup, and correctness record?

## Optimization Experiment Log
This section records recent optimization attempts even when they are not yet
promoted into the trusted artifact reading above. The intent is to preserve
enough quantitative context to guide the next iteration and avoid repeating the
same dead ends.

### 2026-04-04: Rejected attempt — fully incremental active-edge index maintenance
- status:
  rejected and reverted locally; not part of the current `full-artifact-opt`
  baseline
- implementation sketch:
  replace dirty/lazy index rebuild with per-edge incremental maintenance of
  `activeIncomingEdgeIdsByNode_`, `activeOutgoingEdgeIdsByRef_`,
  `activeSemanticInputOccurrencesByFact_`, and `activeEdgeIds_`
- reason for trying it:
  the current taint appendix profile shows large
  `implicit_overlay_rebuild_index_ms`, so making the overlay indices
  incrementally maintained looked like the most direct bookkeeping reduction
- quantitative outcome on the strict taint subset
  `and-roc, app-018, yaaic`:
  `no_rewrite / implicit_rewrite` geometric-mean speedup regressed from
  `2.298135x` to `2.085395x`
- per-case implicit totals on the same subset:
  `and-roc 54.83s -> 72.68s`, `app-018 18.44s -> 20.58s`,
  `yaaic 8.13s -> 10.48s`
- failure mode:
  `implicit_overlay_rebuild_index_ms` dropped to zero, but the cost was
  shifted into more expensive per-edge maintenance during split/rewrite
  application, especially in `typefilter-dlog` and `pt-obj-dlog`
- conclusion:
  replacing batched rebuilds with eager fine-grained index mutation is not a
  good direction for these taint-heavy workloads

### 2026-04-04: Mixed attempt — split candidate frontier from active fan-out facts
- status:
  re-evaluated under same-worktree baseline-vs-patch; still not promoted into
  the current `full-artifact-opt` baseline because the taint reading regressed
  even though the corrected side-channel reading was positive
- implementation sketch:
  `applySplit()` no longer scans the entire `nodeState_`; it now only visits
  base facts that currently appear in `activeOutgoingEdgeIdsByRef_` with
  `alias=0`, at least two active outgoing edges, and the usual
  `originalIsFact/currentIsFact/no-output/no-evidence` preconditions
- reason for trying it:
  this keeps split enumeration aligned with the semantic domain where split can
  actually fire, without changing rewrite semantics or adding benchmark-specific
  special cases
- the earlier detached-baseline reading is now treated as methodologically
  invalid for strict keep/drop decisions; see the pitfall above on timing drift
  across different worktree rebuilds of the same source
- corrected same-worktree fixed-seed side-channel comparison on `P19/P20`
  against the clean active-worktree rebuild:
  clean `P19 bdd 11.8851s, bdd_r 1.2372s`, candidate
  `P19 bdd 9.8167s, bdd_r 0.9042s`; clean
  `P20 bdd 4.0838s, bdd_r 1.7739s`, candidate
  `P20 bdd 3.5456s, bdd_r 1.3165s`
- corrected side-channel reading:
  on the same generated inputs and same binary lineage, the candidate improved
  both plain and rewrite lanes on the checked large cases; `bdd_r` improved by
  about `26.9%` on `P19` and `25.8%` on `P20`
- same-worktree taint subset comparison on `and-roc, app-018, yaaic`:
  `no_rewrite / implicit_rewrite` geometric-mean speedup regressed from
  `2.993262x` to `2.807896x`
- per-case taint reading on the same subset:
  `and-roc implicit 44.54s -> 46.41s`, `app-018 14.67s -> 15.13s`,
  `yaaic 7.10s -> 7.70s`; all cases remained fully consistent with
  `no_rewrite`
- stage-level taint reading on the same subset:
  aggregate implicit bookkeeping increased slightly rather than decreasing:
  `implicit_total_ms 11400.96 -> 11868.53`,
  `overlay_prep_ms 10018.62 -> 10334.10`,
  `rebuild_index_ms 2826.78 -> 3033.02`,
  `siso_detect_ms 830.43 -> 939.10`,
  `siso_summarize_ms 1304.92 -> 1490.49`
- conclusion:
  narrowing split enumeration to the active fan-out frontier remains a
  promising side-channel optimization, but in its current form it slightly
  regresses taint-heavy workloads and therefore still fails the no-regression
  bar for a kept default

### 2026-04-04: Rejected attempt — monotone all-facts closure with pass-local counts
- status:
  rejected and reverted locally; not part of the current `full-artifact-opt`
  baseline
- implementation sketch:
  replace the current single-pass `rewriteAllFactsPass()` scan with a monotone
  worklist that snapshots active adjacency once, maintains pass-local incoming /
  outgoing / semantic-input counts, and propagates newly factified outputs
  without rebuilding overlay indices inside the pass
- reason for trying it:
  the heaviest taint stages are dominated by `overlay_split + all-facts +
  rebuild_index`; a monotone closure looked like the most principled way to
  collapse repeated all-facts iterations into one pass-local transaction
- fixed-seed `3`-run side-channel comparison against the detached stable
  baseline on `P19/P20`:
  baseline `P19 bdd 9.9745s, bdd_r 0.9398s`, candidate
  `P19 bdd 10.2550s, bdd_r 1.1445s`; baseline
  `P20 bdd 3.7776s, bdd_r 1.4443s`, candidate
  `P20 bdd 4.9195s, bdd_r 1.8531s`
- side-channel reading:
  this attempt regressed absolute `bdd_r` time on both checked cases and also
  slowed the plain `bdd` lane, so it failed the no-regression bar before any
  taint upside could justify it
- taint subset outcome on `and-roc, app-018, yaaic`:
  rejected before report collection completed because
  `implicit_rewrite/and-roc/typefilter-dlog` reproduced the known bad shape:
  `output/typeFilter.csv` was emitted but the stage never produced
  `run.meta.json` or returned control to the harness
- conclusion:
  a pass-local all-facts closure is attractive in principle, but this concrete
  version is not safe enough to keep; it regresses fixed-seed side-channel
  timings and reintroduces the taint stage hang failure mode

### 2026-04-04: Rejected attempt — saturate local fast paths before generic detect
- status:
  rejected and reverted locally; not part of the current `full-artifact-opt`
  baseline
- implementation sketch:
  keep the existing stable `rewriteAllFactsPass()` and
  `rewriteSingleHyperedgePass()` logic, but change scheduling so that
  `all-facts + single-hyperedge` saturate to a local fixpoint before entering
  the more expensive `linear / parallel / fan-out` global detect loop
- reason for trying it:
  the heaviest taint stages often report zero generic pattern hits, so running
  global candidate construction between every local all-facts step looked like
  avoidable empty work
- fixed-seed `3`-run side-channel comparison against the detached stable
  baseline on `P19/P20`:
  baseline `P19 bdd 9.9745s, bdd_r 0.9398s`, candidate
  `P19 bdd 10.9166s, bdd_r 1.2560s`; baseline
  `P20 bdd 3.7776s, bdd_r 1.4443s`, candidate
  `P20 bdd 4.0370s, bdd_r 1.8405s`
- side-channel reading:
  this scheduling change regressed both the plain and rewrite lanes on the
  checked large boundary cases, so it failed before any appendix-facing benefit
  could justify further iteration
- taint subset note:
  a parallel strict-subset probe on `and-roc, app-018, yaaic` was started but
  intentionally abandoned once the fixed-seed side-channel regression was
  already clear; this attempt therefore has no trusted taint speedup reading
- conclusion:
  reordering the existing passes is safe enough to smoke-test, but it is not a
  free win; on the checked workloads it simply delayed useful work and worsened
  end-to-end time

### 2026-04-04: Mixed attempt — dirty split fact worklist
- status:
  measured, recorded, and currently the best principled local candidate, but
  not yet promoted into the kept `full-artifact-opt` baseline because the taint
  subset still shows a small regression
- implementation sketch:
  keep virtual split plus direct commit, but replace repeated split candidate
  enumeration with a stable dirty fact worklist: seed all split-eligible
  original facts once, then only re-enqueue fact families whose outgoing edge
  membership changes under later rewrite commits
- reason for trying it:
  unlike the earlier active-frontier filter, this is a true dirty-driven split
  engine and directly targets the remaining full-facts scan in `applySplit()`
- same-worktree fixed-seed side-channel comparison on `P19/P20`:
  clean `P19 bdd_r 1.2372s`, candidate `0.9321s`;
  clean `P20 bdd_r 1.7739s`, candidate `1.2747s`
- side-channel reading:
  geometric mean of `bdd_r / clean_bdd_r` on `P19/P20` improved to `0.7358x`
  with correctness preserved on every checked run
- same-worktree taint subset comparison on `and-roc, app-018, yaaic`:
  `no_rewrite / implicit_rewrite` geometric-mean speedup changed from
  `2.993262x` to `2.815555x`
- taint absolute reading:
  implicit runtime geometric mean stayed close to clean
  (`impl / clean_impl = 1.012240x`), but it was still a real regression rather
  than parity
- stage-level taint reading:
  `implicit_overlay_rebuild_index_ms` improved (`2826.78 -> 2745.64`), but
  `implicit_overlay_split_ms` and `implicit_overlay_siso_summarize_ms`
  increased enough to erase that win
- conclusion:
  a true dirty split worklist is the strongest direction tried so far and it
  clearly helps side-channel, but it still needs one more reduction in taint
  bookkeeping or rewrite churn before it satisfies the no-regression bar

### 2026-04-04: Rejected attempt — alias-gated dirty split marking
- status:
  rejected and reverted locally; not part of the current best candidate
- implementation sketch:
  keep the dirty split fact worklist, but only re-enqueue base facts when a
  changed edge mentions the base ref directly; alias-ref edge changes no longer
  mark the base fact dirty
- reason for trying it:
  split only reasons about base-ref outgoing sets, so alias-edge churn looked
  like avoidable noise in the dirty worklist
- side-channel outcome on the same `P19/P20` fixed-seed check:
  this was the strongest side-channel result of the day:
  `P19 bdd_r 0.8928s`, `P20 bdd_r 1.1108s`
- taint subset outcome:
  it over-pruned the worklist and regressed the taint geometric mean to
  `2.812684x`, with implicit runtime geometric mean worsening to
  `1.038818x` of clean
- stage-level reading:
  raw bookkeeping totals looked good
  (`implicit_total_ms 11400.96 -> 11351.72`), but `app-018` and `yaaic`
  slowed in later stages, indicating that the narrower dirty rule lost useful
  rewrite opportunities
- conclusion:
  alias-ref changes cannot simply be ignored; that optimization improves
  bookkeeping totals and side-channel runs, but it is too aggressive for taint

### 2026-04-04: Rejected attempt — single-hyperedge removed-input dirty marking
- status:
  rejected and reverted locally; not part of the current best candidate
- implementation sketch:
  refine the dirty split worklist so that single-hyperedge rewrites only
  re-enqueue fact inputs actually removed from the edge body, not the retained
  `si`
- reason for trying it:
  this was meant to be a smaller, semantically precise follow-up to the dirty
  worklist, targeting unnecessary split rechecks in `rewriteSingleHyperedgePass`
- side-channel outcome on the same `P19/P20` fixed-seed check:
  `P19 bdd_r` improved further to `0.7983s`, while `P20 bdd_r` stayed strong at
  `1.2990s`
- taint subset outcome:
  despite the side-channel gain, the appendix subset regressed the most:
  `no_rewrite / implicit_rewrite` geometric-mean speedup fell to `2.716069x`,
  and implicit runtime geometric mean rose to `1.059687x` of clean
- conclusion:
  the more selective single-hyperedge dirty rule amplifies the side-channel
  win, but it pushes the taint subset further away from parity and therefore is
  not an acceptable keep

### 2026-04-04: Accepted local refactor candidate — centralize overlay mutation side effects
- status:
  measured against a contemporaneous serial detached baseline and kept locally
  in the active `full-artifact-opt` worktree; not yet promoted into the trusted
  artifact reading above
- implementation sketch:
  replace open-coded overlay mutation sequences inside each pattern-specific
  rewrite with a centralized mutation API:
  `factifyNode(...)`, `deactivateEdge(...)`, `rewriteEdgeInPlace(...)`,
  `addSyntheticEdge(...)`, plus explicit split-dirty note helpers for removed,
  added, and retained edge rewrites
- reason for trying it:
  the previous implementation mixed three concerns inside every pattern branch:
  pattern logic, overlay state mutation, and split-dirty bookkeeping. The
  helper-only refactor separates those concerns without changing the intended
  rewrite semantics, making later dirty-policy experiments much easier to
  express and review
- contemporaneous serial side-channel comparison against detached baseline
  `ef43f0f4f` on `P19/P20`, fixed seed `424242`, `3` runs each:
  baseline `P19 bdd_r 0.9272s`, helper `0.9677s`; baseline
  `P20 bdd_r 1.4133s`, helper `1.2212s`; helper-vs-baseline geometric mean
  `0.9497x`
- contemporaneous serial taint subset comparison against the same detached
  baseline on `and-roc, app-018, yaaic`:
  baseline implicit `45.98s, 15.40s, 7.47s`; helper implicit
  `46.18s, 15.22s, 7.41s`; helper-vs-baseline implicit geometric mean
  `0.9949x`
- important interpretation detail:
  the derived taint `no_rewrite / implicit_rewrite` speedup row moved from
  `2.8618x` to `2.7644x`, but this was caused by drift in the matched
  `no_rewrite` lane rather than a real slowdown in implicit rewrite itself
- stage-level taint reading:
  helper-only refactoring redistributed some overlay work
  (`and-roc/typefilter-dlog +431ms`, `and-roc/pt-obj-dlog -140ms`,
  `app-018/pt-obj-dlog +80ms`, `yaaic/typefilter-dlog -63ms`) while keeping the
  net implicit absolute time effectively flat
- conclusion:
  the central mutation-helper refactor is an acceptable architecture cleanup.
  The problematic direction was the later exact-dirty semantic narrowing, not
  the refactor itself

### 2026-04-05: Accepted local refactor candidate — isolate split dirty policy in `SplitDirtyTracker`
- status:
  measured against a contemporaneous detached baseline and then revalidated on
  fully serial taint reruns; kept locally in the active `full-artifact-opt`
  worktree
- implementation sketch:
  move the split-dirty queue and its policy-specific operations out of
  `ImplicitSplitOverlay` and into a dedicated `SplitDirtyTracker` helper.
  Pattern code still uses the same mutation API (`factifyNode`,
  `deactivateEdge`, `rewriteEdgeInPlace`, `addSyntheticEdge`), but split-dirty
  bookkeeping is now owned by one layer rather than being scattered as
  `noteSplitFactsFor*` calls throughout the overlay body
- reason for trying it:
  the helper-only refactor still left dirty-policy semantics implicit in every
  pattern branch. A separate tracker makes the architecture more intuitive and
  localizes future dirty-policy experiments without changing rewrite math or
  pattern matching
- contemporaneous side-channel comparison against detached baseline
  `3544411a5` on `P19/P20`, fixed seed `424242`, `3` runs each:
  baseline `P19 bdd_r 1.5690s`, tracker `1.8761s`; baseline
  `P20 bdd_r 2.5858s`, tracker `2.1560s`; tracker-vs-baseline geometric mean
  `0.9985x`
- first taint subset comparison on `and-roc, app-018, yaaic` looked mixed
  (`impl / baseline_impl = 1.0433x`), but that run overlapped with side-channel
  and baseline builds and therefore was not a trustworthy oracle
- serial taint subset revalidation on the same three cases:
  baseline implicit `52.06s, 17.07s, 8.51s`; tracker implicit
  `52.00s, 16.89s, 8.62s`; tracker-vs-baseline implicit geometric mean
  `1.0004x`
- serial stage-level taint reading:
  aggregate overlay bookkeeping was slightly better rather than worse:
  `implicit_total_ms 14605.91 -> 14246.41`,
  `overlay_prep_ms 12751.97 -> 12497.23`,
  `split_ms 3102.18 -> 3018.98`,
  `fastpath_ms 6539.47 -> 6349.73`,
  `rebuild_index_ms 3674.01 -> 3620.58`
- interpretation detail:
  the serial taint `no_rewrite / implicit_rewrite` geometric mean still moved
  (`2.5825x -> 2.8167x`), reinforcing the earlier pitfall that absolute
  implicit time and internal profiling are the right keep/drop signal for
  architecture-only refactors
- conclusion:
  `SplitDirtyTracker` is an acceptable organization cleanup. It improves the
  separation between mutation semantics and dirty-policy bookkeeping without a
  measurable side-channel regression and with essentially flat serial taint
  absolute time

### 2026-04-05: Accepted local refactor candidate — raise pattern rewrites onto semantic mutation helpers
- status:
  measured against a detached baseline built from `b7199897c` and kept locally
  in the active `full-artifact-opt` worktree; ready to use as the next
  checkpoint for deeper architecture cleanup
- implementation sketch:
  move remaining pattern-local edge-removal sequences onto higher-level
  semantic helpers:
  `removeEdge(...)`, `collapseEdgeToFact(...)`, and
  `replaceEdgesWithSyntheticEdge(...)`. Pattern branches no longer open-code
  `splitDirty_.noteEdgeRemoval(...) + deactivateEdge(...)`; they express only
  the intended rewrite outcome and let the mutation layer own the side effects
- reason for trying it:
  after introducing `SplitDirtyTracker`, pattern code still leaked
  mutation-policy details in several places. This helper lift keeps pattern
  detection/math separate from overlay mutation and makes later event-style
  refactors much more straightforward
- serial side-channel comparison against detached baseline `b7199897c` on
  `P19/P20`, fixed seed `424242`, `3` runs each:
  baseline `P19 bdd_r 1.0006s`, helper-lift `0.8829s`; baseline
  `P20 bdd_r 1.4358s`, helper-lift `1.3060s`; helper-lift-vs-baseline
  geometric mean `0.8959x`
- serial taint subset comparison against the same detached baseline on
  `and-roc, app-018, yaaic`:
  baseline implicit `46.47s, 14.85s, 7.58s`; helper-lift implicit
  `46.69s, 15.25s, 7.44s`; helper-lift-vs-baseline implicit geometric mean
  `1.0042x`
- serial stage-level taint reading:
  aggregate overlay profiling stayed essentially flat:
  `implicit_total_ms 12662.32 -> 12665.76`,
  `overlay_prep_ms 11155.50 -> 11129.58`,
  `rebuild_index_ms 3232.11 -> 3185.81`,
  `split_ms 2753.37 -> 2787.02`,
  `fastpath_ms 5667.36 -> 5623.34`,
  `siso_detect_ms 904.64 -> 905.41`,
  `siso_summarize_ms 1710.91 -> 1706.78`
- stage-level interpretation detail:
  the only noticeable absolute-stage slowdowns were
  `and-roc/pt-obj-dlog +353ms` and `app-018/pt-obj-dlog +287ms`, while
  `yaaic/pt-obj-dlog -158ms` and the aggregate implicit bookkeeping totals
  remained flat. This points to minor stage noise and downstream FC shape
  changes rather than a systemic overlay regression
- conclusion:
  the semantic mutation-helper lift is a safe architecture cleanup. It makes
  the implicit rewrite pipeline more intuitive without introducing measurable
  aggregate overhead, and it is a better base for future event-driven mutation
  refactors than the previous pattern-local bookkeeping style

### 2026-04-05: Rejected attempt — share one single-hyperedge classifier across direct and generic passes
- status:
  rejected and reverted locally; not part of the current keep set
- implementation sketch:
  move the duplicated single-hyperedge classifier logic into shared overlay
  helpers and reuse that shared classifier from both
  `rewriteSingleHyperedgePass` and `rewriteFastPathsToFixpoint`
- reason for trying it:
  after the mutation-helper lift, this looked like the next obvious cleanup:
  a single classifier layer seemed like a natural way to remove drift between
  the direct single-hyperedge pass and the generic fast-path selector
- side-channel outcome against detached baseline `b7199897c` on `P19/P20`,
  fixed seed `424242`, `3` runs each:
  baseline `P19 bdd_r 1.0006s`, shared-classifier `2.2915s`; baseline
  `P20 bdd_r 1.4358s`, shared-classifier `1.6736s`; geometric mean
  `1.6339x` slower
- profiling diagnosis on `P19 bdd_r`:
  final `rand_vars` stayed unchanged at `7306`, but
  `rewrite_ms 677 -> 2187`,
  `implicit_total_ms 479.54 -> 1939.63`, and especially
  `implicit_overlay_fastpath_single_ms 132.12 -> 1496.39`
- root cause:
  the two old classifier sites were not semantically identical. The generic
  fast-path selector carried an expensive `hasActivePath` acyclicity/path
  guard, while the direct single-hyperedge fixpoint pass used a cheaper local
  test. Sharing one classifier silently imported that path-BFS cost into the
  direct pass, which preserved correctness but destroyed side-channel
  performance
- conclusion:
  direct single-hyperedge rewriting and generic fast-path selection must keep
  their policy split explicit. Shared helpers are safe only when both the
  semantics and the cost model are truly aligned

### 2026-04-05: Accepted local refactor candidate — symmetric build/apply structure for direct local passes
- status:
  measured against the detached baseline used for the current
  `full-artifact-opt` line and kept locally in the active worktree
- implementation sketch:
  extend the earlier direct single-hyperedge `build/apply` split to
  `all-facts` as well. Both direct local passes now expose explicit candidate
  builders and mutation applicators:
  `buildAllFactsCandidate(...)` / `applyAllFactsCandidate(...)` and
  `buildDirectSingleHyperedgeCandidate(...)` /
  `applyDirectSingleHyperedgeCandidate(...)`
- reason for trying it:
  after the semantic mutation-helper lift and `SplitDirtyTracker`, the next
  remaining asymmetry was that `all-facts` still open-coded its detect/apply
  sequence while direct single-hyperedge already had a cleaner two-phase shape.
  Making the direct local passes structurally symmetric is a better base for
  any later generic worklist driver without touching rewrite math
- detached side-channel comparison against baseline
  `/tmp/cavfull_side_p19p20_refactorcheck_baseline` on `P19/P20`, fixed seed
  `424242`, `3` runs each:
  baseline `P19 bdd_r 1.3299s`, current `1.0077s`; baseline
  `P20 bdd_r 1.6046s`, current `1.2732s`; current-vs-baseline geometric mean
  `0.7754x`
- detached serial taint subset comparison against baseline
  `/tmp/cavfull_taint_subset_refactorcheck_baseline` on
  `and-roc, app-018, yaaic`:
  baseline implicit `45.22s, 16.22s, 8.19s`; current implicit
  `45.59s, 16.09s, 7.49s`; current-vs-baseline implicit geometric mean
  `0.9707x`
- serial stage-level taint reading:
  aggregate overlay profiling improved almost everywhere:
  `implicit_total_ms 12531.09 -> 12206.99`,
  `overlay_prep_ms 10986.06 -> 10764.29`,
  `split_ms 2796.86 -> 2751.00`,
  `fastpath_ms 5470.50 -> 5403.70`,
  `siso_detect_ms 938.17 -> 926.17`,
  `siso_summarize_ms 1655.60 -> 1568.63`;
  only `rebuild_index_ms` drifted slightly upward
  (`3057.64 -> 3074.86`)
- conclusion:
  this is a safe keep. It makes the direct local rewrite passes more uniform
  and easier to reason about, while remaining clearly on the no-regression side
  of both checked workloads

### 2026-04-05: Accepted local refactor candidate — explicit scheduling split between direct local passes and generic rounds
- status:
  measured against the same detached baseline used for the current
  `full-artifact-opt` line and kept locally in the active worktree
- implementation sketch:
  split `rewriteFastPathsToFixpoint(...)` into explicit scheduling phases:
  `ensureActiveEdgeIndicesWithStats(...)`,
  `runDirectLocalFastPaths(...)`, and
  `runGenericFastPathRounds(...)`.
  The direct local passes (`all-facts`, `single-hyperedge`) now have an
  explicit scheduling boundary relative to the generic
  `linear / parallel / fan-out` round loop, instead of being embedded as local
  lambdas inside one large function body
- reason for trying it:
  after making direct local passes structurally symmetric, the remaining
  architectural ambiguity was still at the scheduler level. The code knew about
  the direct-vs-generic split, but the structure did not make that boundary
  explicit. This refactor moves the organization closer to the intended
  mechanism without changing candidate semantics or classifier policy
- detached side-channel comparison against baseline
  `/tmp/cavfull_side_p19p20_refactorcheck_baseline` on `P19/P20`, fixed seed
  `424242`, `3` runs each:
  baseline `P19 bdd_r 1.3299s`, current `1.1286s`; baseline
  `P20 bdd_r 1.6046s`, current `1.4518s`; current-vs-baseline geometric mean
  `0.8763x`
- detached serial taint subset comparison against baseline
  `/tmp/cavfull_taint_subset_refactorcheck_baseline` on
  `and-roc, app-018, yaaic`:
  baseline implicit `45.22s, 16.22s, 8.19s`; current implicit
  `45.30s, 16.07s, 8.11s`; current-vs-baseline implicit geometric mean
  `0.9942x`
- serial stage-level taint reading:
  aggregate profiling stayed effectively flat while preserving the direct /
  generic boundary explicitly:
  `implicit_total_ms 12531.09 -> 12596.54`,
  `overlay_prep_ms 10986.06 -> 10983.93`,
  `split_ms 2796.86 -> 2769.53`,
  `fastpath_ms 5470.50 -> 5407.16`,
  `siso_detect_ms 938.17 -> 947.89`,
  `siso_summarize_ms 1655.60 -> 1594.10`,
  `rebuild_index_ms 3057.64 -> 3045.77`
- conclusion:
  this is also a safe keep. It does not materially change runtime, but it makes
  the intended scheduler architecture visible in the code: direct local passes
  are their own phase, and generic fast-path rounds are their own phase. That
  is a cleaner base for any later event-driven scheduler work

### 2026-04-05: Accepted local refactor candidate — explicit round/result carriers for fast-path scheduling
- status:
  measured against the same detached baseline used for the current
  `full-artifact-opt` line and kept locally in the active worktree
- implementation sketch:
  keep the direct-vs-generic scheduler split, but replace the remaining
  scheduler booleans with explicit result carriers:
  `DirectLocalFastPathResult` and `GenericFastPathRoundResult`.
  `runDirectLocalFastPaths(...)` now reports both
  `changed` and `activeEdgesExhausted`, and generic scheduling runs one
  explicit `runGenericFastPathRound(...)` per iteration before the outer loop
  decides whether to continue
- reason for trying it:
  after separating the direct local phase from generic rounds, the control flow
  still encoded scheduler state through a small cluster of booleans
  (`changed`, `changedAny`, `rebuiltBeforeLocal`, `localChanged`). That was
  still less direct than the intended mechanism. Making the per-phase result
  explicit is a cleaner base for any later event- or worklist-driven scheduler
  changes, while keeping classifier policy and rewrite math untouched
- detached side-channel comparison against baseline
  `/tmp/cavfull_side_p19p20_refactorcheck_baseline` on `P19/P20`, fixed seed
  `424242`, `3` runs each:
  baseline `P19 bdd_r 1.3299s`, current `1.1729s`; baseline
  `P20 bdd_r 1.6046s`, current `1.6552s`; current-vs-baseline geometric mean
  `0.9538x`
- detached serial taint subset comparison against baseline
  `/tmp/cavfull_taint_subset_refactorcheck_baseline` on
  `and-roc, app-018, yaaic`:
  baseline implicit `45.22s, 16.22s, 8.19s`; current implicit
  `45.06s, 16.51s, 7.92s`; current-vs-baseline implicit geometric mean
  `0.9936x`
- serial stage-level taint reading:
  aggregate profiling improved slightly across every tracked overlay bucket:
  `implicit_total_ms 12531.09 -> 12487.21`,
  `overlay_prep_ms 10986.06 -> 10920.13`,
  `split_ms 2796.86 -> 2773.52`,
  `fastpath_ms 5470.50 -> 5404.47`,
  `siso_detect_ms 938.17 -> 936.94`,
  `siso_summarize_ms 1655.60 -> 1633.80`,
  `rebuild_index_ms 3057.64 -> 3003.28`
- conclusion:
  this is another safe keep. It does not change rewrite semantics, but it
  makes the scheduler state machine itself more explicit and slightly reduces
  overlay bookkeeping on the checked taint subset while staying positive on the
  checked large side-channel cases overall

### 2026-04-05: Accepted local refactor candidate — collect fast-path enable flags into scheduler options
- status:
  measured against the same detached baseline used for the current
  `full-artifact-opt` line and kept locally in the active worktree
- implementation sketch:
  replace repeated five-boolean plumbing between
  `rewriteFastPathsToFixpoint(...)`, direct local passes, generic rounds, and
  candidate collection with a single `FastPathScheduleOptions` carrier. The
  direct phase, generic rounds, and candidate collector now all consume the
  same scheduler options object instead of re-spelling the enable bits in each
  call signature
- reason for trying it:
  after making phase results explicit, the remaining scheduler-level ad hoc
  piece was the repeated manual propagation of
  `enableSingleHyperedge / enableLinearTwoEdge / enableParallelEdge /
  enableFanOutConverge / enableAllFacts`. Collecting these into one local
  carrier makes the intended control surface clearer and removes another source
  of accidental scheduler divergence without touching any rewrite math
- detached side-channel comparison against baseline
  `/tmp/cavfull_side_p19p20_refactorcheck_baseline` on `P19/P20`, fixed seed
  `424242`, `3` runs each:
  baseline `P19 bdd_r 1.3299s`, current `1.0566s`; baseline
  `P20 bdd_r 1.6046s`, current `1.6064s`; current-vs-baseline geometric mean
  `0.8919x`
- detached serial taint subset comparison against baseline
  `/tmp/cavfull_taint_subset_refactorcheck_baseline` on
  `and-roc, app-018, yaaic`:
  baseline implicit `45.22s, 16.22s, 8.19s`; current implicit
  `44.68s, 16.36s, 8.10s`; current-vs-baseline implicit geometric mean
  `0.9952x`
- serial stage-level taint reading:
  total implicit time stayed effectively flat
  (`implicit_total_ms 12531.09 -> 12529.67`), with mixed but small internal
  bucket movement:
  `overlay_prep_ms 10986.06 -> 11059.54`,
  `split_ms 2796.86 -> 2712.69`,
  `fastpath_ms 5470.50 -> 5615.91`,
  `siso_detect_ms 938.17 -> 937.27`,
  `siso_summarize_ms 1655.60 -> 1624.31`,
  `rebuild_index_ms 3057.64 -> 3216.60`
- conclusion:
  this is still a safe keep. It is primarily an interface cleanup, but it
  makes the scheduler configuration surface more coherent while remaining
  effectively neutral on taint and still positive on the checked large
  side-channel cases

### 2026-04-05: Accepted local refactor candidate — split generic round planning from execution
- status:
  measured against the same detached baseline used for the current
  `full-artifact-opt` line and kept locally in the active worktree
- implementation sketch:
  introduce an explicit `GenericFastPathRoundPlan` and split generic rounds
  into `buildGenericFastPathRoundPlan(...)` and
  `applyGenericFastPathRoundPlan(...)`. `runGenericFastPathRound(...)` now only
  orchestrates one round: build the selected generic candidates, apply them,
  rebuild indices if needed, then run the direct local phase
- reason for trying it:
  after the schedule-options cleanup, generic rounds still mixed three
  concerns in one helper: candidate collection, candidate execution, and
  post-round orchestration. This refactor makes generic rounds structurally
  match the rest of the codebase better without touching candidate semantics or
  rewrite math
- detached side-channel comparison against baseline
  `/tmp/cavfull_side_p19p20_refactorcheck_baseline` on `P19/P20`, fixed seed
  `424242`, `3` runs each:
  baseline `P19 bdd_r 1.3299s`, current `1.0253s`; baseline
  `P20 bdd_r 1.6046s`, current `1.5259s`; current-vs-baseline geometric mean
  `0.8562x`
- detached serial taint subset comparison against baseline
  `/tmp/cavfull_taint_subset_refactorcheck_baseline` on
  `and-roc, app-018, yaaic`:
  baseline implicit `45.22s, 16.22s, 8.19s`; current implicit
  `46.25s, 15.52s, 7.65s`; current-vs-baseline implicit geometric mean
  `0.9705x`
- serial stage-level taint reading:
  total implicit time stayed essentially flat while internal buckets moved only
  slightly:
  `implicit_total_ms 12531.09 -> 12539.71`,
  `overlay_prep_ms 10986.06 -> 11066.26`,
  `split_ms 2796.86 -> 2810.22`,
  `fastpath_ms 5470.50 -> 5426.26`,
  `siso_detect_ms 938.17 -> 974.99`,
  `siso_summarize_ms 1655.60 -> 1618.65`,
  `rebuild_index_ms 3057.64 -> 3013.92`
- conclusion:
  this is a safe keep. It gives generic rounds the same kind of explicit
  plan/apply structure already used in direct local passes, improves the
  checked large side-channel cases, and remains effectively neutral on taint
  absolute time

## Related commits
- `UNCOMMITTED` — docs(research): log rejected and candidate implicit overlay optimization attempts
- `UNCOMMITTED` — docs(research): refresh curated rewrite status around the packaged full artifact
- `00565d0` — feat(artifact): package maintained CAV full workflows
- `7d9d45b72` — feat(problog): add implicit split rewrite pipeline
