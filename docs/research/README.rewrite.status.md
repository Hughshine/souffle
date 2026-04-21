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
- This pair records the pre-unified packaged full artifact baseline. The local
  unified checkpoint below folds in additional rewrite, symbolization, string,
  and aggregation work that is not yet a published artifact tip.
- benchmark side:
  - `problog-benchmark/.worktree/clones/CAV-FULL`
  - branch `CAV-FULL`
  - packaged tip `00565d05fb576b66ec624bc601e63c94299e020a`
- compiler side:
  - `.worktree/clones/full-artifact`
  - branch `full-artifact`
  - paired tip `7d9d45b72fb59657062d07348e7361d1ecdc20f8`

## Trusted Current Conclusions
- In the local unified checkpoint, bare `--rewrite` is the artifact-facing
  smart dispatcher.
- Older packaged full-artifact runs still used the explicit `--rewrite` /
  `--implicit-rewrite` distinction; keep that distinction only for historical
  comparison and diagnostic controls.
- `--explicit-rewrite` is now the non-smart spelling for the legacy explicit
  graph rewrite lane.
- `--implicit-rewrite` remains the direct implicit-split diagnostic pipeline.
- The maintained full artifact now defaults to implicit rewrite for the
  side-channel rewrite comparison.
- `RQ3` is no longer treated as a separate workload; it is derived from the
  validated `RQ2` standard runs.
- Iterative implicit rewrite remains experimental and is not part of the current
  packaged full artifact contract.

## Local Unified Checkpoint: 2026-04-21
- compiler workspace:
  `.worktree/clones/full-artifact-opt-unified-wt`
- branch:
  `artifact-rewrite-followups-20260421`
- base commit before local changes:
  `c306be2731b0 perf(fc): defer dep-graph depths until cyclewise build`
- benchmark workspace:
  `problog-benchmark/.worktree/clones/CAV-FULL`
- symbolization benchmark commit:
  `c33d371 artifact: add DDisasm symbolization benchmark`

Implemented in the local compiler checkpoint:
- removed the temporary `--rewrite-compaction-only` CLI path
- made bare `--rewrite` a smart dispatcher:
  probabilistic rules select implicit rewrite with naive split; deterministic
  rules select legacy no-split rewrite
- kept explicit control flags for diagnosis:
  `--explicit-rewrite` forces legacy explicit graph rewrite,
  `--implicit-rewrite` still forces implicit rewrite, and
  `--split-mode=no-split` still forces no-split legacy rewrite
- added detector-level feature masking so disabled SISO detectors are not still
  paid for before post-filtering
- added a no-split follow-up heuristic:
  after a pass that rewrites only linear/parallel regions, the next detector
  pass skips fact-oriented detectors
- added a no-split primary detector mask:
  deterministic no-split rewrite starts with fact-absorption patterns and falls
  back to the full detector only on zero-hit passes, avoiding initial
  linear/parallel/fan-out scans on symbolization-shaped graphs
- changed component subgraph partitioning from full SCC dependency-graph
  construction to direct weak-component partitioning over the final derivation
  view; SCC scheduling is still built inside slow components when formulas are
  actually compiled
- changed evidence grouping to use the already-built component ids, with an
  empty-evidence fast return
- when component fast paths are disabled, component analysis skips detailed
  directed-cycle classification that is only needed to guard those fast paths
- split component classification timing into pure classification and fast
  component evaluation wall time
- for deterministic no-split auto-dispatch, disabled the single-rand component
  fast path by default because on symbolization it moves many tiny exact
  computations into a slower per-component evaluator; the BDD path remains
  exact on the checked cases
- restored global deterministic edge compaction for the no-split path. A
  dirty-only compaction restriction introduced in the first smart-dispatch
  checkpoint removed fewer redundant edges on symbolization and caused `troff`
  to abort in CUDD formula construction.

Current symbolization reading from
`/tmp/symbolization_head_fix_20260421/full_rerun/summary.tsv`:
- cases:
  selected table uses `bison, cluster, flex, gawk, gcc11, gpgsm, gpp11,
  gvmap, llvm-config, llvm-pdbutil, readelf, tc, tmux, troff, wget`; `sshd`
  is kept as an extra small smoke input.
- bare `--rewrite` dispatch:
  all symbolization stages selected `auto-legacy-no-split` because the rules are
  deterministic; input facts remain probabilistic
- completion:
  rewrite completed `15/15` selected cases; plain completed `10/15`; plain
  aborted on `flex, gcc11, gpp11, tmux, troff`
- among cases where both modes completed:
  rewrite was faster on `10/10`, with geometric-mean speedup `1.75x`
- plain aborted while rewrite completed on:
  `flex, gcc11, gpp11, tmux, troff`
- root cause of the transient `troff` regression:
  the SUM aggregate replay increased the graph load, but `c306be273` and the
  fixed branch both completed. The abort was introduced by dirty-only
  no-split compaction in `33a7b3912`; it left `edgesRemoved=60218` /
  `edgesAdded=46568` after rewrite instead of the working
  `edgesRemoved=68712` / `edgesAdded=55062` shape.
- representative positive cases:
  `llvm-config 11.86s -> 4.85s`, `llvm-pdbutil 8.95s -> 4.29s`,
  `readelf 17.73s -> 9.36s`, `troff plain abort -> 37.14s`,
  `wget 20.61s -> 7.89s`
- correctness:
  all both-completed cases had identical output keys; most were byte-exact.
  `readelf` had two values and `wget` had one value differing by `1e-8`, i.e.
  the last printed decimal under the current 8-decimal output format. The
  audited cases are all `labeled_ea` outputs whose probability is an OR over
  one string object and two symbolic-data supports, with the exact decimal
  value landing on an 8-digit rounding boundary. Example:
  `1 - (1 - 0.95) * (1 - 0.2491) * (1 - 0.275) = 0.972779875`.

2026-04-21 follow-up optimization on `readelf`
(`/tmp/symbolization_opt12_20260421`):
- command:
  `compute_final -F .../symbolization_benchmark/data/inputs/readelf -D /tmp/symbolization_opt12_20260421/readelf_final --det-opt --rewrite`
- output check:
  `facts.prob` matched the earlier optimized run byte-for-byte
- detector scheduling effect:
  iteration 1 skipped linear/parallel/fan-out scans and detected the same
  `21523` single-hyperedge plus `19700` all-facts regions; iteration 3 kept
  the previous linear/parallel-only zero-hit check without falling back to full
  detection
- representative final timing:
  `rewrite took 1673 ms`, down from the pre-optimization profiled
  `2133 ms` on the same `readelf` host shape
- component partition effect:
  `component subgraph build took 472 ms` in the final rerun, compared with the
  earlier SCC dependency-graph-backed profile of `1551 ms`
- other final fields:
  `BDD formula build 108 ms`, `component analysis 139 ms`,
  `evidence grouping 0 ms`, `component classification 12 ms`,
  `per-node conditional WMC 20 ms`
- rejected micro-optimization:
  pre-counting component node/edge sizes to reserve `unordered_set`s increased
  component subgraph build time (`391 ms -> 525 ms` in local reruns), so it was
  reverted.

Current side-channel smoke from
`/tmp/cavfull_smoke_smart_script_20260421`:
- case:
  `P1` technical smoke, after migrating the CAV-FULL script from
  `--implicit-rewrite` to bare `--rewrite`
- bare `--rewrite` dispatch:
  `auto-implicit split_mode=naive-split`; standard RQ2 had
  `total_rules=19 probabilistic_rules=1`
- output:
  `bdd_r` matched `bdd` with `max|delta|=0`
