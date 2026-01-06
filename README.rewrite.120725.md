# Rewrite Notes (Historical, 2025-12-07)

## Status
- Historical record from 2025-12-07; not the current implementation.
- Use `README.rewrite.md` for current behavior and `README.eval.md` for runs.

## Scope
- Historical record (2025-12-07); not an authoritative description of the current implementation.
- Applies only to full-mode rewrite; incremental modes do not perform rewrite.

## Background / Usage
- Core code: `src/include/souffle/problog/GraphAnalyzer.h` (SISO detection), `GraphRewriter.h` (rewrite and post-processing), `Pipeline.h` (pipeline timing), `Plan.md` tracks TODOs.
- Build: `cmake --build cmake-build-release --target souffle -j4`
- Example run (with profiling): `./compute_new -p run_profile.log -F ./input -D ./output`; enable rewrite with `-r`; enable fast-path debug output with `SOUFFLE_SISO_FAST_DEBUG=1`.

## Current implementation status (2025-12-07)
### Fast-path SISO detection
- Supported RegionKind: `SingleHyperedge` (non-fact SI + >=1 fact inputs, single-edge fact output), `AllFactsToSO` (all inputs are facts and only connect to this edge), `LinearTwoEdge` (entry->mid->exit, mid must not be query/evidence), `ParallelEdge` (>=2 single-input parallel edges, placeholder not rewritten), `General` disabled.
- Filters: fact inputs must be non-evidence/needOutput; all-facts inputs must have only this outgoing edge; single-hyperedge skips single-input "naive" edge; linear mid must have a unique outgoing edge and be non-query/evidence.
- Overlap: still applies non-overlap filtering.

### Rewrite logic
- All-facts: fold into a fact output, multiply probability by all fact inputs; delete isolated fact inputs and the original edge.
- Single-hyperedge: create a new SI->SO edge, multiply probability by fact inputs, delete the old edge and isolated fact inputs.
- Linear two-edge: multiply the two edge probabilities to create a new entry->exit edge, delete the mid node and the two old edges.
- Parallel two-edge: still placeholder (skipped).
- At the end of each round, run "edge compaction": absorb non-evidence/query fact inputs into all edges, update probability, delete isolated facts.
- BDD manager is lazily initialized; dot dumps and detection timing are preserved.

### Output/logs
- `[pipeline]` rows list timings for create/pruning/rewrite/build/WMC, etc.; rewrite rows include iterations, region counts, RV changes, node/edge adds/removes.
- `Current live nodes` is the BDD node count for final forward compilation.
- `SOUFFLE_SISO_FAST_DEBUG=1` prints fast-path filter reasons.
- Per-iteration timing (GraphRewriter): `total`, `countBefore`, `detect`, `loop` (with `loopCond/loopRewritten/loopSkip`), `edgeList`, `compact`, `countAfter`, `dumpRegions` (dumpAllRegionsAsDot in debug), `dumpDot(before/after)`, `preLog/log`, `other` is the remaining miscellaneous cost. dumpAllRegions/dumpDot may dominate in debug (e.g., P13 first round dumpRegions~=18ms, dumpDot(before)~=7ms, after~=2ms).
- Fast-path debug text output is commented out to reduce logging overhead; facts.prob rewrite/no-rewrite consistency has been confirmed.

## Recent benchmarks (facts.prob are consistent)
- P13: no-rw ~=4550 ms, BDD 97k; rw ~=2446 ms, BDD 10.9k.
- P14: no-rw ~=10285 ms, BDD 199k; rw ~=4916 ms, BDD 17.5k.
- P15: no-rw ~=44767 ms, BDD 820k; rw ~=27037 ms, BDD 45k.
- P16: no-rw ~=141729 ms, BDD 2.08M; rw ~=74557 ms, BDD 97k.
- P17 no-rewrite timed out at 300s (not completed); rewrite not run. P18/P19 not run.

## Possible next steps
- Finish ParallelTwoEdge rewrite (or disable it).
- Resolve P17/P18/P19 timeouts/large outputs: run only rewrite or increase the timeout.
- Re-check edge compaction benefits/correctness on large graphs; consider skipping query/evidence-related edges.
- Clean untracked experiment artifacts (many dot/log files).

## Benchmark graph structure summary (side_channel_full)
- Dominant SISO shapes: `AllFactsToSO` and `SingleHyperedge` are most frequent (many fact prefixes into one hyperedge), followed by a small number of `LinearTwoEdge`; `ParallelEdge` has almost no matches.
- Common pattern: many input facts aggregate into one intermediate node, then point to a RAND/KEY node; after rewriting, the intermediate node can become a fact, triggering another all-facts fold.
- Rare/missing: true multi-input parallel single edges (Parallel) are almost absent; general/complex SISO is disabled.
- Performance-impacting stage: rewrite overhead is almost entirely fast-path SISO detection (slowest in the first round, then decreasing); with `-p`, if debug is enabled, `dumpAllRegionsAsDot`/`dumpDot` I/O can be significant (10-20 ms per call, observed on P13).
- BDD build: after rewrite, BDD node count drops sharply (P14-P17: rewrite BDD is 10-50x smaller than no-rewrite), build time is proportional to node count.

