# Rewrite Notes (Historical, 2025-12-08)

## Source references
- [src/include/souffle/problog/GraphRewriter.h](src/include/souffle/problog/GraphRewriter.h)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)


## Status
- Historical record from 2025-12-08; not the current implementation.
- Use `README.rewrite.impl.md` for current behavior and `README.eval.md` for runs.

## Scope
- Historical record (2025-12-08); not an authoritative description of the current implementation.
- Applies only to full-mode rewrite; incremental modes do not perform rewrite.

## Background and goals (2025-12-08)
- Core components: `src/include/souffle/problog/GraphAnalyzer.h` (SISO detection), `GraphRewriter.h` (rewrite and edge compaction), `Pipeline.h` (pipeline timing), `Plan.md` (task log).
- Goal: reduce random variables and BDD build cost via SISO/local rewrites and fact-prefix/edge compaction, and use cheaper fast paths instead of BDDs on simple structures.
- Current fast path assumes no negation (unless explicitly handled): by default only absorbs positive literals, and `bodyNegations` is only used in some paths (fact absorption, single-edge absorption, linear two-edge entry polarity, fan-out converge, etc.).

## Run command template
- Build Souffle (release): `cmake --build cmake-build-release --target souffle -j4`
- Build each benchmark (example: P13):  
  `souffle_bin=/home/hugh/research/datalog/souffle/cmake-build-release/src/souffle`  
  `"$souffle_bin" --full-only --profile=/dev/null --online -F ./input -D ./output compute.souffle.dl -o compute_new > rebuild.log 2>&1`
- Run rewrite: `./compute_new -r -p run_rewrite_prof_new.log -F ./input -D ./output_rewrite_prof > run_rewrite_prof_new.stdout 2>&1`
- Run no-rewrite: `./compute_new -p run_no_rewrite_prof_new.log -F ./input -D ./output_no_rewrite_prof > run_no_rewrite_prof_new.stdout 2>&1`
- Diff outputs: `diff output_no_rewrite_prof/facts.prob output_rewrite_prof/facts.prob`
- Clean dot (example P13): `rm -f rewrite_iter*.dot siso_regions_iter*.dot rewrite_final.dot`

### Quick run for multiple benchmarks
- Clean dot at repo root: `find experiments/side_channel_full -maxdepth 2 -name "*.dot" -delete`
- Build compute once (Souffle command above).
- Run rewrite in each directory: `for p in P13 P14 P15 P16 P17; do (cd experiments/side_channel_full/$p && ../../compute_new -r -p run_rewrite_prof_new.log -F ./input -D ./output_rewrite_prof > run_rewrite_prof_new.stdout 2>&1); done`
- Similarly for no-rewrite: drop `-r` and change output to `_no_rewrite_prof`.
- Diff after running: `for p in P13 P14 P15 P16 P17; do diff experiments/side_channel_full/$p/output_no_rewrite_prof/facts.prob experiments/side_channel_full/$p/output_rewrite_prof/facts.prob || echo "$p differs"; done`

## Open items (historical)
1. LinearTwoEdge rewrite not implemented.
2. Handle disjunction pattern: `a -> b`, `a' -> b`.
3. Fix coupling between split and evidence handling.
4. Optimize the split algorithm.
5. Parallelize rewrite when detected SISOs are independent.


## Implemented SISO RegionKind (fast path)
- `SingleHyperedge`: one hyperedge with exactly 1 SI (non-fact), the rest absorbable facts (primitive, single outgoing edge). Rewrite: absorb fact probability (consider negation only when absorbing facts via p/(1-p)), create a new SI->SO edge.
- `AllFactsToSO`: single edge whose inputs are all facts (no incoming edges, single outgoing edge, non-evidence/needOutput), SO becomes a fact, probability is edge coin * product of inputs (consider negation via p/(1-p)).
- `LinearTwoEdge`: entry->mid->exit, mid is non-query/evidence and has only one outgoing edge; entry edge may be negated, exit edge must not be negated. Rewrite: multiply probabilities, preserve entry polarity, create a single entry->exit edge.
- `ParallelEdge`: only detects parallel single-input edges (bucketed by input and polarity), no rewrite.
- `FanOutConverge`: SI (fact) fans out via single-input edges to xi, xi have unique outgoing edges that converge to the same SO with multi-input positive edges; SI->xi polarity must be consistent, mixed polarity deletes the subgraph. Rewrite: absorb SI coin (by polarity), fan-edge coin, converge-edge coin; SI becomes a fact; insert an SI->SO edge with probability 1; delete xi and related edges.
- `General`: disabled.