- time:
  standard RQ2 `0.015s -> 0.012s`; probabilistic-enhanced RQ2 remained
  consistent (`max|delta|=0`) with essentially equal wall time on this tiny
  smoke case

Current taint smoke from
`/tmp/cavfull_smoke_smart_script_20260421`:
- case:
  `angulo` technical smoke, after migrating the `implicit_rewrite` variant from
  `--implicit-rewrite` to bare `--rewrite`
- full stage chain:
  `cipt-cg-dlog, pre-dlog, typefilter-dlog, pt-obj-dlog, taint-lim-dlog`
- bare `--rewrite` dispatch:
  probabilistic taint stages selected `auto-implicit split_mode=naive-split`;
  `pt-obj-dlog` had `total_rules=37 probabilistic_rules=37`
- output:
  `implicit_rewrite` consistency vs `no_rewrite`: yes across 5 checked stages,
  `max|delta|=0`
- time:
  `0.59s -> 0.25s` on this small smoke case

Current interpretation:
- side-channel and taint validate the intended smart-dispatch shape:
  probabilistic-rule workloads still get implicit split and retain large speedup
- symbolization validates that deterministic-rule workloads need no-split
  component-wise processing, but raw SISO rewrite is not the main source of
  speedup there; the useful effect is deterministic/independence decomposition
  plus component-specialized evaluation
- symbolization now shows a useful selected-case optimized path, including
  cases where plain aborts, but still needs more engineering before claiming a
  uniform `2x` speedup. Correctness should be reported with a `1e-8` tolerance
  unless the output writer is changed to avoid rounding-boundary bitwise drift.

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

### 2026-04-05: Rejected attempt — encode generic-round continuation policy on result methods
- status:
  measured once, then reverted locally; not kept in `full-artifact-opt`
- implementation sketch:
  add convenience methods such as `madeProgress()` and `shouldRunAnotherRound()`
  onto `DirectLocalFastPathResult` and `GenericFastPathRoundResult`, and use
  those helpers to simplify `runGenericFastPathRounds(...)` by removing the
  explicit tail `continue` branch
- reason for trying it:
  after splitting generic-round planning from execution, the remaining obvious
  scheduler cleanup was to make the round result itself carry the continuation
  policy, so the loop body consumed a semantic result instead of raw fields
- measured outcome on the usual detached baseline comparison:
  side `P19/P20` and taint `and-roc/app-018/yaaic` both regressed sharply in
  one round of runs:
  side `bdd_r` geometric mean rose to `1.3017x` of baseline, taint absolute
  implicit time rose to `1.1793x`, and aggregate overlay profiling buckets
  also rose together (`implicit_total_ms 12531.09 -> 16408.99`)
- interpretation:
  this should be treated as an untrusted noisy run rather than as a credible
  code-signal regression. The code change was too small and semantically
  equivalent to explain a simultaneous `~30%` increase across side-channel,
  taint, and internal overlay profiling, while the same run also showed clear
  machine-load inflation in shared compile and stage wall times
- conclusion:
  do not keep this exact cleanup as measured. If this continuation-policy
  cleanup is revisited, it needs a fresh low-load rerun before any keep/drop
  decision. The current branch stays on the previous safe checkpoint

### 2026-04-05: Submission-ready regression checkpoint on `full-artifact-opt`
- status:
  full regression rerun completed on branch head `715c24c5c`; this is the
  current submission-ready checkpoint for the refactoring stack recorded above
- verification completed:
  repo regression suite:
  `ctest --test-dir build -L regression --output-on-failure --progress -j4`
  passed `4/4`
- side-channel `RQ2` standard/full rerun:
  `/tmp/cavfull_submit_regress_standard/RQ2result.tsv`
  completed for all available cases. `P2` remained the known missing-source
  case. For the `19` comparable `bdd`/`bdd_r` rows, `bdd_r` matched `bdd` on
  every checked case and the geometric-mean end-to-end speedup was `2.97x`.
  The only measured slowdown was `P12` (`0.87x`)
- side-channel `RQ2` stress/full_probabilistic_enhanced rerun:
  `/tmp/cavfull_submit_regress_stress_full_probabilistic_enhanced/RQ2result.tsv`
  completed for all available cases. `P2` again remained the known
  missing-source case. For the `18` comparable rows, `bdd_r` matched `bdd` on
  every checked case and the geometric-mean end-to-end speedup was `7.04x`.
  `P1` and `P3` were small-case slowdowns (`0.57x` and `0.50x`). `P19` plain
  `bdd` timed out at `300s`, so the recorded `bdd_r` mismatch there remains the
  known artifact of comparing against an incomplete baseline rather than a new
  correctness regression
- taint appendix rerun:
  `/tmp/cavfull_submit_taint_full/reports/appendix-summary.md`
  and `/tmp/cavfull_submit_taint_full/reports/appendix-cases.tsv`
  completed for all `15` bundled cases under `no_rewrite` and
  `implicit_rewrite`. Implicit final-stage consistency was `15/15`, and the
  geometric-mean `no_rewrite / implicit_rewrite` speedup was `2.46x`
- conclusion:
  the current branch head is stable enough to submit. The refactoring stack did
  not introduce new correctness regressions in the checked side-channel or
  taint workflows, and the remaining exceptions are the previously understood
  benchmark issues (`P2` missing source and `stress P19` baseline timeout)

### 2026-04-06: Rejected attempt — Dice-inspired graph-level fact compaction
- status:
  implemented as a local probe, measured under fixed-seed side and taint
  comparisons, then reverted from the kept code path; not part of the current
  `full-artifact-opt` baseline
- design source:
  inspired by `pdatalog-dice`'s `merge_literal_products` simplify pass and by
  the existing explicit-rewrite edge compaction path
- implementation sketch:
  add an implicit direct-local pass that absorbs eligible fact-only inputs from
  a single active edge into that edge's probability/support bundle when the
  fact looks private enough to preserve semantics
- reason for trying it:
  this is the cleanest derivation-graph-level analogue of a useful formula-side
  simplification: collapse a local literal product without switching the whole
  runtime to per-query formula construction
- fixed-seed side-channel reading on `P19/P20`:
  baseline `bdd_r` wall time was `3.1506s`, `3.1428s`; the probe measured
  `3.2088s`, `3.0600s`; patch/baseline wall-time geometric mean: `0.9958x`
- fixed-seed taint subset reading on `and-roc, app-018, yaaic`:
  baseline implicit totals were `0.4473s`, `0.3971s`, `0.2135s`; the probe
  measured `0.4235s`, `0.3766s`, `0.2022s`; patch/baseline absolute-implicit
  geometric mean: `0.9473x`
- critical profiling result:
  `implicit_overlay_fastpath_compaction_ms` stayed `0.000000` on the checked
  side and taint runs, so the observed timing movement cannot be credited to
  actual compaction opportunities firing
- interpretation:
  the derivation-graph analogue is still conceptually plausible, but the
  current eligibility rule is either too conservative or simply not matching
  the checked benchmark shapes. Without evidence that the pass actually fires
  on real workloads, it should not be promoted into the maintained branch
- conclusion:
  keep the idea as a research note only. `pdatalog-dice` remains useful as a
  source of structural simplification ideas, but the first direct DG-level
  analogue tried here did not produce workload-backed evidence strong enough to
  keep

### 2026-04-19: DDisasm pass exploration — symbolization is the best current host
- scope:
  evaluate representative DDisasm-inspired `.dl` slices under the maintained
  `full-artifact-opt` binary to determine whether the task family is suitable
  for inclusion as a rewrite benchmark
