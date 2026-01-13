# Incremental Profiling Notes (Non-SEM Stages)

## Scope
- Focus: inc vs full slowdowns outside SEMINAIVE.
- Data source: per-stage tables in `README.eval.inc.md`.
- Assumption: SEMINAIVE fixes are already in place; see `README.dred.md`.

## Observations (data + code)
- PRUNING_INC dominates inc slowdowns on larger cases/deltas; speedup < 1.0 is common.
- PRUNING_INC is not incremental: it runs a full reachability BFS on the whole graph
  and rebuilds impacted maps via BFS per delta node.
- PRUNING_INC always dumps graph statistics in the prune path (even when not needed),
  which adds overhead on large graphs.
- Incremental prune disables bi-imp merging, so FC/WMC in inc often run on larger
  graphs than full.
- FORWARD_COMPILATION_INC rebuilds SCC/dependency graph and runs const analysis each
  turn, plus full scans of formula maps to drop invalid entries.
- WMC_INC still iterates all valid nodes; it only skips per-node recomputation using
  changedNodes but retains full-node scans.
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

## PRUNING_INC Issues (Current Evidence)
- ApplyDeltaDeletes scales with deleted ruleapps; most time is spent in edge lookup,
  adjacency vector erase, and global edge-key cleanup.
- Prune is effectively full-graph work: reachability BFS and mark/prune touch the
  entire live graph each turn.
- Impact-map rebuild is BFS per delta node, and dominates insert turns for large
  deltas (e.g., P17 inc5 impact_ms ~4.7s, build_view_ms ~2.3s).
- View build involves materializing large delta/live sets, which is expensive when
  live graph size is large or delta is large.

## Hypothesized Root Causes (Prune)
- `applyDeltaDeletes` removes edges by scanning per-node edge vectors and removing
  matching edges; this is O(sum of degrees of affected nodes).
- `prune()` always performs full reachability and re-labels every node/edge, even
  when deltas are small or localized.
- Impacted map rebuild is O(|delta_nodes| * (|V|+|E|)) in the worst case, because it
  does a BFS from each delta node.
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
3. Impact-map rebuild optimization
   - Replace per-delta BFS with a multi-source BFS for delta nodes.
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