## Edge compaction (after each SISO round)
- Traverse all edges, absorb "fact inputs with a single outgoing edge" and respect negation: if the fact input is marked `bodyNegations[i]`, multiply by `(1-p)`, otherwise multiply by `p`. Only absorb `isFact && !hasEvidence() && !needOutput && outs.size()==1 && outs[0]==edge`, keep other inputs. Remove facts that become isolated; rebuild the new edge and update probability. Do not absorb other inputs without negation.

## Important constraints and semantics
- Fast path does not support regions with `not` by default, except for specific handling (fact absorption using 1-p, LinearTwoEdge entry negation, FanOutConverge consistent polarity).
- Fact absorption requires a single outgoing edge to avoid breaking external references; SISO detection also requires this for absorbable facts.
- Linear two-edge skips fast path if the exit edge is negated (fallback).
- ParallelEdge only detects, does not rewrite; mixed-polarity parallels are not merged.
- FanOutConverge: requires converge edge body to be all positive and fan polarity consistent; mixed polarity deletes the subgraph (SO preserved).

## Recent experiments and results
- P5: fan-out-converge hit once; rewrite/no-rewrite `facts.prob` consistent; detection time ~0 ms; debug dot adds 10-20 ms I/O.
- P13 (latest code): rewrite end-to-end ~105 ms; BDD build 731 ms (no-rewrite 2705 ms); facts.prob consistent; fan-out-converge 0 hits; parallel detection bucketed but not rewritten.
- Older P14-P17 (without latest compaction/negation handling): rewrite vs no-rewrite end-to-end speedup ~4.7x-9.6x, mainly from BDD node reduction; re-run on latest version to update stats.

## Performance and consistency (latest observations, BDD)
- P13: rewrite 163 ms; BDD build 13 ms (no-rewrite 2626 ms, per-node WMC 1642 ms); RV 6142->650 (ratio 0.106); facts.prob consistent. Debug dot ~15-20 ms I/O per round.
- P14: rewrite 227 ms; build 236 ms (no-rewrite 3557 ms, per-node WMC 3850 ms); RV 9017->1592 (ratio 0.177); facts.prob consistent.
- P15: rewrite 484 ms; build 1041 ms (no-rewrite 13532 ms, per-node WMC 17286 ms); RV 18130->4233 (ratio 0.233); facts.prob consistent.
- P16: rewrite 980 ms; build 1978 ms (no-rewrite 53678 ms, per-node WMC 63277 ms); RV 32011->7700 (ratio 0.241); facts.prob consistent.
- P17: rewrite 1484 ms; build 3587 ms (no-rewrite 96263 ms); per-node WMC 2008 ms; RV 43529->10500 (ratio 0.241); facts.prob consistent.
- P18: rewrite 2589 ms; build 6724 ms; per-node WMC 3504 ms; RV 55051->13286 (ratio 0.241); rewrite only.
- P19: rewrite 3092 ms; build 8577 ms; per-node WMC 5403 ms; RV 69446->16800 (ratio 0.242); rewrite only.
- Fast-detect time is in the ms range; enabling debug (dot output) adds 10-20 ms/round I/O.

## SDD status
- P13 SDD path WMC is abnormal: `KEY_SENSITIVE(20)` overflows to `8.1854755e+127` (BDD 0.944704). Initial weights look normal; suspected SddFormulaManager var index/weight mapping or var_count mismatch. Do not use SDD for now.

## Debug/logs
- `-p` outputs `[pipeline]` timings; rewrite rows include iteration count, region count, RV changes, etc.
- Fast-path detection logs (`SOUFFLE_SISO_FAST_DEBUG=1`) print skip reasons; default off to reduce overhead.
- Timing fields (GraphRewriter per-iter): `countBefore`, `detect`, `loop` (cond/rewritten/skip), `edgeList`, `compact`, `countAfter`, `dumpRegions`, `dumpDot(before/after)`, `preLog/log`, `remainder`.

## Remaining notes/TODO
- ParallelEdge rewrite not implemented; detection already buckets by polarity.
- If further improvements are needed, prioritize checking SddFormulaManager var mapping/var_count to fix SDD.
- Full negation support is incomplete; regions with `not` should fall back to BDD.
- Changing SingleHyperedge to allow SI as fact caused fact differences (external escape); currently keep SI non-fact.
- Clean old artifacts (outputs, dot) before running to avoid mixing results.

## Notes/risks
- Negation is not fully supported globally; regions with `not` should fall back to BDD except for explicitly handled cases.
- SingleHyperedge SI currently requires non-fact (relaxing it previously caused probability differences).
- SDD results are currently unreliable (P13 overflow); default to BDD.
- Clean old artifacts (outputs, dot) before running to avoid mixing results.

## Related commits
- `812ea4081` — docs(repo): refine README narratives
- `44778971e` — perf(rewrite): precompute output facts in pipeline
- `4dd403de4` — Translate Chinese comments and docs to English