- task reading from the original project/paper:
  DDisasm is a Datalog-based disassembler for stripped binaries that first
  decodes a superset of possible instructions, then uses Datalog analyses to
  recover code locations, symbolization, and function boundaries before
  emitting GTIRB / reassembleable assembly
- pass-level reading:
  - `function_inference`: poor host; dominated by shared `next_block` /
    reachability / ownership propagation, not weakly shared local proof cones
  - `code_inference` heuristic slice: not useful in the current probabilistic
    benchmark form; exact inference collapses to deterministic/precomputed
    outputs and does not exercise the intended rewrite effect
  - `symbolization` data-object slice: best current host; it models the real
    uncertainty in binary recovery (literal vs. symbol/data-object
    interpretation) and rewrite is structurally useful on it
- host caveat / TODO:
  the extracted DDisasm symbolization host currently uses a temporary
  numeric encoding for some string-valued `type` / `reason` style fields to
  stay off the remaining `symbol`-typed variable gap in the maintained
  ProbLog derivation/runtime path. Once native end-to-end `symbol/string`
  support is fully restored for those variable-bearing paths, revert this host
  back to its original string-native representation instead of keeping the
  numeric surrogate encoding.
- representative maintained experiments:
  - `symbolization_data_object / symbol_medium`
    - plain: `0.11s`
    - implicit: `0.10s`
    - outputs equal
    - no graph-level regions / no RV shrink, but rewrite decomposes one larger
      FC problem into many tiny components
  - larger `symbolization_data_object` synthetic inputs after the FC
    placeholder-negation fix:
    - `num_slots=12000`
      - plain: `7.26s`
      - implicit: `7.64s`
      - outputs equal
      - `randomVars 17000 -> 17000`
    - `num_slots=30000`
      - plain: `20.16s`
      - implicit: `21.64s`
      - legacy `--rewrite`: `18.41s`
      - outputs equal across all three modes
      - `randomVars` stay fixed (`42500`)
    - `num_slots=60000`
      - plain: `44.37s`
      - implicit: `46.24s`
      - legacy `--rewrite`: `40.98s`
      - outputs equal across all three modes
      - `randomVars` stay fixed (`85000`)
    - interpretation:
      on this host, rewrite usefulness comes from structural
      factorization/decomposition rather than from reducing the number of
      probabilistic variables. The maintained implicit pipeline is near parity
      but not yet better than plain, while explicit/legacy rewrite becomes a
      genuine end-to-end win on the larger symbolization-shaped inputs
  - `code_inference_heuristic / codeinf_medium`
    - plain: `0.04s`
    - implicit: `0.05s`
    - outputs equal
    - exact lane effectively sees `random_vars=0` after pruning/precomputation
- conclusion:
  if DDisasm contributes a benchmark to this work, it should come from a
  symbolization-style recovery host, not from function-boundary propagation or
  the currently extracted code-inference heuristic slice. The most defensible
  claim is therefore: DDisasm exposes naturally uncertain binary-recovery
  tasks, and a representative symbolization-oriented Datalog host is
  compatible with this work. On that host, rewrite is effective because it
  structurally factorizes the exact evaluation problem even though random
  variable counts stay fixed; in the maintained line this benefit is already
  visible end-to-end for explicit rewrite on the larger extracted cases.
- caveat / TODO:
  the current DDisasm-derived benchmark line still uses synthetic/generated
  case families (`synthetic_originalish`, `synthetic_symbolization_data_object`,
  etc.), not real DDisasm facts extracted from the original toolchain. This is
  sufficient for host-shape exploration, but not sufficient for a final paper
  claim that the benchmark is a real binary-recovery workload.
- paper grounding:
  the original DDisasm paper evaluates on `200` benchmark programs:
  `106` Coreutils, `69` DARPA CGC binaries, and `25` real-world open-source
  applications. Compiled across the reported compiler/optimization matrix,
  this becomes `7658` binaries totaling `888 MB` of input data. The paper's
  whole-tool timing claim is that DDisasm is faster than Ramblr on all but
  `294` of `7658` binaries and is `4.9x` faster on average.
- concrete next-step TODO:
  replace at least a small representative subset of the synthetic
  symbolization-oriented cases with real DDisasm example/test binaries and
  their exported Souffle relations. The most promising local candidates in
  `/tmp/ddisasm` are:
  - `examples/asm_examples/ex_symbolic_operand_heuristics`
  - `examples/asm_examples/ex_referred_string`
  - `examples/ex_symbol_selection`
  - `examples/ex_pointerReattribution3`
  - `examples/ex_confusing_data`
  The local DDisasm tests already expose the right extraction mechanism via
  `--with-souffle-relations`; the remaining blocker is environment/tooling
  availability (`ddisasm`/`gtirb` not currently installed in this workspace),
  not uncertainty about which real workloads to use.

### 2026-04-19: DDisasm symbolization — alias-aware direct commit keeps implicit close to explicit
- scope:
  continue the representative synthetic `symbolization_data_object /
  num_slots=60000` host until plain, explicit, and implicit are compared on
  the same freshly recompiled `compute`, same binary lineage, same input, and
  no concurrent profiling noise
- old blind spot:
  the first `full-artifact-opt` direct-commit path flattened overlay
  `SplitNodeRef{base, alias}` inputs back to plain base `NodePtr`s before the
  committed graph reached later exact evaluation. That preserved semantics but
  discarded the alias-separated graph shape that the materialized path kept
  alive via `_split_shadow_*` nodes.
- kept fix:
  direct commit is now alias-aware. `OverlayEdgeCommit` preserves full
  `SplitNodeRef` inputs, and `applyImplicitOverlayCommit()` creates local
  shadow fact nodes on demand for aliased inputs before reconstructing the
  committed hyperedges.
- follow-up review fix:
  subagent review found two additional alias-path correctness risks, both now
  fixed in the FAOPT worktree:
  - `collectEdgeCommits()` no longer treats an alias-only input rewrite as
    “unchanged”; non-zero alias ids now force the edge back through the
    direct-commit patch set
  - `materializeToGraph()` shadow fact nodes now copy probabilistic support
    tokens from the base fact, keeping the materialized alias path aligned
    with direct commit
- matched full exact run on the representative host:
  - plain:
    - total `49.35s`
    - `CREATE_GRAPH_FULL = 10.50s`
    - `FORWARD_COMPILATION_FULL = 32.78s`
    - `manager_init_vars = 170000`
    - `memory_usage_mb = 2989.59`
    - `reordering_runtime = 17.970`
  - explicit / legacy:
    - total `28.94s`
    - `FC_WMC_HYBRID = 12.75s`
    - `rewrite_ms = 6006`
    - `rewrite_detect_total_ms = 3820.30`
    - `component_build_subgraphs_ms = 4514`
    - `manager_init_vars = 8`
    - `total_nodes = 49`
    - `memory_usage_mb = 16.07`
  - implicit / alias-aware direct commit:
    - total `32.30s`
    - `FC_WMC_HYBRID = 18.23s`
    - `rewrite_ms = 10845`
    - `implicit_total_ms = 4272.74`
    - `implicit_overlay_prep_ms = 4128.84`
    - `implicit_overlay_split_ms = 414.50`
    - `implicit_overlay_fastpath_ms = 2550.15`
    - `implicit_materialize_ms = 0`
    - `component_build_subgraphs_ms = 4836`
    - `manager_init_vars = 8`
    - `total_nodes = 49`
    - `memory_usage_mb = 16.07`
- correctness:
  outputs match exactly across all three runs:
  - `symbolic_data.csv`
  - `labeled_ea.csv`
