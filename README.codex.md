# Codex Quickstart

Use this file to orient Codex sessions. It lists the essential docs, code hotspots, and how to run experiments.

## Key Docs
- `README.rewrite.md` — current rewrite pipeline design, status, perf notes.
- `README.rewrite.2.md` — earlier/alternative optimization plan.
- `README.rewrite.impl.md` — experiment logs and insights collected so far.
- `README.siso.md` — SISO definition/assumptions.
- `README.eval.md` — how to run experiments and compare outputs/timings.

## Core Code
- `src/include/souffle/problog/GraphRewriter.h` — rewrite logic (SISO detection loop, local DD/WMC, edge summarization).
- `src/include/souffle/problog/GraphAnalyzer.h` — SISO detection, heuristics/filters.
- `src/include/souffle/problog/Pipeline.h` + `ForwardCompilation.h` — orchestration and BDD construction.

## Experiment How-To (side_channel_full)
- Per benchmark dir (e.g., `experiments/side_channel_full/P12`):
  - No rewrite: `./compute_new -F ./input -D ./output_no_rewrite_xx > run_no_rewrite_xx.log`
  - With rewrite: `./compute_new -r -F ./input -D ./output_rewrite_xx > run_rewrite_xx.log`
  - Outputs should match: `diff output_no_rewrite_xx/facts.prob output_rewrite_xx/facts.prob`
  - Logs contain `[pipeline]` timings (create graph, pruning, SISO detection/rewrite, BDD init/build, WMC).

## Current State (post revert summary)
- Reverted aggressive fact-prefix/folding; simple SISO only updates edge probability, not folding nodes.
- Region filter: edgeCount ≥1 or nodeCount >2, with `kMaxEdges` default 5; RV count not enforced.
- P5: rewrite ≈ no-rewrite (~0.33–0.34s, BDD ~210 nodes).
- P12: rewrite slower (~6.5s vs ~3.3s) mainly due to SISO rewrite (~3.3s); outputs match.

## Investigation Tips
- Profile per-region cost (detect/build/WMC/apply, live nodes) if slowing down.
- Consider heuristic skips for low-payoff regions and analytic shortcuts for very simple SISOs (facts/linear chains).
- Keep changes small; test on both small (P5) and larger (P12/P1x) cases and report timings plus BDD sizes.***
