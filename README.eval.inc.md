# Incremental Side-Channel Benchmark Quick Guide

Incremental side-channel benchmark notes (context + procedures). Experiments live under `experiments/side_channel_inc_eval/` and use the release Soufflé binary. **Do not git-add anything under `experiments/` or other generated artifacts.**

## Prerequisites
- Build release Soufflé: `cmake --build cmake-build-release --target souffle -j4`
- Put the release binary on PATH before running the Python scripts:
  ```bash
  export PATH="/home/hugh/research/datalog/souffle/cmake-build-release/src:$PATH"
  ```

## End-to-end for P1 (from repo root)
```bash
# 1) Generate inputs from SMT
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval generate --cases 1 --cleanup

# 2) Generate delta workloads (default change spec; overwrites delta/)
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval delta --cases 1 --cleanup

# 3) Compile Soufflé with online CLI (produces ./compute in P1/)
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval compile --cases 1 --timeout 300

# 4) Run baseline (full+inc) and one sample for inc10 (or other labels)
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval run \
  --cases 1 --delta-labels inc10 --delta-samples 1 --timeout 300

# 5) (Optional) Collect summary TSV
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval collect --cases 1
```

## What to look at
- Outputs: `output/facts.full.prob`, `output/facts.inc.prob`, per-delta `delta-*-fact-iter*-{inc,full}.prob`.
- Logs: `log_P1_*.json` (per-stage timings), stdout from manual runs (e.g., `log_inc_applyDelta_inc*.stdout`).
- CLI runs in inc mode print delta size: `[applyDelta] delTuples=… delRuleApps=… delFacts=… insTuples=… insRuleApps=… insFacts=…`.
- Prune now rebuilds impacted maps on the pruned subgraph; applyDelta no longer does impacted BFS.

## Latest status / known issues
- P1 (and P3) incremental insertion can loop in `ForwardCompilation` on the KEY_SENSITIVE eq-cycle; worklist never drains. Investigations suggest formulas keep flipping among a few BDDs despite stable var indices. Avoid these cases for now.
- Incremental pruning still dominates time for larger cases (P12/P13), e.g. P13 inc10: prune≈2.6s, forward≈1.07s, total≈3.71s; full run forward+wmc is heavy but still faster overall (≈3.40s). P4–P11 are small and fast.
- All P4–P13 inc10 runs (delta sample 1) currently match full results (`max|Δ|=0`) as recorded in `output/delta-inc10-1.json` per case.
- Generated logs: per-case `log_P*_inc10_1_inc_*.json` and `log_P*_inc10_1_full_*.json` hold stage breakdowns; `output/delta-inc10-1.json` holds consistency checks.

## Quick performance snapshot (inc10 sample=1, wall clock from runner)
- P4–P9: inc ≈0.10–0.20s, full ≈0.09–0.17s (roughly parity; prune dominates inc).
- P10–P11: inc ≈0.68/0.66s vs full ≈0.94/0.63s.
- P12: inc ≈2.80s vs full ≈2.18s (prune+fwd heavy).
- P13: inc ≈8.27s vs full ≈5.92s (prune+fwd heavy).

## Notes
- Do not commit anything under `experiments/`; keep it local.
- If you edit delta files manually (`delta/inc10_*.txt`), rerun compile+run so probabilities match.

## How I run the inc experiments (exact commands)
From repo root, with release Soufflé on PATH:
```bash
export PATH="/home/hugh/research/datalog/souffle/cmake-build-release/src:$PATH"

# Generate cases P4–P13 (skip P1/P3 due to current inc loop issue)
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval generate --cases 4-13 --cleanup

# Generate deltas (overwrite delta/)
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval delta --cases 4-13 --cleanup

# Compile compute for each case using online CLI support
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval compile --cases 4-13 --timeout 300

# Run baseline (full+inc) and one sample of inc10 per case, timeout 30s
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_eval run \
  --cases 4-13 --delta-labels inc10 --delta-samples 1 --timeout 30
```
Artifacts per case:
- Stage logs: `log_P*_inc10_1_inc_*.json`, `log_P*_inc10_1_full_*.json`.
- Consistency + paths: `output/delta-inc10-1.json` (contains base/iter comparisons, `max|Δ|`, etc).
- Prob outputs: `output/delta-inc10-1-{inc,full}-fact-iter*.prob`, `output/facts.{inc,full}.prob`.