- interpretation:
  the direct-commit alias-loss was a real implementation gap. Once the commit
  path preserves alias structure, implicit lands much closer to explicit and
  the final exact problem size aligns (`manager_init_vars=8`, `total_nodes=49`)
  without paying materialization cost.
- current reading:
  - the extracted symbolization host remains the best DDisasm-derived benchmark
    candidate
  - the remaining implicit/explicit gap is no longer about semantic handoff
    blind spots
  - it is now primarily an implementation-cost gap:
    - overlay prep / fastpath bookkeeping
    - a still-necessary post-commit graph rewrite/split tail
    - slightly higher component build / classification cost
- probe result:
  skipping the post-commit `runLegacyRewrite()` after alias-aware direct
  commit looked tempting because it reduced `rewrite_ms` from `10845` to
  `7770`, but it is not valid. On the same host:
  - total regressed to `42.34s`
  - `FC_WMC_HYBRID` regressed to `28.74s`
  - `manager_init_vars` jumped back to `80000`
  - `total_nodes` jumped to `35016`
  - `fc_build_ms` jumped to `9816`
  So the post-commit graph rewrite is still doing necessary structural
  cleanup; it cannot simply be removed to close the gap.
- TODO:
  `CREATE_GRAPH_FULL` is now a recurring fixed cost on the larger DDisasm
  symbolization hosts (about `9.8s` on the representative `num_slots=60000`
  run) and should be treated as a separate optimization target rather than
  something rewrite can hide.
- TODO:
  the stage-level `rewrite_ms` reported by the pipeline is materially larger
  than the `GraphRewriter` self-reported rewrite total on the same explicit
  run. The remaining gap appears to come from pre/post rewrite work that is
  outside the current `GraphRewriter` iteration timing, and the instrumentation
  should be tightened before making stronger claims about rewrite-engine
  overhead.

### 2026-04-19: DDisasm symbolization — apples-to-apples profiling on the instrumented FAOPT binary
- scope:
  rebuild the representative `symbolization_data_object / num_slots=60000`
  case with the same instrumented binary for plain, explicit `--rewrite`, and
  implicit `--implicit-rewrite`, then explain where the time actually goes
  using the stage JSON rather than ad hoc stdout summaries.
- representative runs:
  - plain:
    - total `46.49s`
    - `CREATE_GRAPH_FULL = 8.81s`
    - `FORWARD_COMPILATION_FULL = 29.22s`
    - `WEIGHTED_MODEL_COUNTING_FULL = 0.83s`
  - explicit:
    - total `43.68s`
    - `CREATE_GRAPH_FULL = 9.11s`
    - `FC_WMC_HYBRID = 27.13s`
  - implicit:
    - total `49.44s`
    - `CREATE_GRAPH_FULL = 8.90s`
    - `FC_WMC_HYBRID = 32.54s`
- plain diagnosis:
  - `rand_vars = 85000`
  - `manager_init_vars = 170000`
  - `memory_usage_mb = 2989.59`
  - `reordering_runtime = 17.29s`
  - `ForwardCompilation` itself reports `Insertion time: 22370 ms`
  - interpretation:
    plain is not “too light”; it survives because CUDD spends a large amount
    of time reordering and inserting into a still-regular enough DD problem.
- explicit diagnosis:
  - final exact problem is almost gone:
    - `manager_init_vars = 8`
    - `total_nodes = 49`
    - `memory_usage_mb = 16.07`
    - `reordering_runtime = 0`
  - the hybrid stage is still expensive because it is now dominated by
    bookkeeping rather than DD work:
    - `rewrite_ms = 15538`
    - `component_build_subgraphs_ms = 4129`
    - `component_analyze_ms = 277`
    - `component_classify_ms = 1681`
    - `fc_build_ms = 1`
    - `wmc_ms = 5`
  - the residual gap between `FC_WMC_HYBRID` and the sum of these counters is
    about `5.5s`; the strongest current explanation is logging overhead.
- implicit diagnosis:
  - final exact problem also collapses to the same residual shape:
    - `manager_init_vars = 8`
    - `total_nodes = 49`
    - `memory_usage_mb = 16.07`
    - `reordering_runtime = 0`
  - the hybrid stage is slower than explicit mainly because the overlay path
    adds more rewrite-side bookkeeping:
    - `rewrite_ms = 18352`
    - `implicit_total_ms = 12095`
    - `implicit_overlay_prep_ms = 3683`
    - `implicit_overlay_split_ms = 366`
    - `implicit_overlay_fastpath_ms = 2270`
    - `implicit_materialize_ms = 2805`
    - `implicit_graph_rewrite_ms = 5251`
    - `component_build_subgraphs_ms = 5198`
    - `component_analyze_ms = 329`
    - `component_classify_ms = 1994`
    - `fc_build_ms = 4`
    - `wmc_ms = 2`
- newly confirmed profiling issue:
  both explicit and implicit hybrid runs currently emit enormous per-component
  diagnostics inside the timed stage:
  - `buildFormulasCyclewise()` logs three `INFO` strings per invocation
    (`preConfig`, `Total rounds`, `Insertion time`)
  - on this host that ends up being `60000` invocations and `180000` such
    strings in one stage
  - the hybrid stage also records `20000` per-component fast-path lines
  - resulting stage JSONs are about `9.7 MB` each
  - this logging is the best current explanation for most of the remaining
    `5.5s–6.7s` residual inside `FC_WMC_HYBRID`
- newly confirmed timing-boundary issue:
  `--dumpjson` itself contributes several seconds that are currently outside the
  main stage accounting:
  - every mode writes `derivation.json` after pruning and before the hybrid/full
    exact stage; on the representative case this file is about `216 MB`
  - rewrite modes also write `rewrite_final.json` after rewrite and before the
    exact solver work inside `FC_WMC_HYBRID`; on the representative case this is
    about `178 MB` for explicit and `195 MB` for implicit
  - these untimed dumps explain most of the `~4s` top-level gap between
    `turn_total` and the sum of stage times, and part of the remaining hybrid
    residual beyond the explicit `rewrite_ms` / component counters
- current reading:
  - rewrite is real and strong; both rewrite modes destroy the global DD
    problem
  - end-to-end speedup is still modest because:
    - `CREATE_GRAPH_FULL` is a fixed `~9s` front cost
    - rewrite/hybrid bookkeeping is expensive
    - the current hybrid implementation is overspending on diagnostic logging
- TODO:
  gate the per-component and per-`buildFormulasCyclewise()` diagnostic logging
  behind an explicit profiling/debug flag; it is currently polluting both
  runtime and JSON size on large hybrid runs.

### 2026-04-19 — timing cleanup for DDisasm symbolization host

On the same `symbolization_data_object / 60000 slots` input, rerunning with
the new defaults:
- benchmark runner default `--dump=none`
- hybrid/FC per-component logs gated behind `--fc-profile`
- `buildFormulasCyclewise()` per-invocation INFO gated behind `--fc-profile`
- per-component CUDD lifecycle prints gated behind `--fc-profile`

produces much cleaner timing numbers:
- plain:
  - old `46.49s` -> new `40.73s`
  - `FORWARD_COMPILATION_FULL 29.22s -> 26.88s`
- explicit:
  - old `43.68s` -> new `37.27s`
  - `FC_WMC_HYBRID 27.13s -> 24.40s`
- implicit:
  - old `49.44s` -> new `43.60s`
  - `FC_WMC_HYBRID 32.54s -> 30.01s`

The stage JSONs shrink correspondingly:
- explicit: `9.24 MB -> 0.003 MB`
- implicit: `9.24 MB -> 0.005 MB`

