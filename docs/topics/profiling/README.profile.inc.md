# Incremental Profiling Notes (Non-SEM Stages)

## Source references
- [src/include/souffle/problog/ForwardCompilation.h](src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [src/include/souffle/problog/formula/CuddManager.h](src/include/souffle/problog/formula/CuddManager.h)


## Scope
- Focus: inc vs full slowdowns outside SEMINAIVE.
- Data source:
  - archived batch TSV summaries in `archive/2026-02-11/tsv/`
- Assumption: SEMINAIVE fixes are already in place; see `docs/topics/pipeline/README.dred.md`.

## Observations (data + code)
- PRUNING_INC dominates inc slowdowns on larger cases/deltas; speedup < 1.0 is common.
- PRUNING_INC is not incremental: it runs a full reachability BFS on the whole graph.
- Impacted-map rebuild in `prune-inc` is currently stubbed/empty; delta-reach falls back
  to analyzer BFS (see `docs/topics/pipeline/README.inc.region.md`).
- PRUNING_INC only dumps graph statistics when `--dumpstat`/`set dumpstat` is enabled.
- Incremental prune forces bi-imp merging off (merge is disabled in incremental CLI
  and when full-only is not enabled), so FC/WMC in inc often run on larger graphs
  than full.
- FORWARD_COMPILATION_INC builds a cycle dependency graph from the current view each
  turn. Insert-only updates may use a local dep-graph from delta-reachable edges;
  otherwise it uses the view cache (rebuilt when invalidated).
- Const analysis runs only when `--fold-const`/`--dumpconst` is enabled. Deletion prep
  erases delta-deleted formulas and sweeps `nodeFormulas`/`edgeFormulas` to drop
  invalid entries.
- WMC_INC still iterates all valid nodes; it decides per-node reuse/recompute based
  on `changedNodes` and evidence changes, so the node loop is still a full pass.
- `--inc-profile` prints `[inc-profile]` lines with PRUNING/FC/WMC sub-step timings.
- `--fc-profile` prints `[fc-profile]` lines with FC sub-phase counters/timings (see `docs/USAGE.md`).

## FC Profiling Update (2026-01-13, --fc-profile)
- Runs: P17–P20, inc0p1/inc0p3/inc0p5, `--det-opt --fc-profile` (post-del disabled by default).
- Data files:
  - `experiments/side_channel_inc_eval_small/fc-profile-summary-p17-p20-v4.tsv`
  - `experiments/side_channel_inc_eval_small/fc-preconfig-summary-p17-p20-v4.tsv`
  - `experiments/side_channel_inc_eval_small/cudd-createvar-summary-p17-p20-v4.tsv`
  - `experiments/side_channel_inc_eval_small/cudd-createvar-top5-p17-p20-v4.tsv`
- Insert time is usually dominated by CUDD preConfig: `insert_preconfig_ms/total_ms`
  ranges 0.41–0.87 (avg ~0.73). Insert loop remains non-trivial: `insert_loop_ms/total_ms`
  ranges 0.02–0.56 (avg ~0.19).
- CUDD preConfig is mostly `createVar(...)` time, and the long tail is driven by a
  small number of very slow fact-variable creates (up to ~2.5s each).
- `--post-del` (default false) eliminates delete-side variable postprocess overhead:
  delete total dropped ~5–6x on average; insert total improved ~10% (v3→v4).
- Full-mode FC output currently prints only iter1+iter2 (no explicit insert stage);
  need a follow-up run that isolates full insert to compare per-turn FC fairly.

### CUDD createVar spike check (2026-01-13, P17 inc0p1)
- Added `CUDD_CREATEVAR_STATS` lines (GC/reorder/swap/node/dead/slots/keys deltas) to
  correlate slow `Cudd_bddIthVar(...)` calls with CUDD internal events.
- Sample run: `experiments/side_channel_inc_eval_small/P17/output_fc_profile_spike/fc-profile-inc0p1-inc.stdout`.
- In the slowest createVar calls, `gc_delta` and `reorder_delta` stay 0, while
  `slots_delta` jumps (e.g., +104,734,720), indicating unique-table resize/rehash
  as the dominant spike source.

### numSlots adjustment follow-up (2026-01-13, P17 inc0p1)
- Rebuilt after changing `numSlots` init size; reran P17 inc0p1 with `--fc-profile`.
- Output: `experiments/side_channel_inc_eval_small/P17/output_fc_profile_spike2/fc-profile-inc0p1-inc.stdout`.
- Max `CUDD_CREATEVAR` time dropped to ~65ms (previously ~0.9–1.5s).
- `gc_delta/reorder_delta/swap_delta` remain 0; the largest spike still coincides
  with `slots_delta` growth (now +13,091,840), so resize/rehash is still the root
  cause, just much smaller.
- Insert preConfig share in this run is ~0.69 (down from ~0.93 in the earlier spike
  run), so the slowdown is reduced but not eliminated.

### Reordering runtime vs preConfig time (2026-01-13, fc-profile v4)
- Source: `output_fc_profile_v4` (inc) and `output_fc_profile_full_v4` (full) logs for
  P18–P20, turn3 (insert).
- `reordering_runtime` (CUDD dynamic reordering) is 0 in inc for these cases, so it
  does not explain the inc slowdowns. The dominant cost comes from the preConfig
  step (logged as `preConfig (cache clear + var scan/create + dyn-reorder setup) took ...`),
  which is ~2.4–3.1s in inc vs ~0.6–1.3s in full.

```tsv
Case	Delta	FullReorder_s	IncReorder_s	FullPreConfig_ms	IncPreConfig_ms
P18	inc0p1	4.220	0.000	563.506	2904
P18	inc0p3	4.250	0.000	606.597	2588
P18	inc0p5	4.180	0.000	600.057	2667
P19	inc0p1	5.580	0.000	577.484	2953
P19	inc0p3	5.520	0.000	659.838	2984
P19	inc0p5	5.570	0.000	647.535	3094
P20	inc0p1	1.860	0.000	1282.096	2497
P20	inc0p3	1.850	0.000	1026.752	2376
P20	inc0p5	1.860	0.000	1046.892	2435
```

Notes:
- P17 differs: inc reordering_runtime is non-zero there (~3–4s), so it can still be
  a contributor in that case.
- For P18–P20, inc slowdowns are better explained by explicit variable reordering +
  insert-loop work (e.g., `make_and` dominated loops).

### Inc preConfig toggle (historical; flag removed) (2026-01-14, P17–P20 inc0p1/inc0p3/inc0p5)
- Goal: evaluate skipping `preConfig(view)` in inc insert (flag removed since).
- Runs (historical):
  - `experiments/side_channel_inc_preconfig_off` (preConfig skipped)
  - `experiments/side_channel_inc_preconfig_on` (preConfig forced)
  - Both with `--det-opt`, `--timeout 600`, `--delta-labels inc0p1,inc0p3,inc0p5`.
- Result: skipping preConfig is slightly faster but causes correctness mismatches in
  P17 (inc0p3/0p5) and all P18 deltas. P19/P20 remain correct.

```tsv
Case	Delta	Inc_off_s	Inc_on_s	Full_off_s	Full_on_s	OK_off	OK_on
P17	inc0p1	5.309	5.578	12.309	13.194	1	1
P17	inc0p3	8.770	9.002	10.048	11.301	0	1
P17	inc0p5	9.121	10.549	10.564	11.599	0	1
P18	inc0p1	12.182	12.402	15.294	14.999	0	1
P18	inc0p3	13.788	12.945	14.567	13.843	0	1
P18	inc0p5	13.561	13.317	14.965	14.340	0	1
P19	inc0p1	13.221	14.050	28.797	29.005	1	1
P19	inc0p3	16.331	14.649	27.388	27.918	1	1
P19	inc0p5	15.239	15.953	28.015	27.948	1	1
P20	inc0p1	7.699	9.039	14.640	15.716	1	1
P20	inc0p3	9.051	9.083	15.121	17.305	1	1
P20	inc0p5	7.542	8.609	14.871	15.067	1	1
```

Implication:
- Full skip of preConfig is unsafe for correctness (likely due to stale WMC cache).
  The new behavior keeps the correctness-sensitive parts (cache clear) while
  limiting createVar scans to delta inserts, and only sets reordering heuristics
  on the first full turn.

### Inc preConfig (turn-sensitive delta scan) (2026-01-14, P17–P20 inc0p1/inc0p3/inc0p5)
- Behavior: preConfig runs every turn to clear WMC cache; when a delta exists it
  only scans `deltaInsert{Nodes,Edges}` for createVar. Dynamic reordering
  heuristics are set only on the first full turn.
- Runs:
  - `experiments/side_channel_inc_preconfig_delta` (delta-only createVar)
  - compared against `experiments/side_channel_inc_preconfig_on` (full preConfig)
- Result: delta-only preConfig is correct on all P17–P20 deltas; runtime changes
  are modest and mixed (often within noise; P20 shows consistent wins).

```tsv
Case	Delta	Inc_delta_s	Inc_full_preconfig_s	Full_delta_s	Full_full_preconfig_s	OK_delta	OK_full
P17	inc0p1	5.534	5.578	13.104	13.194	1	1
P17	inc0p3	9.339	9.002	11.424	11.301	1	1
P17	inc0p5	9.664	10.549	11.735	11.599	1	1
P18	inc0p1	13.234	12.402	16.269	14.999	1	1
P18	inc0p3	13.217	12.945	14.493	13.843	1	1
P18	inc0p5	13.007	13.317	13.468	14.340	1	1
P19	inc0p1	14.126	14.050	29.248	29.005	1	1
P19	inc0p3	13.824	14.649	26.598	27.918	1	1
P19	inc0p5	14.124	15.953	26.595	27.948	1	1
P20	inc0p1	6.436	9.039	12.999	15.716	1	1
P20	inc0p3	7.129	9.083	14.099	17.305	1	1
P20	inc0p5	7.408	8.609	14.028	15.067	1	1
```

## Profiling Run (2026-01-10, --inc-profile, full ruleset)
- Cases: P12, P17; deltas: inc1 (1%), inc5 (5%); 1 sample each.
- Command pattern:
  `./compute -F input -D output --setmode inc --logfile log_<case>_incprofile_<delta> --inc-profile < delta/<label>_1.txt`

Delete turn (turn2):
```tsv
Case	Delta	ApplyDelta_ms	Prune_ms	Outputless_ms	FC_total_ms	WMC_total_ms
P12	inc1	51.67	103.59	49.28	77.73	0.43
P12	inc5	403.29	77.99	33.59	107.89	0.16
P17	inc1	1335.08	912.88	173.62	729.04	3.87
P17	inc5	6652.29	770.90	75.59	655.85	0.94
```

Insert turn (turn3):
```tsv
Case	Delta	ApplyDelta_ms	Prune_ms	Impact_ms	BuildView_ms	FC_total_ms	FC_insert_loop_ms	WMC_total_ms
P12	inc1	60.79	671.69	375.79	169.11	363.03	124.38	0.63
P12	inc5	498.51	1713.79	1101.47	484.76	529.88	303.67	0.58
P17	inc1	1534.90	4455.73	1968.85	1174.23	7163.51	3774.22	18.81
P17	inc5	6917.51	8428.76	4731.23	2330.88	8840.19	5452.32	19.03
```

Notes:
- Insert turns are dominated by PRUNING_INC (impact-map rebuild + view build),
  then FC insert-prep/insert-loop; WMC remains small.
- Delete turns are dominated by applyDeltaDeletes + prune; FC/WMC are secondary.
  (Impact_map timings reflect older runs when prune-inc rebuilt impacted maps.)

## PRUNING_INC Issues (Current Evidence)
- ApplyDeltaDeletes scales with deleted ruleapps; most time is spent in edge lookup,
  adjacency vector erase, and global edge-key cleanup.
- Prune is effectively full-graph work: reachability BFS and mark/prune touch the
  entire live graph each turn.
- Impacted maps are currently empty in prune-inc; analyzer uses BFS fallback,
  so view build and reachability are the dominant prune costs.
- View build involves materializing large delta/live sets, which is expensive when
  live graph size is large or delta is large.

## Hypothesized Root Causes (Prune)
- `applyDeltaDeletes` removes edges by scanning per-node edge vectors and removing
  matching edges; this is O(sum of degrees of affected nodes).
- `prune()` always performs full reachability and re-labels every node/edge, even
  when deltas are small or localized.
- Incremental prune disables bi-imp merge, so the graph is larger than full mode
  for later stages.

## Candidate Fix Strategies (Prune)
1. Reduce applyDeltaDeletes overhead
   - Maintain edge adjacency indices (per-node hash sets or intrusive lists) to avoid
     vector erase scans, or batch deletions with stable-erase lists.
   - Store edge->(output,input indices) to remove from adjacency in O(1) per edge.
2. Make pruning incremental
   - Maintain reachability/ref-counts from outputs and update only affected regions.
   - Cache live/pruned state and avoid full relabeling when delta is small.
3. Impact-map rebuild (if reintroduced)
   - Prefer multi-source BFS for delta nodes.
   - Build impacted maps lazily (only for nodes used by FC) or cache and update.
4. Graph size controls
   - Re-enable bi-imp merge in inc if it can be proven safe for delta correctness.
   - Add an elastic threshold: if delta ratio is high, fall back to full rebuild.

## Hypotheses
- PRUNING_INC cost is near O(|V|+|E|) per turn, so it dominates when deltas are not tiny.
- Extra bookkeeping (delta sets, impacted maps) adds overhead without reducing graph
  traversal cost, so inc loses vs full in many cases.
- Disabling bi-imp merge inflates graph size, increasing FC/WMC time and erasing SEM wins.
- Formula map scans (std::map erase/iterate) scale with total graph size, not delta size.

## Experiment Plan
1. PRUNING_INC micro-timers
   - Break down: applyDelta, init outputs, BFS, mark live/pruned, filter delta,
     impacted-map rebuild, view build.
   - Gate dumpStatisticsInc behind a flag to isolate compute time.
2. Delta-size sensitivity
   - Run P8/P12/P17 with inc1/inc3/inc5; record PRN/FC/WMC ratios and delta ratios
     (delta edges / total edges).
3. Feature toggles
   - Ensure dumpstat/dumpdot/dumpjson are off in perf runs.
   - If safe, enable bi-imp merge in inc and compare graph sizes + FC/WMC time.
4. Impacted-map algorithm
   - Replace per-delta BFS with multi-source BFS; compare PRUNING_INC time.
5. FC/WMC focus
   - Cache cycle dependency graph; measure reuse.
   - Restrict WMC to dirty components; compare total WMC time.

## Candidate Solutions (if hypotheses confirm)
- Make prune truly incremental (reachability ref-counts; update only affected region).
- Compute impacted maps on demand or via multi-source BFS.
- Cache dependency graph and maintain dirty components across turns.
- Add an elastic switch: if delta ratio exceeds a threshold, fall back to full.

## Open Questions
- Can inc safely apply bi-imp merge after prune to shrink the graph without breaking
  delta correctness?

## Related commits
- `4bf38b2a1` — perf(problog): make inc preConfig delta-scoped
- `9ea1b4b53` — perf(problog): add inc preConfig toggle
- `c0378327d` — docs(readme): add FC reordering comparison