## Latest end-to-end comparison (P14-P17, using latest compute_new)
Rough total time is the sum of create+prune+rewrite+BDD build+per-node+prob dump (from `run_rewrite_prof_new.stdout` and `run_no_rewrite_prof_new.stdout`):
- P14: rw ~= 1.4s vs no-rw ~= 13.0s (~9.3x faster)
- P15: rw ~= 9.5s vs no-rw ~= 44.7s (~4.7x)
- P16: rw ~= 16.5s vs no-rw ~= 138.3s (~8.4x)
- P17: rw ~= 31.9s vs no-rw ~= 307.2s (~9.6x)
Breakdown: rewrite-side BDD build/per-node time drops sharply; rewrite itself is down to 0.2-1.9s, detection is in the ms to hundreds of ms range.

### Workload differences by stage (rw vs no-rw)
- RandomVars: after rw, about 24% of no-rw (randomVarsRatio ~=0.24 for P14-P17, removing ~3/4 variables).
- BDD nodes (corresponding to BDD build / per-node time): rw node count and time are about 1/6 to 1/50 of no-rw (within the same case, BDD build and per-node both decrease linearly with node count).
- Rewrite iterations and region counts: P14-P17 have 11-12 rewrite iterations, 13k-63k regions, `nodesRemoved/edgesRemoved` roughly on the same order as `randomVarsRemoved`, strongly shrinking final size.
- No-rewrite case: legacy SISO detection is extremely expensive (first round in old logs can be tens to hundreds of seconds); in rw mode fast-path detection is down to milliseconds.

### Rewrite sub-stage profiling characteristics
- Per-iteration timing output (GraphRewriter): `total`, `countBefore`, `detect` (with fast-path category breakdown), `loop` (cond/rewritten/skip), `edgeList`, `compact` (edge compaction), `countAfter`, `dumpRegions`, `dumpDot(before/after)`, `preLog/log`, `remainder`.
- Quantified time (P14-P17, `-p`, no debug): single-round `total` ~=0.1-0.2s (P14) up to ~=1.9s (P17); `detect` is 5-90 ms in the first round, then 1-20 ms; `compact`/`edgeList`/`count*` often 1-10 ms; `dumpRegions`/`dumpDot` are ~0 ms without debug.
- I/O when debug enabled (`-p` default enabled, dot output controlled by env): `dumpAllRegionsAsDot`/`dumpDot` can take 10-20 ms each (observed P13), reflected in per-iter `dumpRegions/dumpDot`.
- BDD manager is lazily initialized, only created when BDD is needed; end-of-round edge compaction absorbs fact inputs and deletes isolated facts.
- Fast-path detection breakdown: `singleHyperedgeMs/Count`, `allFactsToSOMs/Count`, `linearTwoEdgeMs/Count`, `parallelEdgeMs/Count`; debug filter reasons available via `SOUFFLE_SISO_FAST_DEBUG=1`.

### SISO distribution in recent cases (cumulative, rw mode)
- P14: single~=554, linear~=6.3k, parallel=0, all-facts~=3.9k; first round is dominated by all-facts, later rounds shift to linear.
- P15: single~=1.1k, linear~=13.4k, parallel=0, all-facts~=7.4k.
- P16: single~=1.8k, linear~=22.6k, parallel=0, all-facts~=13.4k.
- P17: single~=2.4k, linear~=30.7k, parallel=0, all-facts~=18.1k.
- Pattern: linear and all-facts dominate; parallel never hits; single is smaller but notable.

## Recent experiment commands (P14-P17)
- Build executable (with profile and full-only):  
  `souffle_bin=/home/hugh/research/datalog/souffle/cmake-build-release/src/souffle`  
  `"$souffle_bin" --full-only --profile=/dev/null --online -F ./input -D ./output compute.souffle.dl -o compute_new > rebuild.log 2>&1`
- Run rewrite (with iteration/detection logs, output to `run_rewrite_prof_new.*`):  
  `./compute_new -r -p run_rewrite_prof_new.log -F ./input -D ./output_rewrite_prof > run_rewrite_prof_new.stdout 2>&1`
- Run no-rewrite baseline (output to `run_no_rewrite_prof_new.*`, run `mkdir -p output_no_rewrite_prof` first):  
  `./compute_new -p run_no_rewrite_prof_new.log -F ./input -D ./output_no_rewrite_prof > run_no_rewrite_prof_new.stdout 2>&1`
- Applicable directories: `experiments/side_channel_full/P14`-`P17` (run the commands in each directory).