and the hybrid stage log counts collapse from roughly `200k` INFO entries to:
- explicit: `31`
- implicit: `53`

This confirms the earlier concern: the old timing runs were materially polluted
by JSON dump volume and generic per-component logging. The cleaned timings are
now a much better approximation of actual engine cost.

### 2026-04-19 — explicit rewrite profiling tightened on DDisasm symbolization

Continuing to profile the representative DDisasm-derived
`symbolization_data_object / 60000 slots` host on the cleaned timing path
showed that explicit rewrite still spent too much time before the actual
collapse work:

- pre-fix explicit (`/tmp/ddisasm_symbol_nodump/explicit_out/...124517.json`)
  - total `37.27s`
  - `FC_WMC_HYBRID = 24.40s`
  - `rewrite_ms = 17943`
  - `rewrite_collect_evidence_affected_ms = 4671.051530`
  - `rewrite_detect_total_ms = 6335.643894`

The host uses no evidence at all, so the `4.67s` evidence-affected cost was
pure overhead. `GraphRewriter::collectEvidenceAffectedNodes()` now short-circuits
when `view.getEvidenceNodes().empty()`, avoiding SCC/dependency-graph
construction in the common no-evidence case.

After that fix, explicit improved to:
- explicit (`/tmp/ddisasm_symbol_nodump2/explicit_out/...125910.json`)
  - total `41.47s` on a noisier run with slower early stages
  - `rewrite_ms = 14678`
  - `rewrite_collect_evidence_affected_ms = 0.000073`
  - `rewrite_detect_total_ms = 10171.671921`

That exposed the next dominant cost: dirty-frontier detect itself. The
frontier builder was expanding dirty seeds for two rounds. On this host, the
first `60000` `all-facts` rewrites already seed the exact exits/adjacent edges
needed to expose the second-round `linear-two-edge` regions; the second
expansion round ballooned the frontier back toward a near-full scan.

Reducing the dirty-frontier expansion from two rounds to one produced:
- explicit (`/tmp/ddisasm_symbol_nodump3/explicit_out/...13452.json`)
  - total `22.40s`
  - `FC_WMC_HYBRID = 11.15s`
  - `rewrite_ms = 5958`
  - `rewrite_collect_evidence_affected_ms = 0.000073`
  - `rewrite_detect_total_ms = 3730.811178`
- implicit (`/tmp/ddisasm_symbol_nodump3/implicit_out/...13534.json`)
  - total `30.97s`
  - `FC_WMC_HYBRID = 19.52s`
  - `rewrite_ms = 12053`
  - `rewrite_collect_evidence_affected_ms = 0.000136`
  - `rewrite_detect_total_ms = 1207.784897`

Outputs remained identical to the plain baseline:
- `symbolic_data.csv` diff: empty
- `labeled_ea.csv` diff: empty

Current reading:
- explicit rewrite is still not "cheap", but the dominant waste is now much
  better localized:
  - first it was no-evidence SCC analysis
  - then it was overly aggressive dirty-frontier expansion
- both fixes are semantics-preserving on the maintained regression suite and on
  the representative DDisasm host

One more explicit rewrite cost remained in the final no-region iteration: the
naive fan-out split pass still scanned the entire graph even though detect had
already converged. Making `splitFanoutNaive()` seed from the previous dirty
frontier instead of full-scanning every fact reduced that tail split from
roughly `700 ms` to `130 ms` on the large symbolization host and brought the
best clean timing down to:
- explicit (`/tmp/ddisasm_symbol_profile_continue/explicit_large_out/...133922.json`)
  - total `20.92s`
  - `FC_WMC_HYBRID = 9.41s`
  - `rewrite_ms = 4861`
  - `rewrite_detect_total_ms = 3119.477589`
  - `component_build_subgraphs_ms = 2527`
- plain (`/tmp/ddisasm_symbol_profile_continue/plain_large_out/...134042.json`)
  - total `38.97s`
  - `FORWARD_COMPILATION_FULL = 26.35s`
  - `reordering_runtime = 15.990`
- implicit (`/tmp/ddisasm_symbol_profile_continue/implicit_large_out/...134115.json`)
  - total `31.47s`
  - `FC_WMC_HYBRID = 19.25s`
  - `rewrite_ms = 12624`
  - `implicit_total_ms = 7718.216526`

The remaining explicit-rewrite bottlenecks are now:
1. fast-path detection still pays for large zero-hit scans
   - iter1: only `all-facts` matters, but `single+linear+parallel+fan-out`
     still cost `~1.0s`
   - iter2: only `linear-two-edge` matters, but the other detectors still cost
     `~0.6s`
   - iter3: no regions remain, but all detectors still run for `~0.16s`
2. `component_build_subgraphs_ms`, which is now dominated by dependency-graph
   construction rather than the final bucketing step
   - `--profile-dep-graph` on the same run reports a root dep-graph build of
     `3383 ms` for `380000` nodes / `295000` edges

Remaining priorities now line up as:
1. TODO: close the remaining implicit vs explicit gap
2. continue debugging explicit rewrite cost (`detect` scheduling + dep-graph/component build)
3. TODO: `CREATE_GRAPH_FULL`

Additional profiling notes:
- This `full-artifact-opt` line is effectively `online + full-only` by default.
  `MainDriver.cpp` hardwires the online translation strategy and sets
  `full-only` in config, so external runners should not expect
  `--online/--full-only` CLI flags to exist on this branch.
- `buildComponentSubgraphs()` reuses the dependency graph within a single
  `DerivationGraphViewInterface` instance via `cachedCycleDependencyGraph_`.
  The expensive `component_build_subgraphs_ms` on the representative host is
  therefore first-touch dep-graph construction, not repeated dep-graph rebuilds
  within the same view.
- Two additional low-risk explicit optimizations were tried and reverted:
  - reordering detector families to early-return on dominant batches
  - pre-count/reserve component bucket sets
  Neither produced a stable speedup on the `60000`-slot symbolization host.
  The current checked-in state remains the best known clean explicit timing.
- Timing/profiling caveat:
  `problog-benchmark/benchmarks/ddisasm_funinfer/cli/ddisasm_funinfer_full.py`
  currently appends `--derv-only` / `--derivation-only` during `run`, so its
  `run` subcommand does not measure the full exact lane on this branch. Trusted
  full timings for the representative synthetic DDisasm host therefore still
  need to be collected by:
  1. `compile --force` with the current repo-built `souffle` binary
  2. manually invoking the freshly produced `compute` binary without
     `--derv-only`
  Reusing an already-built `compute` after runtime/header changes is also
  invalid for profiling; the synthetic host must be recompiled for each kept
  runtime experiment.
- Two more dependency-graph / component-build micro-optimizations were tried
  and reverted:
  - lazily deferring edge-cycle/depth materialization in `CycleDependencyGraph`
  - reducing `computeDependencies()` hash lookups and pre-reserving some
    component-build containers
  On a properly recompiled `60000`-slot symbolization host these changes either
  regressed the explicit path badly or produced no stable gain. At this point,
  low-risk dep-graph micro-patches appear close to exhausted; further progress
  likely needs a more structural redesign rather than more container-level
  tweaks.
- TODO:
  detector scheduling still looks promising, but it likely needs to be
  iteration-aware rather than a global family reorder. The simple attempt that
  moved dominant families earlier and early-returned on large batches regressed
  the representative `60000`-slot symbolization host badly. The next try
  should gate families by expected per-iteration shape instead:
  - iter1: prioritize `all-facts` (and possibly `single-hyperedge`)
  - iter2: prioritize `linear-two-edge`
  - terminal pass: run the low-hit families once as a cleanup sweep
  This remains TODO and should be tested against side-channel and taint before
  promotion.

### 2026-04-19: one-off sanity check on existing side-channel / taint workloads
- scope:
  run a minimal baseline-vs-current sanity comparison on representative
  `full-artifact` benchmark cases using the original `full-artifact-opt` binary
  (`2374561f9`) and the current FAOPT worktree binary (`ef803682b` + local
  runtime changes). Because this branch defaults to `online + full-only`,
  lightweight wrapper scripts were used to strip the legacy `--online` and
  `--full-only` CLI flags from the benchmark runners without editing the
  runners themselves.
- side-channel:
  `P19`, full-mode, `--implicit-rewrite --det-opt`
  - baseline binary:
    `compile = 35.12s`, `run = 3.553s`, `keys = 7255`
  - current binary:
    `compile = 38.20s`, `run = 3.356s`, `keys = 7255`
  - `facts.prob` matched exactly between the two runs
- taint:
  `and-roc`, `pt-obj-dlog`, full pipeline, `--implicit-rewrite` (runner
  auto-added `--det-opt`)
  - baseline binary:
    `run = 165.03s`, `keys = 11385`
  - current binary:
    `run = 132.53s`, `keys = 11385`
  - `facts.prob` matched exactly for the checked `pt-obj-dlog` stage
- interpretation:
  this is not a controlled benchmark campaign, just a one-off sanity read, but
  it is enough to say that the currently kept rewrite-side optimizations did
  not obviously regress the representative checked side-channel and taint
  workloads. The detector-scheduling idea still remains TODO rather than kept
  behavior because it has only been tried in the DDisasm-derived host and
  regressed there.

### 2026-04-19: current plain/explicit/implicit trend sanity on representative side/taint workloads
- scope:
  check whether the *current* FAOPT binary still shows the expected
  `plain / explicit / implicit` speedup trend on one representative
  side-channel workload and one representative taint workload. Because another
  heavy session was active on the same machine, treat the absolute wall times
  as sanity readings only; the trusted comparison is the per-run debugger JSON
  produced by the same binary on the same input.
- side-channel:
  whole-case `P17`, current FAOPT runtime, direct `compute` invocation on the
  same freshly compiled binary and the same `input/`
  - plain:
    `turn = 4.709s`, `CREATE_GRAPH_FULL = 0.381s`,
    `FORWARD_COMPILATION_FULL = 3.574s`,
    `WEIGHTED_MODEL_COUNTING_FULL = 0.047s`
  - explicit (`--rewrite`):
    `turn = 2.043s`, `FC_WMC_HYBRID = 0.977s`, `rewrite_ms = 554`
  - implicit (`--implicit-rewrite`):
    `turn = 1.691s`, `FC_WMC_HYBRID = 0.662s`, `rewrite_ms = 469`,
    `implicit_total_ms = 373.797575`
  - output agreement:
    `facts.prob`, `KEY_IND.csv`, `KEY_SENSITIVE.csv`, and the full non-log
    output file set matched exactly across all three runs
- taint:
  representative heavy stage `yaaic / pt-obj-dlog`, direct `compute`
  invocation on the same compiled stage binary and the same materialized
  `pt-obj-dlog/input`
  - rationale:
    `yaaic` is a medium-sized real case in the prepared taint bundle and
    `pt-obj-dlog` is its dominant exact stage (`15.454s` in the bundle's
    stage report), so this is the cleanest representative place to compare
    rewrite behavior without taint-runner orchestration noise
  - plain:
    `turn = 13.211s`, `CREATE_GRAPH_FULL = 1.066s`,
    `FORWARD_COMPILATION_FULL = 11.487s`,
    `WEIGHTED_MODEL_COUNTING_FULL = 0.018s`
  - explicit (`--rewrite`):
    `turn = 3.824s`, `FC_WMC_HYBRID = 1.589s`, `rewrite_ms = 173`
  - implicit (`--implicit-rewrite`):
    `turn = 3.834s`, `FC_WMC_HYBRID = 1.367s`, `rewrite_ms = 216`,
    `implicit_total_ms = 148.629320`
  - output agreement:
    the full non-log output file set under the stage output directory matched
    exactly across plain/explicit/implicit, including `facts.prob`
- interpretation:
  the representative checked side-channel and taint workloads still preserve
  the qualitative trend we want:
  - `plain` is the slowest path
  - rewrite is clearly beneficial
  - explicit and implicit stay in the same performance band
  The exact ordering is not identical across domains: on checked side-channel
  `P17`, implicit was slightly faster than explicit; on checked taint
  `yaaic / pt-obj-dlog`, explicit and implicit were effectively tied. This is
  still consistent with the claim that the current runtime changes did not
  break the established rewrite speedup trend on the existing benchmark
  families.

## 2026-04-20 — SUM aggregate replay prototype and symbolization sanity

- scope:
  current-source `full-artifact-opt-string-port-wt`, after restoring implicit
  full-mode to a single maintained `overlay all-facts = on` path
- implementation:
  added a minimal `SUM` aggregate replay path without changing `RuleApplication`
  shape
  - `Rule` now carries lightweight `AggregateSpec` metadata
  - `Synthesiser` emits `AggregateSpec` for single-atom `SUM` clauses
  - `DerivationGraph` reconstructs aggregate witnesses from replay-visible
    tuples and builds a deterministic DP subgraph of synthetic
    `__agg_sum_state(...)` nodes
- correctness checks:
  - new maintained regression case
    `problog_sum_exact_roundtrip`
  - tiny probe:
    `base("a"), w("a",2)=0.5, w("a",3)=0.6, total(X,S) :- S = sum ...`
    now yields `total("a",5) : 0.3`
  - the pre-existing `problog_symbol_aggregate_roundtrip` regression still
    passes unchanged
- key bug fixes discovered while implementing:
  - aggregate tuple indexing was initially using iterators from two different
    temporary `rule->getVars()` vectors; fixing that removed a create-graph
    crash
  - synthetic hyperedges created with `rule=nullptr` were leaving edge
    probability uninitialized; these edges are now deterministic (`1.0`)
- symbolization status:
  - after rewriting the extracted host's negated existential into a helper
    relation, probabilistic symbolization cases can now reach the aggregate
    replay path
  - medium cases such as `gawk_balanced` and `grep_balanced` no longer show the
    old `data_object_total_points` probability collapse to a single
    relation-level constant; symbol candidates now exhibit combined values such
    as `0.696256 = 0.86 * 0.88 * 0.92`
  - larger cases such as `flex_balanced` still blow up in BDD construction
    (`makeOrBalanced failed` after ~11M CUDD nodes), so the current DP
    encoding is semantically better but not yet cost-robust for large
    symbolization workloads
- interpretation:
  this is enough to say the current runtime can replay `SUM` witnesses
  numerically on small/medium cases, but the aggregate support is still
  prototype-level for benchmark-scale symbolization inputs because the
  synthetic DP subgraph can expand too aggressively.

## 2026-04-20 — Symbolization: eager dep-graph depths were the main extra FC tax

- scope:
  unified checkpoint `full-artifact-opt-unified` on the extracted DDisasm
  symbolization host (`symbolization_data_object_extracted.dl`) with real
  balanced binary inputs
- finding:
  the expensive `component_build_subgraphs_ms` in rewrite mode was not just
  component bucketing. On the hybrid path it eagerly built a full
  `CycleDependencyGraph`, including cycle depths, before component analysis.
  The full-view depth map is only needed later by cyclewise formula
  construction on the residual slow components.
- implementation:
  `CycleDependencyGraph::computeDepths()` is now deferred until
  `buildFormulasCyclewise*()` actually needs `edgeDepthsGlobal`.
- evidence:
  on `gawk_balanced`, a dep-graph-profiled explicit run now reports
  `depth=0 depth_deferred=1 total=587.8 ms` for the full-view dep graph,
  versus the previous eager profile of roughly `1145 ms` total with about
  `360 ms` spent only on depth computation.
- resulting runtime changes:
  - `gawk_balanced`
    - plain: `6.57 s`
    - explicit: `4.88 s`
    - implicit: `6.38 s`
    - `manager_init_vars`: plain `25054`, rewrite `122`
  - `bison_balanced`
    - plain: `5.80 s`
    - explicit: `4.38 s`
    - implicit: `5.61 s`
    - `manager_init_vars`: plain `25124`, rewrite `44`
  - `tmux`
    - plain still crashes in CUDD `makeOrBalanced`
    - explicit: `21.61 s`
    - implicit: `28.33 s`
    - `manager_init_vars`: rewrite `492`
- stage-level interpretation:
  - rewrite still removes almost all hard backend variables on these cases.
  - the previous slowdown on medium symbolization cases was largely inflated by
    eager full-view dep-graph work inside `component_build_subgraphs_ms`.
  - after deferring depth construction, both explicit and implicit regain
    end-to-end viability on the representative medium cases; explicit is now
    clearly faster than plain on `gawk` and `bison`, while implicit is roughly
    tied with or slightly faster than plain.
  - large cases still justify rewrite even more strongly because plain remains
    non-robust (`tmux` crashes, rewrite completes).

## 2026-04-20 — Symbolization explicit rewrite: front-end rewrite overhead

- scope:
  explicit rewrite only, same extracted symbolization host and balanced inputs
  as above, after the lazy dep-graph-depth checkpoint
- implemented low-risk rewrite-front-end optimizations:
  - hot detector/rewrite loops now use `Hyperedge` input/negation references
    directly instead of repeatedly copying vectors through the view API
  - compaction snapshots live edges/nodes into vectors instead of copying the
    whole unordered-set container before mutating the view
  - compaction no longer rebuilds probabilistic semantic fact-use stats, since
    it only absorbs deterministic facts and therefore never consults those
    occurrence counts
  - detector profiling now reports `semantic-stats=... ms` inside the
    fast-path breakdown so this cost is visible separately
- correctness:
  `problog_sum_exact_roundtrip`, `problog_string_roundtrip`, and
  `problog_symbol_aggregate_roundtrip` passed with the repo-built binary.
  For `gawk_balanced`, `bison_balanced`, and `tmux`, the checked output CSVs
  and `facts.prob` matched the previous lazy-depth explicit outputs exactly.
- measurements:
  - previous lazy-depth explicit rewrite totals:
    `gawk=1830 ms`, `bison=1628 ms`, `tmux=8158 ms`
  - best post-copy explicit rewrite totals observed:
    `gawk=1470 ms`, `bison=1550 ms`, `tmux=6336 ms`
  - final no-op split is pure overhead on the checked symbolization cases:
    `--split-mode=no-split` produced identical checked outputs and the same
    random-variable removal (`gawk=9968`, `bison=10824`, `tmux=39156`)
- rejected/neutral attempt:
  dirty frontier narrowing by marking only rewritten-region boundaries was
  not kept. It reduced raw dirty seed counts, but the one-hop frontier expanded
  to the same large node/edge set on `tmux` and did not produce stable rewrite
  time improvement.
- remaining bottleneck:
  rewrite cost is now dominated by repeated detector scans and the final
  no-region proof. On `tmux`, the third explicit detect still scans a dirty
  frontier of roughly `203k` nodes and `159k` edges just to produce zero
  candidates, followed by a no-op naive split. The next optimization should
  separate dirty reasons or detector phases so linear/parallel follow-up
  scans do not force all fact-oriented detectors over the same large frontier.

## 2026-04-20 — Symbolization rewrite benefit is deterministic-component compaction

- active host/provenance:
  the current symbolization experiments use the extracted host
  `problog-benchmark/benchmarks/ddisasm_symbolization_extracted/dataset/programs/symbolization_data_object_extracted.dl`
  and the preserved `*_balanced` input directories under
  `/tmp/ddisasm_symbolization_inputs_syspaper_20260419_1645`. This extracted
  host is not yet tracked in the packaged `CAV-FULL` benchmark tree and should
  be migrated there before treating it as artifact-stable.
- current output relations in that host:
  only `data_object`, `symbolic_data`, and `labeled_ea` are now emitted. The
  earlier debug/intermediate outputs `data_object_candidate`,
  `data_object_total_points`, and `discarded_data_object` were removed from the
  benchmark-facing `.output` set to avoid inflating the queried output nodes.
- tuple interpretation:
  `symbolic_data(EA, Size, Val)` means that the bytes at address `EA` form a
  selected data object of byte length `Size` and contain a refined symbolic
  pointer/reference value `Val`. Example:
  `symbolic_data(183624,8,17056)` is backed by
  `address_in_data_refined(183624,17056)`, selected as an 8-byte `"symbol"`
  data object.
- key ablation on `gawk_balanced`:
  - full explicit rewrite: `manager_init_vars=122`, max slow component estimate
    `61`
  - skip literal rewrite but keep hybrid backend: `manager_init_vars=8106`, max
    slow component estimate `4053`
  - disable only deterministic edge compaction:
    `manager_init_vars=8094`, max slow component estimate `4047`
  - disable all-facts but keep compaction:
    `manager_init_vars=122`, max slow component estimate `61`
- interpretation:
  symbolization's current rewrite advantage is not primarily SISO probabilistic
  precomputation. The dominant effect is deterministic fact-input compaction:
  deterministic leaf facts are projected out of hyperedge inputs, which removes
  correlation-free graph connectors and lets the component-wise backend solve
  many small components. This preserves checked `facts.prob` outputs on the
  tested `grep/gawk/bison` cases, but it should be presented as
  component-aware deterministic normalization/splitting rather than as evidence
  that SISO rewrite itself is useful for symbolization.
- next packaging direction:
  separate a cheap deterministic-input compaction or support-aware component
  construction pass from the expensive SISO rewrite loop. For symbolization, the
  intended story should be: the system outperforms ProbLog by exploiting
  deterministic/correlation-free structure and component-specialized
  evaluation; raw SISO rewrite is secondary or sometimes pure overhead.

## 2026-04-21 — Symbolization checkpoint: no-split rewrite and component-fast optimization

- active experiment workspace:
  `/tmp/symbolization_10case_trim_random_20260421`
- active extracted host:
  `problog-benchmark/benchmarks/ddisasm_symbolization_extracted/dataset/programs/symbolization_data_object_extracted.dl`
- current fixed case pool:
  - syspaper balanced: `bison`, `flex`, `gawk`, `grep`, `m4`, `make`,
    `patch`, `rsync`, `tar`, `wget`
  - medium extras: `bugpoint`, `cluster`, `dirmngr`, `git-http-backend`,
    `git-remote-http`, `gpgcompose`, `gpgsm`, `gvmap`, `ip`,
    `llvm-config`, `readelf`, `screen`, `sshd`, `tc`, `tmux`, `troff`
  - larger extras: `gcc11`, `gpp11`, `llvm-objcopy`, `llvm-objdump`,
    `llvm-pdbutil`
- selected 15-case symbolization benchmark:
  - retain cases whose completed plain runtime is at least `5s`, plus cases
    where plain aborts but the optimized path completes
  - add back `gpgsm` as the fastest dropped case by speedup (`3.965s` plain,
    `2.391s` best, `1.66x`) to make a 15-case set
  - selected cases: `bison`, `cluster`, `flex`, `gawk`, `gcc11`, `gpgsm`,
    `gpp11`, `gvmap`, `llvm-config`, `llvm-pdbutil`, `readelf`, `tc`, `tmux`,
    `troff`, `wget`
- fixed benchmark-facing output relations remain only:
  `data_object`, `symbolic_data`, and `labeled_ea`.
- current fastest tested symbolization path:
  bare `--det-opt --rewrite`; the smart dispatcher selects the no-split legacy
  path automatically because the symbolization rules are deterministic
  - this is not pure SISO probabilistic precomputation
  - global random variable count usually does not decrease on these cases
  - nevertheless, no-split SISO structural rewrites compress the graph and
    reduce downstream component construction/classification cost
- compaction-only ablation:
  - a temporary `--rewrite-compaction-only` option was used locally to enter
    the rewrite stage, bypass SISO detection/rewrite, run deterministic edge
    compaction plus isolated-node cleanup, and then continue to the
    component-wise backend
  - `readelf`: compaction-only `10.39s`, no-split rewrite `9.37s`
  - `llvm-config`: compaction-only `5.11s`, no-split rewrite `3.01s`
  - conclusion:
    compaction is necessary but not sufficient. The no-split SISO structural
    rewrites are still useful because they shrink the component graph even
    when they do not remove global random variables.
  - disposition:
    the standalone compaction-only CLI path is not artifact-facing and has
    been removed after this ablation.
- component classification reading:
  - the existing `component classification` timer is not pure classification;
    it includes fast component evaluation.
  - `readelf` has `18036` fast-single components and `1366` slow components in
    the checked no-split run.
  - `llvm-config` has `1234` fast-single components and `677` slow components.
- implemented candidate component-side optimization:
  - add a borrowed component view so fast component evaluators do not copy
    `ComponentSubgraph` node/edge sets into temporary `SubgraphView`s
  - evaluate a single-random-variable component for both `false` and `true`
    assignments in one pass instead of running the boolean evaluator twice
  - stage-level effect:
    - `readelf` component classification dropped from about `1013ms` to
      about `632ms`
    - `llvm-config` component classification dropped from about `72ms` to
      about `42ms`
  - end-to-end wall-clock remains noisy and is still affected by graph
    construction, SISO detection, component subgraph build, and occasional
    slow-component BDD formula build costs; use stage timings rather than one
    wall-clock run as the immediate keep/drop signal.
- correctness status:
  - checked output CSVs matched on `readelf` and `llvm-config`.
  - `llvm-config facts.prob` matched exactly in the checked pair.
  - `readelf facts.prob` had same key set with only last-digit differences
    around `1e-8` in the checked pair.
  - do not assume those `1e-8` differences are harmless floating-point noise
    yet. They may reflect output-order, evaluation-order, or fast-path
    arithmetic differences and need a focused audit before claiming exact
    equality.
- next optimization targets:
  - keep artifact-facing `--rewrite` as the only required optimized command in
    benchmark instructions; retain `--explicit-rewrite`, `--implicit-rewrite`,
    and `--split-mode=*` only as diagnostic controls
  - remove or avoid the final no-op SISO detection pass on symbolization-style
    runs
  - keep the useful no-split structural rewrites, but make their detector
    schedule cheaper than the generic full SISO loop
  - reduce `component_build_subgraphs_ms`, which is often larger than
    classification after the fast-single optimization
  - split classification timing into pure classification and fast-evaluation
    timing so future regressions are easier to diagnose
  - keep the `facts.prob` comparison tolerance at `1e-8` unless the output
    writer is changed to avoid rounding-boundary bitwise drift
  - final artifact cleanup pass:
    first write a focused report that separates artifact-evaluation pipeline
    code from local probes, temporary flags, rejected experiments, debug-only
    instrumentation, and stale benchmark helpers. Use that report to decide
    what should be removed, archived, hidden behind diagnostics, or kept before
    cutting the final artifact-evaluation version.

### 2026-04-21 — Side-channel and taint smoke after rewrite-overhead cleanup

- compiler checkpoint:
  `artifact-rewrite-followups-20260421` at
  `beb581c24 perf(problog): reduce symbolization rewrite overhead`
- side-channel smoke:
  `CAV-FULL` standard `RQ2 P19`, one run, full rules, current repo-built
  `souffle`, generated under `/tmp/cavfull_side_p19_beb581c24`
  - plain `bdd`: elapsed `15.111s`, `Avg_Overall_s=12.349266197`,
    `Avg_LiveNodes=395972`
  - default optimized `bdd_r` (`--det-opt --rewrite --knowledge bdd`):
    elapsed `3.506s`, `Avg_Overall_s=1.104612295`, `Avg_LiveNodes=0`
  - output agreement:
    `bdd_r` matched `bdd` exactly (`mismatches=0`, `max|delta|=0`)
  - direct diagnostic `--explicit-rewrite` on the same compiled `compute_rq2`
    also matched `bdd` exactly; its hybrid stage was `1.132599484s`, close to
    the default optimized path
- taint smoke:
  `CAV-FULL` appendix bundle case `app-018`, one full-pipeline run, current
  repo-built `souffle`, generated under
  `/tmp/cavfull_taint_app018_all_beb581c24`
  - `no_rewrite`: `81.97s`
  - `explicit_rewrite`: `102.13s`
  - default optimized `implicit_rewrite` (`--rewrite`): `13.16s`
  - output agreement:
    explicit and implicit both matched `no_rewrite` on all five checked stages
    (`max|delta|=0`)
  - default optimized speedup:
    `no_rewrite / implicit_rewrite = 6.23x`
- important diagnostic note:
  forced `--explicit-rewrite` is still not a safe artifact default for taint.
  On `app-018 / typefilter-dlog`, explicit spent
  `explicit_graph_rewrite_ms=87014` even though that stage had `rand_vars=0`;
  default `--rewrite` selected `auto-implicit` and finished the same stage in
  `0.423s`. This reinforces the current artifact policy: expose `--rewrite`
  as the optimized path and keep explicit/implicit/split flags as diagnostics.
- interpretation:
  the rewrite-overhead cleanup did not regress the checked representative
  side-channel or taint default `--rewrite` behavior. Side-channel keeps a
  strong optimized-path win, and taint retains the expected large
  `no_rewrite / implicit_rewrite` speedup on a representative high-speedup
  case.

## Related commits
- `UNCOMMITTED` — feat(problog): add minimal SUM aggregate replay regression and graph reconstruction
- `UNCOMMITTED` — perf(problog): remove compaction-only probe and keep fast-single component evaluator optimization
- `UNCOMMITTED` — fix(implicit-rewrite): force materialized handoff when overlay split creates aliases
- `UNCOMMITTED` — perf(rewrite): trim explicit rewrite front-end scans for symbolization
- `UNCOMMITTED` — perf(fc): defer dep-graph depth construction until cyclewise build
- `UNCOMMITTED` — docs(research): record DDisasm pass exploration and symbolization host reading
- `UNCOMMITTED` — docs(research): log rejected and candidate implicit overlay optimization attempts
- `UNCOMMITTED` — docs(research): refresh curated rewrite status around the packaged full artifact
- `00565d0` — feat(artifact): package maintained CAV full workflows
- `7d9d45b72` — feat(problog): add implicit split rewrite pipeline
