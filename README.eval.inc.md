# Incremental Side-Channel Benchmark Guide

Incremental side-channel benchmark notes (context + procedures). Experiments live
under `experiments/side_channel_inc_eval/` (legacy) and
`experiments/side_channel_inc_trimmed_eval/` (trimmed ruleset, no equal_assign),
using the Souffle binary built from this repo. Do not git-add anything under
`experiments/` or other generated artifacts.

## Status
- Active evaluation workflow.
- 2026-01-10 (trimmed ruleset): P1,P3,P4-P20, inc1/inc3/inc5, sample=1, all OK.
- 2026-01-09 (post-fix rerun): P1,P3,P4-P20, inc1/inc3/inc5, sample=1, all OK.
- 2026-01-09 (pre-fix run): P1,P3,P4-P20, inc1/inc3/inc5, sample=1, P7/P20 mismatches (see Results).
- 2025-12-21 (inc10 sample=1): P4-P13 snapshot + notes preserved below.

## Scope
- Online incremental CLI path only (online is default; `--online` optional;
  inc-naive/inc-regional). The legacy `--inc` backend is removed.
- Incremental modes do not run rewrite; they reuse the online DRed-like deletion
  and rederive paths.
- Full baseline uses `full-hard` by default (`--setmode full`); `full-soft` is
  available for reuse of the DD manager state.

## Prerequisites
- Build release Souffle:
  ```bash
  cmake -S . -B build
  JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
  cmake --build build -j${JOBS}
  ```
- Put the build binary on PATH before running the Python scripts:
  ```bash
  export PATH="/home/hugh/research/datalog/souffle/build/src:$PATH"
  ```
- If you build into `cmake-build-release`, adjust the PATH to
  `.../cmake-build-release/src`.

## End-to-end for P1 (from repo root)
```bash
# 1) Generate inputs from SMT
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval generate --cases 1 --cleanup

# 2) Generate delta workloads (default change spec; overwrites delta/)
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval delta --cases 1 --cleanup

# 3) Compile Souffle (online default; produces ./compute in P1/)
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval compile --cases 1 --timeout 600

# 4) Run baseline (full+inc) and one sample for inc1/inc3/inc5
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval run   --cases 1 --delta-labels inc1,inc3,inc5 --delta-samples 1 --timeout 600

# 5) Collect summary TSV
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval collect --cases 1
```

## What to look at
- Outputs: `output/facts.full.prob`, `output/facts.inc.prob`, and per-turn
  `output/fact-iterN-{inc-naive,inc-regional,full}.prob`. Runner outputs are
  prefixed per delta, e.g.
  `output/delta-<label>-<sample>-{inc-naive,inc-regional,full}-fact-iterN-<mode>.prob`.
- Logs: debugger JSON reports land in `output/` (e.g.,
  `output/log_P1_inc1_1_inc_*.json`); stdout from manual runs (e.g.,
  `log_inc_applyDelta_inc*.stdout`) still holds CLI prints.
- Debugger JSON `turns[].mode` shows `FULL-HARD` / `FULL-SOFT` / `INC`.
- CLI runs in inc mode print delta size:
  `[applyDelta] delTuples=... delRuleApps=... delFacts=... insTuples=... insRuleApps=... insFacts=...`.
- CLI logs pre-prune applyDelta counts (symmetric) and view-update counts (asymmetric):
  `[inc-iter N] mode=INC_NAIVE apply_delta_ops: delTuples=... delRuleApps=... delFacts=... insTuples=... insRuleApps=... insFacts=...`.
  `[inc-iter N] mode=INC_NAIVE apply_delta_view: insNodes=... insEdges=... delNodes=... delEdges=...` (view/prune-driven).
- Forward compilation logs print delta counts:
  `[inc-naive] delta counts: insNodes=... insEdges=... delNodes=... delEdges=...`.
- Deletion stage prints `Deletion deletedVarsIndex size: N` before postprocessing
  variables.
- Prune prints delta-delete counts per phase:
  `[prune-inc] delta-delete counts (start|post-mark-pruned|filtered|canonicalised|view): nodes=... edges=...`.
- Full prune prints graph sizes:
  `[prune-full] pre nodes=... edges=...` and `[prune-full] post nodes=... edges=...`.
- Prune now rebuilds impacted maps on the pruned subgraph; applyDelta no longer
  does impacted BFS.
- Optional debug outputs (default off): `--dumpjson`, `--dumpdot`, `--dumpstat`
  gate JSON/DOT/stats dumps after prune, all written under the `-D` output dir.

## Implementation pointers (online path)
- `src/ast2ram/online/UnitTranslator.cpp`: `inc_table_update` + `_inc` strata.
- `src/synthesiser/Synthesiser.cpp`: `runFunctionInc` and `runAllInc` in generated code.
- `src/include/souffle/cli/Cli.h`: applyDelta + mode dispatch (inc-naive/inc-regional).

## Latest status / known issues
- 2026-01-10 (trimmed ruleset): correctness OK for P1,P3,P4-P20 (P2 missing in source). Summary counts: inc1 7/19 faster, inc3 7/19 faster, inc5 1/19 faster.
- 2026-01-09 (post-fix rerun): correctness OK for P1,P3,P4-P20 (P2 missing in source). Summary counts: inc1 16/19 faster (ok=19/19), inc3 8/19 faster (ok=19/19), inc5 2/19 faster (ok=19/19).
- 2026-01-09 (pre-fix run): correctness mismatches (inc_iter1_vs_full_iter1): P7 inc1/inc3/inc5 mismatches=1 max|d|=0.10239319; P20 inc5 mismatches=2 max|d|=0.2522536. Summary counts: inc1 16/19 faster (ok=18/19), inc3 10/19 faster (ok=18/19), inc5 6/19 faster (ok=17/19).
- Delta-size sensitivity: inc1 often faster, inc3 mixed, inc5 usually slower (full recompute wins for larger deltas).
- PRUNING_INC dominates on larger cases and especially on insert turns; see the
  per-stage breakdown table for ratios (Speedup < 1.0 indicates inc slower).
- Delta node/edge counts are taken from forward compilation logs and can be
  asymmetric due to prune/derivation changes; they are not the same as input facts.
- inc-regional is not exercised in the current runs (logs show inc-naive only).
- Current inc1 workloads have no disjunctions, so inc-regional degenerates to inc-naive.
- Analyzer cost has been reduced and is now negligible relative to forward compilation.
- The printed delete/insert timers are small and do not trigger reordering, yet
  incremental delete/insert still show long forward-compilation time relative to
  the delta size; needs profiling to confirm whether this is expected.
- DRed deletion is still inefficient; no fix yet.
- Incrementalizing prune is under consideration but lower priority; prune time
  still contributes non-trivially.
- Elastic incremental pipeline remains a plan; design/implementation is not settled.
- To exercise inc-regional properly we likely need HV-related rules (richer
  side-channel reasoning) or another benchmark (e.g., data race).
- inc-naive (and current inc-regional) is faster than full largely because delta
  impact is small and variable ordering from turn-1 is reused (no reordering on
  insert/delete).
- Continuous inc-regional state maintenance/correctness is still pending; current
  experiments only include a single inc-regional turn (no calibration, so no
  outdated DD applied).
- Earlier runs saw P1/P3 incremental insertion loop in ForwardCompilation on the
  KEY_SENSITIVE eq-cycle; if it reappears, skip those cases.
- 2025-12-21 (inc10): incremental pruning still dominates time for larger cases
  (P12/P13), e.g. P13 inc10: prune ~= 2.6s, forward ~= 1.07s, total ~= 3.71s;
  full run forward+wmc is heavy but still faster overall (~=3.40s).
- 2025-12-21 (inc10): all P4-P13 inc10 runs (delta sample 1) match full results
  (`max|d|=0`) as recorded in `output/delta-inc10-1.json` per case.
- 2025-12-21 (inc10): generated logs per case
  `output/log_P*_inc10_1_inc_*.json` and `output/log_P*_inc10_1_full_*.json`;
  `output/delta-inc10-1.json` holds consistency checks.
- 2025-12-21 (inc10): P12 manual repro (delta inc10_1) still matches; turn-3
  reordering ~=0.21s; live_nodes turn2/3 reflect deletions (inc-naive 1642/11870,
  inc-regional 1625/10545, full 1868/13817).
- Incremental runs disable bi-imp merge; if a graph is merged (full mode),
  switching to inc/inc-regional asserts to avoid cache mismatch.

## Results (inc1/inc3/inc5, sample=1)
Data sources:
- `experiments/side_channel_inc_trimmed_eval/results-souffle-inc.tsv` (end-to-end, 2026-01-10)
- `experiments/side_channel_inc_trimmed_eval/P*/output/log_P*_<label>_1_{inc,full}_*.json`
  (per-stage breakdown, delta counts, 2026-01-10)
- `experiments/side_channel_inc_eval/results-souffle-inc.tsv` (end-to-end, 2026-01-09)
- `experiments/side_channel_inc_eval/P*/output/log_P*_<label>_1_{inc,full}_*.json`
  (per-stage breakdown, delta counts, 2026-01-09)

### End-to-end wall time (inc vs full)
Speedup = FullTime / IncTime (>1.0 means inc faster).
#### 2026-01-10 (trimmed ruleset, no equal_assign)
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc1	0.337566	1.286437	3.811	1
P1	inc3	0.370647	1.028006	2.774	1
P1	inc5	1.236234	0.674332	0.545	1
P3	inc1	1.162363	0.737374	0.634	1
P3	inc3	1.104880	0.732703	0.663	1
P3	inc5	1.189762	0.726775	0.611	1
P4	inc1	0.015281	0.013889	0.909	1
P4	inc3	0.018728	0.015583	0.832	1
P4	inc5	0.015677	0.014013	0.894	1
P5	inc1	0.016249	0.015899	0.978	1
P5	inc3	0.014509	0.020673	1.425	1
P5	inc5	0.017839	0.015902	0.891	1
P6	inc1	0.017493	0.018616	1.064	1
P6	inc3	0.021958	0.022397	1.020	1
P6	inc5	0.023372	0.022702	0.971	1
P7	inc1	0.027244	0.026758	0.982	1
P7	inc3	0.031928	0.033618	1.053	1
P7	inc5	0.039772	0.037617	0.946	1
P8	inc1	0.033662	0.034403	1.022	1
P8	inc3	0.040625	0.038162	0.939	1
P8	inc5	0.047059	0.046882	0.996	1
P9	inc1	0.033846	0.033542	0.991	1
P9	inc3	0.041368	0.041887	1.013	1
P9	inc5	0.048792	0.048052	0.985	1
P10	inc1	0.119009	0.137689	1.157	1
P10	inc3	0.192643	0.212458	1.103	1
P10	inc5	0.307846	0.284290	0.923	1
P11	inc1	0.102617	0.117096	1.141	1
P11	inc3	0.190447	0.197392	1.036	1
P11	inc5	0.269119	0.274488	1.020	1
P12	inc1	0.303927	0.251685	0.828	1
P12	inc3	0.451209	0.304003	0.674	1
P12	inc5	0.486506	0.381906	0.785	1
P13	inc1	1.539222	1.033123	0.671	1
P13	inc3	1.475342	1.123304	0.761	1
P13	inc5	1.528772	1.300920	0.851	1
P14	inc1	2.375514	2.317190	0.975	1
P14	inc3	2.644482	2.163876	0.818	1
P14	inc5	2.774842	2.387992	0.861	1
P15	inc1	6.447595	8.072398	1.252	1
P15	inc3	7.621881	7.134407	0.936	1
P15	inc5	8.958616	7.720081	0.862	1
P16	inc1	17.786523	14.867957	0.836	1
P16	inc3	17.096539	11.900136	0.696	1
P16	inc5	17.910979	13.288718	0.742	1
P17	inc1	22.690092	15.751412	0.694	1
P17	inc3	25.109208	14.663478	0.584	1
P17	inc5	25.437655	15.934298	0.626	1
P18	inc1	23.024591	17.099180	0.743	1
P18	inc3	27.333167	17.007375	0.622	1
P18	inc5	31.320941	18.466959	0.590	1
P19	inc1	36.558594	23.779726	0.650	1
P19	inc3	42.888465	23.127642	0.539	1
P19	inc5	44.262132	25.203704	0.569	1
P20	inc1	15.179235	16.208378	1.068	1
P20	inc3	24.246785	17.372865	0.717	1
P20	inc5	32.598067	23.049153	0.707	1
```

#### 2026-01-09 (post-fix rerun)
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc1	0.194069	0.519645	2.678	1
P1	inc3	0.229571	0.513915	2.239	1
P1	inc5	0.594321	0.441185	0.742	1
P3	inc1	0.647965	0.405309	0.626	1
P3	inc3	0.666429	0.449008	0.674	1
P3	inc5	0.754287	0.413788	0.549	1
P4	inc1	0.015575	0.015974	1.026	1
P4	inc3	0.014534	0.016409	1.129	1
P4	inc5	0.018372	0.017835	0.971	1
P5	inc1	0.017280	0.015438	0.893	1
P5	inc3	0.017071	0.016748	0.981	1
P5	inc5	0.015393	0.014487	0.941	1
P6	inc1	0.050608	0.048368	0.956	1
P6	inc3	0.058748	0.048011	0.817	1
P6	inc5	0.056322	0.051558	0.915	1
P7	inc1	0.109587	0.120688	1.101	1
P7	inc3	0.137401	0.131418	0.956	1
P7	inc5	0.137810	0.128632	0.933	1
P8	inc1	0.267559	0.430122	1.608	1
P8	inc3	0.467368	0.404941	0.866	1
P8	inc5	1.633401	0.349616	0.214	1
P9	inc1	0.124938	0.199879	1.600	1
P9	inc3	0.210857	0.202309	0.959	1
P9	inc5	0.221700	0.207476	0.936	1
P10	inc1	0.412860	0.645395	1.563	1
P10	inc3	0.463995	0.646879	1.394	1
P10	inc5	0.604941	0.745003	1.232	1
P11	inc1	0.342649	0.547737	1.599	1
P11	inc3	0.429868	0.661546	1.539	1
P11	inc5	0.529117	0.677693	1.281	1
P12	inc1	3.214876	3.671281	1.142	1
P12	inc3	7.173889	3.518608	0.490	1
P12	inc5	7.746429	3.283018	0.424	1
P13	inc1	4.535383	7.599825	1.676	1
P13	inc3	6.864388	6.955791	1.013	1
P13	inc5	7.831079	6.874586	0.878	1
P14	inc1	8.530714	11.769947	1.380	1
P14	inc3	10.822894	11.146831	1.030	1
P14	inc5	13.294106	11.402932	0.858	1
P15	inc1	19.302159	30.179378	1.564	1
P15	inc3	27.430529	28.559028	1.041	1
P15	inc5	31.070705	28.031695	0.902	1
P16	inc1	33.837666	48.993638	1.448	1
P16	inc3	44.546503	45.624610	1.024	1
P16	inc5	51.444408	45.489276	0.884	1
P17	inc1	53.039856	68.598614	1.293	1
P17	inc3	67.651573	65.426974	0.967	1
P17	inc5	77.143617	64.360135	0.834	1
P18	inc1	70.452234	84.913686	1.205	1
P18	inc3	88.677936	83.868373	0.946	1
P18	inc5	101.166053	81.792562	0.808	1
P19	inc1	96.114424	112.246172	1.168	1
P19	inc3	122.202203	110.635033	0.905	1
P19	inc5	137.009689	110.528643	0.807	1
P20	inc1	56.208667	79.573999	1.416	1
P20	inc3	86.931920	83.349990	0.959	1
P20	inc5	172.173196	81.444416	0.473	1
```

#### 2026-01-09 (pre-fix run)
```tsv
Case	Delta	IncTime	FullTime	Speedup	OK
P1	inc1	0.604551	0.373050	0.617	1
P1	inc3	0.790896	0.396658	0.502	1
P1	inc5	0.681234	0.409831	0.602	1
P3	inc1	0.653207	0.387377	0.593	1
P3	inc3	0.529795	0.386704	0.730	1
P3	inc5	0.676294	0.397174	0.587	1
P4	inc1	0.012401	0.014256	1.150	1
P4	inc3	0.012129	0.015080	1.243	1
P4	inc5	0.013769	0.015167	1.102	1
P5	inc1	0.014648	0.014195	0.969	1
P5	inc3	0.013719	0.013065	0.952	1
P5	inc5	0.017921	0.014458	0.807	1
P6	inc1	0.035919	0.043847	1.221	1
P6	inc3	0.060882	0.040107	0.659	1
P6	inc5	0.058952	0.043452	0.737	1
P7	inc1	0.074652	0.112049	1.501	0
P7	inc3	0.079892	0.113963	1.426	0
P7	inc5	0.085110	0.115735	1.360	0
P8	inc1	0.211576	0.385113	1.820	1
P8	inc3	0.250679	0.399446	1.593	1
P8	inc5	0.394993	0.374243	0.947	1
P9	inc1	0.127537	0.181056	1.420	1
P9	inc3	0.143274	0.188003	1.312	1
P9	inc5	0.154803	0.204918	1.324	1
P10	inc1	0.350396	0.520656	1.486	1
P10	inc3	0.392633	0.606679	1.545	1
P10	inc5	0.545664	0.664452	1.218	1
P11	inc1	0.312864	0.580883	1.857	1
P11	inc3	0.392436	0.570571	1.454	1
P11	inc5	0.474962	0.634948	1.337	1
P12	inc1	1.946632	3.240192	1.665	1
P12	inc3	3.482785	3.211639	0.922	1
P12	inc5	4.074973	3.221125	0.790	1
P13	inc1	4.143678	6.442994	1.555	1
P13	inc3	5.153035	6.808034	1.321	1
P13	inc5	5.836867	6.328045	1.084	1
P14	inc1	7.559217	10.818736	1.431	1
P14	inc3	9.485047	10.214262	1.077	1
P14	inc5	11.899553	10.071940	0.846	1
P15	inc1	17.405719	27.654654	1.589	1
P15	inc3	23.352126	26.472463	1.134	1
P15	inc5	26.944416	25.539730	0.948	1
P16	inc1	32.918036	44.385915	1.348	1
P16	inc3	44.361851	42.919150	0.967	1
P16	inc5	49.746649	42.566471	0.856	1
P17	inc1	52.646017	65.098837	1.237	1
P17	inc3	64.572775	61.473317	0.952	1
P17	inc5	71.500162	62.564490	0.875	1
P18	inc1	63.116400	81.246020	1.287	1
P18	inc3	80.687500	78.303712	0.970	1
P18	inc5	92.060303	76.914889	0.835	1
P19	inc1	91.542006	106.882494	1.168	1
P19	inc3	116.656206	103.822104	0.890	1
P19	inc5	128.814061	103.063635	0.800	1
P20	inc1	48.325213	79.132816	1.638	1
P20	inc3	70.973095	75.755336	1.067	1
P20	inc5	85.374623	76.569061	0.897	0
```

### Legacy quick snapshot (inc10 sample=1, 2025-12-21)
- P4-P9: inc ~=0.10-0.20s, full ~=0.09-0.17s (roughly parity; prune dominates inc).
- P10-P11: inc ~=0.68/0.66s vs full ~=0.94/0.63s.
- P12: inc ~=2.80s vs full ~=2.18s (prune+fwd heavy).
- P13: inc ~=8.27s vs full ~=5.92s (prune+fwd heavy).

### Approximate per-case evaluation time (inc1/inc3/inc5, 2026-01-09 post-fix)
DeltaEval_s sums inc+full wall time for the three deltas; add baseline full+inc
overhead (not timed in the table) and compile time (~30-70s per case) when planning
end-to-end runs. Full suite delta runs sum to ~45.7 min on this machine.
```tsv
Case	DeltaEval_s	DeltaEval_min
P1	2.493	0.04
P3	3.337	0.06
P4	0.099	0.00
P5	0.096	0.00
P6	0.314	0.01
P7	0.766	0.01
P8	3.553	0.06
P9	1.167	0.02
P10	3.519	0.06
P11	3.189	0.05
P12	28.608	0.48
P13	40.661	0.68
P14	66.967	1.12
P15	164.573	2.74
P16	269.936	4.50
P17	396.221	6.60
P18	510.871	8.51
P19	688.736	11.48
P20	559.682	9.33
```

### Post-prune delta node/edge counts (inc logs, 2026-01-09 post-fix)
Counts are from `[inc-naive] delta counts: ...` in `FORWARD_COMPILATION_INC` logs; del counts
are from turn2 (delete) and ins counts from turn3 (insert) for each delta. These are
view/prune deltas and can be asymmetric; use `apply_delta_ops` for SEM delta proxies.
```tsv
Case	Delta	DelNodes	DelEdges	InsNodes	InsEdges	NodeRatio	EdgeRatio
P1	inc1	22	97	27	102	1.227	1.052
P1	inc3	37	161	42	166	1.135	1.031
P1	inc5	37	161	42	166	1.135	1.031
P3	inc1	0	0	0	0	0.000	0.000
P3	inc3	22	97	27	102	1.227	1.052
P3	inc5	44	194	54	204	1.227	1.052
P4	inc1	0	0	0	0	0.000	0.000
P4	inc3	0	0	0	0	0.000	0.000
P4	inc5	0	0	0	0	0.000	0.000
P5	inc1	10	9	49	24	4.900	2.667
P5	inc3	10	9	49	24	4.900	2.667
P5	inc5	11	9	49	24	4.455	2.667
P6	inc1	24	240	40	248	1.667	1.033
P6	inc3	26	241	42	249	1.615	1.033
P6	inc5	34	247	115	284	3.382	1.150
P7	inc1	42	370	83	386	1.976	1.043
P7	inc3	69	726	125	752	1.812	1.036
P7	inc5	69	726	125	752	1.812	1.036
P8	inc1	23	22	100	61	4.348	2.773
P8	inc3	100	1831	172	1867	1.720	1.020
P8	inc5	184	1915	1043	10793	5.668	5.636
P9	inc1	53	50	187	102	3.528	2.040
P9	inc3	151	772	421	875	2.788	1.133
P9	inc5	168	783	439	886	2.613	1.132
P10	inc1	12	16	26	33	2.167	2.062
P10	inc3	17	27	31	44	1.824	1.630
P10	inc5	31	52	86	227	2.774	4.365
P11	inc1	0	0	0	0	0.000	0.000
P11	inc3	2	1	4	2	2.000	2.000
P11	inc5	2	1	4	2	2.000	2.000
P12	inc1	504	4973	1764	8453	3.500	1.700
P12	inc3	1376	19805	3808	31184	2.767	1.575
P12	inc5	1641	20037	4694	31697	2.860	1.582
P13	inc1	723	684	3001	1697	4.151	2.481
P13	inc3	1975	6343	7347	9797	3.720	1.545
P13	inc5	2626	8325	8786	11785	3.346	1.416
P14	inc1	1723	3714	7036	13150	4.084	3.541
P14	inc3	3206	5480	11897	17761	3.711	3.241
P14	inc5	3770	5930	13507	23746	3.583	4.004
P15	inc1	4608	4495	17334	18122	3.762	4.032
P15	inc3	8237	9012	29263	38780	3.553	4.303
P15	inc5	9363	9918	29807	39087	3.183	3.941
P16	inc1	9367	12226	32825	21962	3.504	1.796
P16	inc3	15366	17810	47905	30688	3.118	1.723
P16	inc5	16850	18877	49909	41863	2.962	2.218
P17	inc1	15592	20501	53020	44176	3.400	2.155
P17	inc3	21318	26793	66418	51900	3.116	1.937
P17	inc5	23314	28986	68161	53745	2.924	1.854
P18	inc1	21180	25403	70609	45796	3.334	1.803
P18	inc3	27885	31381	85036	54923	3.050	1.750
P18	inc5	29978	35773	87204	65309	2.909	1.826
P19	inc1	28080	27607	92977	53429	3.311	1.935
P19	inc3	34738	33327	106692	63168	3.071	1.895
P19	inc5	37339	34978	108749	64349	2.912	1.840
P20	inc1	8066	15948	34194	36828	4.239	2.309
P20	inc3	19644	34445	77738	60641	3.957	1.761
P20	inc5	27631	80451	98066	116734	3.549	1.451
```

### Pre-prune applyDelta op counts (P17 inc1, 2026-01-09 post-fix)
Counts are captured from the `PRUNING_INC` stage log line
`[inc-iter N] mode=INC_NAIVE apply_delta_ops: ...` (after applyDelta, before prune). These
are symmetric and should be used as SEM delta proxies.
In this dataset, deleted facts with surviving derivations do not occur, so applyDelta
op counts are symmetric across delete/insert turns.
```tsv
Turn	ApplyDelTuples	ApplyDelRuleApps	ApplyDelFacts	ApplyInsTuples	ApplyInsRuleApps	ApplyInsFacts
del	46521	221157	829	0	0	0
ins	0	0	0	46521	221157	829
```

### Pre-prune applyDelta view-update counts (P17 inc1, 2026-01-09 post-fix)
Counts are captured from the `PRUNING_INC` stage log line
`[inc-iter N] mode=INC_NAIVE apply_delta_view: ...` (after applyDelta, before prune). These
are asymmetric because the view/prune bookkeeping reintroduces nodes/edges that remain derivable.
```tsv
Turn	InsNodes	InsEdges	DelNodes	DelEdges
del	0	0	28471	221157
ins	46912	221157	0	0
```

### Per-stage breakdown (turn2=delete, turn3=insert)
Speedup = Full_s / Inc_s (>1.0 means inc faster).
#### 2026-01-09 (post-fix rerun)
```tsv
Case	Delta	Turn	Stage	Full_s	Inc_s	Speedup
P1	inc1	del	SEM	0.150286	0.001311	114.649
P1	inc1	del	PRN	0.004109	0.004426	0.928
P1	inc1	del	FC	0.000316	0.000485	0.652
P1	inc1	del	WMC	0.000026	0.000026	1.019
P1	inc1	ins	SEM	0.165449	0.003246	50.966
P1	inc1	ins	PRN	0.004916	0.004589	1.071
P1	inc1	ins	FC	0.000477	0.001047	0.456
P1	inc1	ins	WMC	0.000026	0.000026	1.026
P1	inc3	del	SEM	0.152345	0.003865	39.419
P1	inc3	del	PRN	0.003999	0.005571	0.718
P1	inc3	del	FC	0.000230	0.000646	0.356
P1	inc3	del	WMC	0.000026	0.000031	0.837
P1	inc3	ins	SEM	0.155578	0.004302	36.161
P1	inc3	ins	PRN	0.004639	0.005924	0.783
P1	inc3	ins	FC	0.000459	0.001885	0.244
P1	inc3	ins	WMC	0.000027	0.000028	0.968
P1	inc5	del	SEM	0.087959	0.139243	0.632
P1	inc5	del	PRN	0.002451	0.092785	0.026
P1	inc5	del	FC	0.000227	0.000662	0.343
P1	inc5	del	WMC	0.000025	0.000028	0.887
P1	inc5	ins	SEM	0.151069	0.058631	2.577
P1	inc5	ins	PRN	0.003911	0.091307	0.043
P1	inc5	ins	FC	0.000484	0.001864	0.260
P1	inc5	ins	WMC	0.000026	0.000029	0.902
P3	inc1	del	SEM	0.043786	0.119938	0.365
P3	inc1	del	PRN	0.001761	0.120696	0.015
P3	inc1	del	FC	0.000448	0.000336	1.332
P3	inc1	del	WMC	0.000025	0.000035	0.715
P3	inc1	ins	SEM	0.158259	0.086936	1.820
P3	inc1	ins	PRN	0.004857	0.120315	0.040
P3	inc1	ins	FC	0.000484	0.000297	1.632
P3	inc1	ins	WMC	0.000028	0.000036	0.779
P3	inc3	del	SEM	0.048714	0.111859	0.435
P3	inc3	del	PRN	0.002523	0.113811	0.022
P3	inc3	del	FC	0.000322	0.000610	0.527
P3	inc3	del	WMC	0.000027	0.000027	0.972
P3	inc3	ins	SEM	0.178918	0.091577	1.954
P3	inc3	ins	PRN	0.005756	0.158794	0.036
P3	inc3	ins	FC	0.000469	0.001140	0.412
P3	inc3	ins	WMC	0.000027	0.000029	0.944
P3	inc5	del	SEM	0.020018	0.112539	0.178
P3	inc5	del	PRN	0.000865	0.134470	0.006
P3	inc5	del	FC	0.000198	0.000677	0.292
P3	inc5	del	WMC	0.000029	0.000027	1.068
P3	inc5	ins	SEM	0.172734	0.121563	1.421
P3	inc5	ins	PRN	0.005395	0.167900	0.032
P3	inc5	ins	FC	0.000512	0.001833	0.279
P3	inc5	ins	WMC	0.000029	0.000029	1.010
P4	inc1	del	SEM	0.000961	0.000929	1.035
P4	inc1	del	PRN	0.000143	0.000201	0.709
P4	inc1	del	FC	0.000095	0.000052	1.838
P4	inc1	del	WMC	0.000024	0.000031	0.776
P4	inc1	ins	SEM	0.000951	0.001542	0.616
P4	inc1	ins	PRN	0.000133	0.000184	0.724
P4	inc1	ins	FC	0.000076	0.000058	1.309
P4	inc1	ins	WMC	0.000024	0.000032	0.736
P4	inc3	del	SEM	0.001457	0.002614	0.557
P4	inc3	del	PRN	0.000187	0.000216	0.864
P4	inc3	del	FC	0.000116	0.000054	2.154
P4	inc3	del	WMC	0.000030	0.000029	1.014
P4	inc3	ins	SEM	0.000963	0.000760	1.267
P4	inc3	ins	PRN	0.000147	0.000156	0.943
P4	inc3	ins	FC	0.000075	0.000043	1.736
P4	inc3	ins	WMC	0.000022	0.000029	0.766
P4	inc5	del	SEM	0.000834	0.002952	0.282
P4	inc5	del	PRN	0.000132	0.000315	0.420
P4	inc5	del	FC	0.000097	0.000046	2.108
P4	inc5	del	WMC	0.000024	0.000030	0.789
P4	inc5	ins	SEM	0.001669	0.001434	1.164
P4	inc5	ins	PRN	0.000124	0.000257	0.484
P4	inc5	ins	FC	0.000095	0.000037	2.590
P4	inc5	ins	WMC	0.000024	0.000024	1.000
P5	inc1	del	SEM	0.000278	0.000697	0.399
P5	inc1	del	PRN	0.000138	0.000167	0.826
P5	inc1	del	FC	0.000095	0.000850	0.111
P5	inc1	del	WMC	0.000024	0.000025	0.944
P5	inc1	ins	SEM	0.000322	0.001612	0.200
P5	inc1	ins	PRN	0.000343	0.000428	0.801
P5	inc1	ins	FC	0.000220	0.000434	0.507
P5	inc1	ins	WMC	0.000026	0.000033	0.800
P5	inc3	del	SEM	0.000273	0.001671	0.164
P5	inc3	del	PRN	0.000106	0.000145	0.732
P5	inc3	del	FC	0.000078	0.000846	0.092
P5	inc3	del	WMC	0.000024	0.000024	0.988
P5	inc3	ins	SEM	0.000295	0.002238	0.132
P5	inc3	ins	PRN	0.000294	0.000496	0.593
P5	inc3	ins	FC	0.000211	0.000446	0.473
P5	inc3	ins	WMC	0.000026	0.000033	0.773
P5	inc5	del	SEM	0.000273	0.000872	0.313
P5	inc5	del	PRN	0.000123	0.000149	0.828
P5	inc5	del	FC	0.000096	0.000841	0.114
P5	inc5	del	WMC	0.000024	0.000024	0.983
P5	inc5	ins	SEM	0.000296	0.001256	0.236
P5	inc5	ins	PRN	0.000316	0.000444	0.712
P5	inc5	ins	FC	0.000213	0.000361	0.590
P5	inc5	ins	WMC	0.000026	0.000027	0.968
P6	inc1	del	SEM	0.003598	0.005488	0.656
P6	inc1	del	PRN	0.002722	0.001715	1.587
P6	inc1	del	FC	0.001793	0.002520	0.711
P6	inc1	del	WMC	0.000032	0.000031	1.037
P6	inc1	ins	SEM	0.003528	0.003041	1.160
P6	inc1	ins	PRN	0.003410	0.005600	0.609
P6	inc1	ins	FC	0.002244	0.004390	0.511
P6	inc1	ins	WMC	0.000034	0.000032	1.066
P6	inc3	del	SEM	0.003516	0.003968	0.886
P6	inc3	del	PRN	0.002759	0.001720	1.605
P6	inc3	del	FC	0.001610	0.002525	0.637
P6	inc3	del	WMC	0.000032	0.000031	1.041
P6	inc3	ins	SEM	0.003548	0.008508	0.417
P6	inc3	ins	PRN	0.003360	0.005859	0.573
P6	inc3	ins	FC	0.002300	0.005365	0.429
P6	inc3	ins	WMC	0.000034	0.000033	1.015
P6	inc5	del	SEM	0.003803	0.008326	0.457
P6	inc5	del	PRN	0.002550	0.001747	1.459
P6	inc5	del	FC	0.002038	0.002544	0.801
P6	inc5	del	WMC	0.000030	0.000048	0.620
P6	inc5	ins	SEM	0.003853	0.002809	1.372
P6	inc5	ins	PRN	0.003297	0.006148	0.536
P6	inc5	ins	FC	0.002055	0.005048	0.407
P6	inc5	ins	WMC	0.000034	0.000033	1.026
P7	inc1	del	SEM	0.013004	0.009794	1.328
P7	inc1	del	PRN	0.008436	0.006041	1.397
P7	inc1	del	FC	0.005885	0.006510	0.904
P7	inc1	del	WMC	0.000040	0.000045	0.892
P7	inc1	ins	SEM	0.014474	0.004787	3.023
P7	inc1	ins	PRN	0.009775	0.013014	0.751
P7	inc1	ins	FC	0.006315	0.008631	0.732
P7	inc1	ins	WMC	0.000041	0.000040	1.031
P7	inc3	del	SEM	0.012515	0.010388	1.205
P7	inc3	del	PRN	0.007836	0.005986	1.309
P7	inc3	del	FC	0.004927	0.007115	0.692
P7	inc3	del	WMC	0.000040	0.000048	0.835
P7	inc3	ins	SEM	0.013163	0.004461	2.950
P7	inc3	ins	PRN	0.009271	0.023850	0.389
P7	inc3	ins	FC	0.006746	0.016573	0.407
P7	inc3	ins	WMC	0.000044	0.000060	0.739
P7	inc5	del	SEM	0.012111	0.010723	1.129
P7	inc5	del	PRN	0.007512	0.006454	1.164
P7	inc5	del	FC	0.005179	0.006137	0.844
P7	inc5	del	WMC	0.000040	0.000053	0.740
P7	inc5	ins	SEM	0.012441	0.005477	2.272
P7	inc5	ins	PRN	0.010049	0.024449	0.411
P7	inc5	ins	FC	0.007202	0.012829	0.561
P7	inc5	ins	WMC	0.000045	0.000054	0.827
P8	inc1	del	SEM	0.054243	0.007734	7.013
P8	inc1	del	PRN	0.032484	0.018493	1.757
P8	inc1	del	FC	0.029774	0.013045	2.282
P8	inc1	del	WMC	0.000057	0.000053	1.079
P8	inc1	ins	SEM	0.055015	0.007548	7.289
P8	inc1	ins	PRN	0.033381	0.017790	1.876
P8	inc1	ins	FC	0.030508	0.012751	2.393
P8	inc1	ins	WMC	0.000059	0.000053	1.103
P8	inc3	del	SEM	0.044966	0.019697	2.283
P8	inc3	del	PRN	0.027718	0.021805	1.271
P8	inc3	del	FC	0.024921	0.018974	1.313
P8	inc3	del	WMC	0.000056	0.000097	0.577
P8	inc3	ins	SEM	0.054737	0.013348	4.101
P8	inc3	ins	PRN	0.032837	0.138414	0.237
P8	inc3	ins	FC	0.029462	0.054123	0.544
P8	inc3	ins	WMC	0.000057	0.000064	0.888
P8	inc5	del	SEM	0.038962	0.022632	1.722
P8	inc5	del	PRN	0.000915	0.012736	0.072
P8	inc5	del	FC	0.000126	0.008138	0.015
P8	inc5	del	WMC	0.000030	0.000039	0.773
P8	inc5	ins	SEM	0.052885	0.012862	4.112
P8	inc5	ins	PRN	0.033429	1.048929	0.032
P8	inc5	ins	FC	0.030676	0.225543	0.136
P8	inc5	ins	WMC	0.000057	0.000145	0.391
P9	inc1	del	SEM	0.027127	0.004078	6.651
P9	inc1	del	PRN	0.014695	0.006651	2.209
P9	inc1	del	FC	0.010639	0.006289	1.692
P9	inc1	del	WMC	0.000050	0.000058	0.868
P9	inc1	ins	SEM	0.027140	0.002904	9.347
P9	inc1	ins	PRN	0.015002	0.007548	1.988
P9	inc1	ins	FC	0.011692	0.009729	1.202
P9	inc1	ins	WMC	0.000060	0.000123	0.488
P9	inc3	del	SEM	0.022686	0.019069	1.190
P9	inc3	del	PRN	0.014667	0.011068	1.325
P9	inc3	del	FC	0.008449	0.008407	1.005
P9	inc3	del	WMC	0.000043	0.000048	0.891
P9	inc3	ins	SEM	0.027475	0.007379	3.723
P9	inc3	ins	PRN	0.015385	0.041623	0.370
P9	inc3	ins	FC	0.011672	0.019194	0.608
P9	inc3	ins	WMC	0.000059	0.000063	0.939
P9	inc5	del	SEM	0.022454	0.019336	1.161
P9	inc5	del	PRN	0.012539	0.011639	1.077
P9	inc5	del	FC	0.009082	0.008368	1.085
P9	inc5	del	WMC	0.000047	0.000051	0.937
P9	inc5	ins	SEM	0.029369	0.008551	3.435
P9	inc5	ins	PRN	0.015975	0.042263	0.378
P9	inc5	ins	FC	0.011717	0.020832	0.562
P9	inc5	ins	WMC	0.000063	0.000061	1.027
P10	inc1	del	SEM	0.167250	0.057061	2.931
P10	inc1	del	PRN	0.009828	0.026542	0.370
P10	inc1	del	FC	0.000784	0.001781	0.440
P10	inc1	del	WMC	0.000039	0.000043	0.911
P10	inc1	ins	SEM	0.172988	0.015545	11.129
P10	inc1	ins	PRN	0.009987	0.024929	0.401
P10	inc1	ins	FC	0.000888	0.001510	0.588
P10	inc1	ins	WMC	0.000040	0.000052	0.766
P10	inc3	del	SEM	0.145143	0.051725	2.806
P10	inc3	del	PRN	0.006648	0.031369	0.212
P10	inc3	del	FC	0.000839	0.001957	0.429
P10	inc3	del	WMC	0.000040	0.000043	0.918
P10	inc3	ins	SEM	0.152614	0.024758	6.164
P10	inc3	ins	PRN	0.008095	0.032207	0.251
P10	inc3	ins	FC	0.000868	0.005790	0.150
P10	inc3	ins	WMC	0.000039	0.000039	1.003
P10	inc5	del	SEM	0.149307	0.053402	2.796
P10	inc5	del	PRN	0.006935	0.039365	0.176
P10	inc5	del	FC	0.000511	0.001899	0.269
P10	inc5	del	WMC	0.000037	0.000044	0.837
P10	inc5	ins	SEM	0.152724	0.032194	4.744
P10	inc5	ins	PRN	0.007135	0.050453	0.141
P10	inc5	ins	FC	0.000897	0.008425	0.106
P10	inc5	ins	WMC	0.000039	0.000040	0.984
P11	inc1	del	SEM	0.137996	0.042612	3.238
P11	inc1	del	PRN	0.004689	0.020278	0.231
P11	inc1	del	FC	0.000224	0.000127	1.762
P11	inc1	del	WMC	0.000030	0.000034	0.878
P11	inc1	ins	SEM	0.151510	0.015008	10.095
P11	inc1	ins	PRN	0.005978	0.021012	0.285
P11	inc1	ins	FC	0.000239	0.000102	2.344
P11	inc1	ins	WMC	0.000032	0.000056	0.567
P11	inc3	del	SEM	0.140501	0.043888	3.201
P11	inc3	del	PRN	0.004949	0.026099	0.190
P11	inc3	del	FC	0.000216	0.001136	0.190
P11	inc3	del	WMC	0.000030	0.000034	0.904
P11	inc3	ins	SEM	0.151708	0.023333	6.502
P11	inc3	ins	PRN	0.005449	0.035633	0.153
P11	inc3	ins	FC	0.000242	0.000270	0.896
P11	inc3	ins	WMC	0.000031	0.000038	0.819
P11	inc5	del	SEM	0.120800	0.049901	2.421
P11	inc5	del	PRN	0.003378	0.032099	0.105
P11	inc5	del	FC	0.000214	0.001310	0.163
P11	inc5	del	WMC	0.000030	0.000032	0.932
P11	inc5	ins	SEM	0.143529	0.027798	5.163
P11	inc5	ins	PRN	0.005270	0.039387	0.134
P11	inc5	ins	FC	0.000235	0.004960	0.047
P11	inc5	ins	WMC	0.000030	0.000031	0.976
P12	inc1	del	SEM	0.731445	0.135671	5.391
P12	inc1	del	PRN	0.145979	0.196444	0.743
P12	inc1	del	FC	0.096753	0.069386	1.394
P12	inc1	del	WMC	0.000216	0.000360	0.598
P12	inc1	ins	SEM	0.872891	0.058799	14.845
P12	inc1	ins	PRN	0.193853	0.908953	0.213
P12	inc1	ins	FC	0.144939	0.400012	0.362
P12	inc1	ins	WMC	0.000437	0.000688	0.635
P12	inc3	del	SEM	0.676968	0.273221	2.478
P12	inc3	del	PRN	0.056159	0.261970	0.214
P12	inc3	del	FC	0.015634	0.090387	0.173
P12	inc3	del	WMC	0.000145	0.000392	0.371
P12	inc3	ins	SEM	0.830800	0.160351	5.181
P12	inc3	ins	PRN	0.200795	3.676396	0.055
P12	inc3	ins	FC	0.133020	0.893183	0.149
P12	inc3	ins	WMC	0.000330	0.000608	0.543
P12	inc5	del	SEM	0.597505	0.353981	1.688
P12	inc5	del	PRN	0.050865	0.358512	0.142
P12	inc5	del	FC	0.015226	0.095501	0.159
P12	inc5	del	WMC	0.000144	0.000228	0.632
P12	inc5	ins	SEM	0.753761	0.255912	2.945
P12	inc5	ins	PRN	0.190889	3.765238	0.051
P12	inc5	ins	FC	0.134192	0.897322	0.150
P12	inc5	ins	WMC	0.000297	0.000772	0.385
P13	inc1	del	SEM	1.655599	0.211352	7.833
P13	inc1	del	PRN	0.226398	0.274869	0.824
P13	inc1	del	FC	0.496145	0.094021	5.277
P13	inc1	del	WMC	0.001061	0.000901	1.177
P13	inc1	ins	SEM	1.903420	0.085241	22.330
P13	inc1	ins	PRN	0.253572	0.287357	0.882
P13	inc1	ins	FC	0.351663	0.827515	0.425
P13	inc1	ins	WMC	0.001327	0.001476	0.899
P13	inc3	del	SEM	1.398513	0.708246	1.975
P13	inc3	del	PRN	0.164440	0.617094	0.266
P13	inc3	del	FC	0.081689	0.100588	0.812
P13	inc3	del	WMC	0.000352	0.000780	0.451
P13	inc3	ins	SEM	1.877691	0.298560	6.289
P13	inc3	ins	PRN	0.241931	1.240569	0.195
P13	inc3	ins	FC	0.352025	0.814578	0.432
P13	inc3	ins	WMC	0.001197	0.001398	0.856
P13	inc5	del	SEM	1.145930	0.836851	1.369
P13	inc5	del	PRN	0.140600	0.794288	0.177
P13	inc5	del	FC	0.070941	0.110608	0.641
P13	inc5	del	WMC	0.000183	0.000492	0.373
P13	inc5	ins	SEM	1.817206	0.569569	3.190
P13	inc5	ins	PRN	0.247157	1.662686	0.149
P13	inc5	ins	FC	0.357407	0.782582	0.457
P13	inc5	ins	WMC	0.001356	0.001535	0.883
P14	inc1	del	SEM	2.649057	0.463945	5.710
P14	inc1	del	PRN	0.229747	0.413588	0.555
P14	inc1	del	FC	0.438554	0.101842	4.306
P14	inc1	del	WMC	0.001145	0.001468	0.780
P14	inc1	ins	SEM	2.972744	0.193334	15.376
P14	inc1	ins	PRN	0.334559	1.853697	0.180
P14	inc1	ins	FC	0.713965	1.089166	0.656
P14	inc1	ins	WMC	0.001994	0.002255	0.884
P14	inc3	del	SEM	2.188151	0.999486	2.189
P14	inc3	del	PRN	0.190253	0.889417	0.214
P14	inc3	del	FC	0.064195	0.125973	0.510
P14	inc3	del	WMC	0.000274	0.000943	0.290
P14	inc3	ins	SEM	2.991978	0.618763	4.835
P14	inc3	ins	PRN	0.342124	2.620715	0.131
P14	inc3	ins	FC	0.693571	0.939938	0.738
P14	inc3	ins	WMC	0.001651	0.002139	0.772
P14	inc5	del	SEM	2.075539	1.458627	1.423
P14	inc5	del	PRN	0.142391	1.195119	0.119
P14	inc5	del	FC	0.039733	0.082667	0.481
P14	inc5	del	WMC	0.000231	0.000450	0.515
P14	inc5	ins	SEM	2.981700	0.938501	3.177
P14	inc5	ins	PRN	0.329946	3.609158	0.091
P14	inc5	ins	FC	0.727687	1.065345	0.683
P14	inc5	ins	WMC	0.002116	0.002334	0.906
P15	inc1	del	SEM	6.986261	1.585136	4.407
P15	inc1	del	PRN	0.490746	0.955204	0.514
P15	inc1	del	FC	0.905954	0.207364	4.369
P15	inc1	del	WMC	0.001647	0.002464	0.668
P15	inc1	ins	SEM	7.435397	0.482602	15.407
P15	inc1	ins	PRN	0.631630	2.872320	0.220
P15	inc1	ins	FC	2.408297	1.975453	1.219
P15	inc1	ins	WMC	0.004651	0.005025	0.926
P15	inc3	del	SEM	5.891017	2.974583	1.980
P15	inc3	del	PRN	0.288169	1.905459	0.151
P15	inc3	del	FC	0.023034	0.145963	0.158
P15	inc3	del	WMC	0.000340	0.000634	0.536
P15	inc3	ins	SEM	7.493096	1.523228	4.919
P15	inc3	ins	PRN	0.674577	6.432035	0.105
P15	inc3	ins	FC	2.400981	2.239904	1.072
P15	inc3	ins	WMC	0.004472	0.005519	0.810
P15	inc5	del	SEM	4.950324	3.475464	1.424
P15	inc5	del	PRN	0.252453	2.568903	0.098
P15	inc5	del	FC	0.018454	0.152134	0.121
P15	inc5	del	WMC	0.000316	0.000473	0.667
P15	inc5	ins	SEM	7.403819	2.197909	3.369
P15	inc5	ins	PRN	0.663303	7.295310	0.091
P15	inc5	ins	FC	2.390035	2.933782	0.815
P15	inc5	ins	WMC	0.003986	0.006476	0.615
P16	inc1	del	SEM	10.975435	4.515618	2.431
P16	inc1	del	PRN	0.699559	1.622124	0.431
P16	inc1	del	FC	1.539313	0.491509	3.132
P16	inc1	del	WMC	0.003001	0.004396	0.683
P16	inc1	ins	SEM	11.465530	0.834887	13.733
P16	inc1	ins	PRN	1.049156	3.766347	0.279
P16	inc1	ins	FC	3.989139	4.211829	0.947
P16	inc1	ins	WMC	0.008413	0.008987	0.936
P16	inc3	del	SEM	9.387051	6.153769	1.525
P16	inc3	del	PRN	0.533062	3.545029	0.150
P16	inc3	del	FC	0.093480	0.370687	0.252
P16	inc3	del	WMC	0.000651	0.001079	0.603
P16	inc3	ins	SEM	11.225025	2.324650	4.829
P16	inc3	ins	PRN	1.111017	7.441712	0.149
P16	inc3	ins	FC	4.002833	4.075054	0.982
P16	inc3	ins	WMC	0.008474	0.011894	0.712
P16	inc5	del	SEM	8.468580	7.019519	1.206
P16	inc5	del	PRN	0.435963	4.643663	0.094
P16	inc5	del	FC	0.051117	0.346785	0.147
P16	inc5	del	WMC	0.000566	0.001011	0.559
P16	inc5	ins	SEM	11.316014	3.699514	3.059
P16	inc5	ins	PRN	1.049687	10.747929	0.098
P16	inc5	ins	FC	3.949415	4.241539	0.931
P16	inc5	ins	WMC	0.007685	0.010215	0.752
P17	inc1	del	SEM	16.138063	7.795415	2.070
P17	inc1	del	PRN	0.841384	2.075817	0.405
P17	inc1	del	FC	1.663688	0.697266	2.386
P17	inc1	del	WMC	0.002773	0.003828	0.725
P17	inc1	ins	SEM	16.713840	1.049018	15.933
P17	inc1	ins	PRN	1.530934	8.335932	0.184
P17	inc1	ins	FC	5.103743	7.761294	0.658
P17	inc1	ins	WMC	0.012738	0.017394	0.732
P17	inc3	del	SEM	13.857574	10.087168	1.374
P17	inc3	del	PRN	0.655642	4.484519	0.146
P17	inc3	del	FC	0.058198	0.537933	0.108
P17	inc3	del	WMC	0.000967	0.001605	0.602
P17	inc3	ins	SEM	16.686241	3.200940	5.213
P17	inc3	ins	PRN	1.400418	13.636774	0.103
P17	inc3	ins	FC	5.015177	7.640902	0.656
P17	inc3	ins	WMC	0.012339	0.017823	0.692
P17	inc5	del	SEM	11.680647	11.556862	1.011
P17	inc5	del	PRN	0.597542	6.823066	0.088
P17	inc5	del	FC	0.045925	0.542056	0.085
P17	inc5	del	WMC	0.000891	0.001241	0.718
P17	inc5	ins	SEM	16.689915	5.373781	3.106
P17	inc5	ins	PRN	1.451857	16.354883	0.089
P17	inc5	ins	FC	5.038975	7.498621	0.672
P17	inc5	ins	WMC	0.013021	0.018242	0.714
P18	inc1	del	SEM	18.976903	13.436108	1.412
P18	inc1	del	PRN	1.080570	3.324535	0.325
P18	inc1	del	FC	1.565117	0.937248	1.670
P18	inc1	del	WMC	0.002943	0.003832	0.768
P18	inc1	ins	SEM	21.210024	2.001859	10.595
P18	inc1	ins	PRN	1.831063	12.102001	0.151
P18	inc1	ins	FC	6.165108	4.883320	1.262
P18	inc1	ins	WMC	0.021030	0.033452	0.629
P18	inc3	del	SEM	17.154974	14.777614	1.161
P18	inc3	del	PRN	0.812582	6.630425	0.123
P18	inc3	del	FC	0.056087	0.785400	0.071
P18	inc3	del	WMC	0.001326	0.001645	0.806
P18	inc3	ins	SEM	21.170891	5.100546	4.151
P18	inc3	ins	PRN	2.068435	20.498193	0.101
P18	inc3	ins	FC	6.318911	4.430526	1.426
P18	inc3	ins	WMC	0.019142	0.027296	0.701
P18	inc5	del	SEM	15.258193	16.056035	0.950
P18	inc5	del	PRN	0.737913	9.541551	0.077
P18	inc5	del	FC	0.015912	0.765683	0.021
P18	inc5	del	WMC	0.001069	0.001156	0.925
P18	inc5	ins	SEM	20.576998	7.380478	2.788
P18	inc5	ins	PRN	1.853081	24.391576	0.076
P18	inc5	ins	FC	6.142754	4.758892	1.291
P18	inc5	ins	WMC	0.021636	0.024774	0.873
P19	inc1	del	SEM	24.409247	20.202007	1.208
P19	inc1	del	PRN	1.385906	3.941049	0.352
P19	inc1	del	FC	1.651545	1.277386	1.293
P19	inc1	del	WMC	0.003828	0.005251	0.729
P19	inc1	ins	SEM	28.737163	2.183931	13.158
P19	inc1	ins	PRN	2.479310	18.789670	0.132
P19	inc1	ins	FC	8.195748	5.459357	1.501
P19	inc1	ins	WMC	0.027279	0.030283	0.901
P19	inc3	del	SEM	22.027390	21.829340	1.009
P19	inc3	del	PRN	1.108596	8.088786	0.137
P19	inc3	del	FC	0.106113	1.126427	0.094
P19	inc3	del	WMC	0.001374	0.002102	0.654
P19	inc3	ins	SEM	29.223146	5.900015	4.953
P19	inc3	ins	PRN	2.516055	29.219920	0.086
P19	inc3	ins	FC	8.181255	6.799188	1.203
P19	inc3	ins	WMC	0.027218	0.042528	0.640
P19	inc5	del	SEM	19.440257	23.456679	0.829
P19	inc5	del	PRN	0.980592	12.456281	0.079
P19	inc5	del	FC	0.101951	1.156558	0.088
P19	inc5	del	WMC	0.001355	0.001754	0.772
P19	inc5	ins	SEM	28.950370	9.471341	3.057
P19	inc5	ins	PRN	2.434148	33.529468	0.073
P19	inc5	ins	FC	8.703763	6.197246	1.404
P19	inc5	ins	WMC	0.026790	0.034620	0.774
P20	inc1	del	SEM	17.824630	7.049249	2.529
P20	inc1	del	PRN	1.987636	3.117125	0.638
P20	inc1	del	FC	2.600869	1.949388	1.334
P20	inc1	del	WMC	0.012314	0.012005	1.026
P20	inc1	ins	SEM	18.830822	0.949854	19.825
P20	inc1	ins	PRN	2.464124	8.474288	0.291
P20	inc1	ins	FC	4.557335	4.308826	1.058
P20	inc1	ins	WMC	0.017015	0.016538	1.029
P20	inc3	del	SEM	16.806544	15.376363	1.093
P20	inc3	del	PRN	1.569239	4.950025	0.317
P20	inc3	del	FC	1.001882	2.275464	0.440
P20	inc3	del	WMC	0.006374	0.012005	0.531
P20	inc3	ins	SEM	19.055086	3.044875	6.258
P20	inc3	ins	PRN	2.468683	11.701473	0.211
P20	inc3	ins	FC	6.021844	13.149732	0.458
P20	inc3	ins	WMC	0.018505	0.077025	0.240
P20	inc5	del	SEM	15.467195	18.695150	0.827
P20	inc5	del	PRN	0.968819	6.675238	0.145
P20	inc5	del	FC	0.469338	2.117876	0.222
P20	inc5	del	WMC	0.004095	0.006336	0.646
P20	inc5	ins	SEM	18.974118	4.988900	3.803
P20	inc5	ins	PRN	2.577071	41.991174	0.061
P20	inc5	ins	FC	4.634768	40.840566	0.113
P20	inc5	ins	WMC	0.018302	0.042977	0.426
```

#### 2026-01-09 (pre-fix run)
```tsv
Case	Delta	Turn	Stage	Full_s	Inc_s	Speedup
P1	inc1	del	SEM	0.036916	0.107275	0.344
P1	inc1	del	PRN	0.001881	0.110591	0.017
P1	inc1	del	FC	0.000432	0.000298	1.448
P1	inc1	del	WMC	0.000024	0.000032	0.741
P1	inc1	ins	SEM	0.146112	0.087999	1.660
P1	inc1	ins	PRN	0.004293	0.116420	0.037
P1	inc1	ins	FC	0.000430	0.000286	1.502
P1	inc1	ins	WMC	0.000025	0.000030	0.819
P1	inc3	del	SEM	0.027843	0.140441	0.198
P1	inc3	del	PRN	0.001665	0.163259	0.010
P1	inc3	del	FC	0.000455	0.000331	1.373
P1	inc3	del	WMC	0.000025	0.000068	0.375
P1	inc3	ins	SEM	0.166298	0.146420	1.136
P1	inc3	ins	PRN	0.006219	0.133987	0.046
P1	inc3	ins	FC	0.000468	0.000254	1.841
P1	inc3	ins	WMC	0.000027	0.000030	0.895
P1	inc5	del	SEM	0.028716	0.117594	0.244
P1	inc5	del	PRN	0.001230	0.123589	0.010
P1	inc5	del	FC	0.000260	0.000254	1.027
P1	inc5	del	WMC	0.000024	0.000027	0.881
P1	inc5	ins	SEM	0.156379	0.114796	1.362
P1	inc5	ins	PRN	0.004872	0.138778	0.035
P1	inc5	ins	FC	0.000443	0.001402	0.316
P1	inc5	ins	WMC	0.000026	0.000037	0.682
P3	inc1	del	SEM	0.048905	0.097985	0.499
P3	inc1	del	PRN	0.002098	0.124664	0.017
P3	inc1	del	FC	0.000419	0.000326	1.288
P3	inc1	del	WMC	0.000024	0.000035	0.693
P3	inc1	ins	SEM	0.141773	0.092253	1.537
P3	inc1	ins	PRN	0.004461	0.131177	0.034
P3	inc1	ins	FC	0.000448	0.000247	1.815
P3	inc1	ins	WMC	0.000025	0.000031	0.799
P3	inc3	del	SEM	0.048009	0.088201	0.544
P3	inc3	del	PRN	0.002084	0.096404	0.022
P3	inc3	del	FC	0.000432	0.000291	1.485
P3	inc3	del	WMC	0.000024	0.000032	0.748
P3	inc3	ins	SEM	0.150190	0.072500	2.072
P3	inc3	ins	PRN	0.004457	0.100844	0.044
P3	inc3	ins	FC	0.000437	0.000249	1.753
P3	inc3	ins	WMC	0.000025	0.000030	0.823
P3	inc5	del	SEM	0.025540	0.098974	0.258
P3	inc5	del	PRN	0.001577	0.117308	0.013
P3	inc5	del	FC	0.000439	0.000293	1.500
P3	inc5	del	WMC	0.000024	0.000032	0.769
P3	inc5	ins	SEM	0.159499	0.117883	1.353
P3	inc5	ins	PRN	0.005659	0.164969	0.034
P3	inc5	ins	FC	0.000567	0.000270	2.098
P3	inc5	ins	WMC	0.000033	0.000037	0.872
P4	inc1	del	SEM	0.000921	0.000961	0.958
P4	inc1	del	PRN	0.000134	0.000147	0.909
P4	inc1	del	FC	0.000087	0.000037	2.348
P4	inc1	del	WMC	0.000022	0.000023	0.931
P4	inc1	ins	SEM	0.000884	0.000866	1.021
P4	inc1	ins	PRN	0.000126	0.000119	1.058
P4	inc1	ins	FC	0.000098	0.000033	2.947
P4	inc1	ins	WMC	0.000022	0.000022	0.994
P4	inc3	del	SEM	0.000903	0.000894	1.010
P4	inc3	del	PRN	0.000125	0.000126	0.996
P4	inc3	del	FC	0.000091	0.000037	2.459
P4	inc3	del	WMC	0.000022	0.000022	0.986
P4	inc3	ins	SEM	0.001640	0.000787	2.085
P4	inc3	ins	PRN	0.000112	0.000106	1.057
P4	inc3	ins	FC	0.000086	0.000033	2.600
P4	inc3	ins	WMC	0.000021	0.000022	0.979
P4	inc5	del	SEM	0.000886	0.001143	0.775
P4	inc5	del	PRN	0.000124	0.000133	0.930
P4	inc5	del	FC	0.000088	0.000034	2.574
P4	inc5	del	WMC	0.000022	0.000022	0.962
P4	inc5	ins	SEM	0.000891	0.001432	0.622
P4	inc5	ins	PRN	0.000124	0.000129	0.961
P4	inc5	ins	FC	0.000088	0.000033	2.677
P4	inc5	ins	WMC	0.000022	0.000022	1.000
P5	inc1	del	SEM	0.000258	0.001385	0.186
P5	inc1	del	PRN	0.000098	0.000126	0.776
P5	inc1	del	FC	0.000087	0.000717	0.121
P5	inc1	del	WMC	0.000022	0.000023	0.960
P5	inc1	ins	SEM	0.000278	0.002362	0.118
P5	inc1	ins	PRN	0.000287	0.000520	0.551
P5	inc1	ins	FC	0.000194	0.000495	0.393
P5	inc1	ins	WMC	0.000023	0.000025	0.948
P5	inc3	del	SEM	0.000261	0.001015	0.257
P5	inc3	del	PRN	0.000095	0.000128	0.741
P5	inc3	del	FC	0.000077	0.000784	0.098
P5	inc3	del	WMC	0.000022	0.000025	0.904
P5	inc3	ins	SEM	0.000275	0.001243	0.221
P5	inc3	ins	PRN	0.000260	0.000396	0.657
P5	inc3	ins	FC	0.000198	0.000394	0.503
P5	inc3	ins	WMC	0.000024	0.000025	0.966
P5	inc5	del	SEM	0.000259	0.001801	0.144
P5	inc5	del	PRN	0.000104	0.000128	0.815
P5	inc5	del	FC	0.000077	0.000819	0.094
P5	inc5	del	WMC	0.000022	0.000029	0.767
P5	inc5	ins	SEM	0.000280	0.001702	0.164
P5	inc5	ins	PRN	0.000278	0.000395	0.705
P5	inc5	ins	FC	0.000214	0.000375	0.571
P5	inc5	ins	WMC	0.000024	0.000025	0.952
P6	inc1	del	SEM	0.003643	0.003390	1.075
P6	inc1	del	PRN	0.003013	0.001313	2.295
P6	inc1	del	FC	0.001827	0.001738	1.052
P6	inc1	del	WMC	0.000028	0.000030	0.942
P6	inc1	ins	SEM	0.004171	0.003336	1.250
P6	inc1	ins	PRN	0.003008	0.001850	1.626
P6	inc1	ins	FC	0.001967	0.001592	1.236
P6	inc1	ins	WMC	0.000031	0.000031	1.022
P6	inc3	del	SEM	0.002740	0.005383	0.509
P6	inc3	del	PRN	0.001450	0.001707	0.850
P6	inc3	del	FC	0.000749	0.001297	0.577
P6	inc3	del	WMC	0.000046	0.000028	1.615
P6	inc3	ins	SEM	0.003907	0.003790	1.031
P6	inc3	ins	PRN	0.003184	0.011571	0.275
P6	inc3	ins	FC	0.002064	0.008875	0.233
P6	inc3	ins	WMC	0.000031	0.000031	0.977
P6	inc5	del	SEM	0.002669	0.004438	0.601
P6	inc5	del	PRN	0.001463	0.001440	1.016
P6	inc5	del	FC	0.000685	0.001348	0.508
P6	inc5	del	WMC	0.000028	0.000028	0.972
P6	inc5	ins	SEM	0.004169	0.002740	1.522
P6	inc5	ins	PRN	0.002946	0.011529	0.256
P6	inc5	ins	FC	0.002119	0.009233	0.230
P6	inc5	ins	WMC	0.000038	0.000039	0.987
P7	inc1	del	SEM	0.013023	0.004702	2.770
P7	inc1	del	PRN	0.009266	0.003851	2.406
P7	inc1	del	FC	0.006582	0.003731	1.764
P7	inc1	del	WMC	0.000039	0.000037	1.047
P7	inc1	ins	SEM	0.013293	0.002075	6.405
P7	inc1	ins	PRN	0.008465	0.003608	2.346
P7	inc1	ins	FC	0.006555	0.003008	2.179
P7	inc1	ins	WMC	0.000040	0.000036	1.094
P7	inc3	del	SEM	0.011901	0.004278	2.782
P7	inc3	del	PRN	0.008918	0.005354	1.666
P7	inc3	del	FC	0.006562	0.003965	1.655
P7	inc3	del	WMC	0.000038	0.000039	0.973
P7	inc3	ins	SEM	0.011909	0.002934	4.059
P7	inc3	ins	PRN	0.008756	0.004578	1.913
P7	inc3	ins	FC	0.006485	0.003135	2.068
P7	inc3	ins	WMC	0.000039	0.000036	1.082
P7	inc5	del	SEM	0.011736	0.004673	2.512
P7	inc5	del	PRN	0.008845	0.005315	1.664
P7	inc5	del	FC	0.006250	0.003878	1.611
P7	inc5	del	WMC	0.000037	0.000036	1.037
P7	inc5	ins	SEM	0.011901	0.003143	3.787
P7	inc5	ins	PRN	0.008424	0.004831	1.744
P7	inc5	ins	FC	0.006142	0.003257	1.886
P7	inc5	ins	WMC	0.000039	0.000046	0.851
P8	inc1	del	SEM	0.048436	0.002659	18.213
P8	inc1	del	PRN	0.030608	0.013638	2.244
P8	inc1	del	FC	0.027437	0.011120	2.467
P8	inc1	del	WMC	0.000048	0.000048	0.988
P8	inc1	ins	SEM	0.049501	0.002519	19.653
P8	inc1	ins	PRN	0.032639	0.013502	2.417
P8	inc1	ins	FC	0.029863	0.010907	2.738
P8	inc1	ins	WMC	0.000069	0.000048	1.431
P8	inc3	del	SEM	0.045830	0.004410	10.392
P8	inc3	del	PRN	0.030744	0.018604	1.653
P8	inc3	del	FC	0.028764	0.012391	2.321
P8	inc3	del	WMC	0.000047	0.000042	1.121
P8	inc3	ins	SEM	0.048340	0.005250	9.208
P8	inc3	ins	PRN	0.031380	0.018211	1.723
P8	inc3	ins	FC	0.028752	0.013169	2.183
P8	inc3	ins	WMC	0.000053	0.000070	0.754
P8	inc5	del	SEM	0.043373	0.012522	3.464
P8	inc5	del	PRN	0.024957	0.018376	1.358
P8	inc5	del	FC	0.021289	0.010051	2.118
P8	inc5	del	WMC	0.000042	0.000041	1.012
P8	inc5	ins	SEM	0.048339	0.007430	6.505
P8	inc5	ins	PRN	0.030377	0.109237	0.278
P8	inc5	ins	FC	0.027974	0.042521	0.658
P8	inc5	ins	WMC	0.000055	0.000058	0.946
P9	inc1	del	SEM	0.023308	0.009787	2.382
P9	inc1	del	PRN	0.013647	0.007858	1.737
P9	inc1	del	FC	0.010157	0.006077	1.672
P9	inc1	del	WMC	0.000048	0.000046	1.060
P9	inc1	ins	SEM	0.025373	0.003356	7.560
P9	inc1	ins	PRN	0.014119	0.007649	1.846
P9	inc1	ins	FC	0.010771	0.010580	1.018
P9	inc1	ins	WMC	0.000055	0.000074	0.744
P9	inc3	del	SEM	0.020228	0.010211	1.981
P9	inc3	del	PRN	0.013055	0.010500	1.243
P9	inc3	del	FC	0.009874	0.005720	1.726
P9	inc3	del	WMC	0.000043	0.000041	1.053
P9	inc3	ins	SEM	0.025115	0.006206	4.047
P9	inc3	ins	PRN	0.014612	0.011557	1.264
P9	inc3	ins	FC	0.011165	0.008706	1.283
P9	inc3	ins	WMC	0.000053	0.000070	0.762
P9	inc5	del	SEM	0.020036	0.011923	1.680
P9	inc5	del	PRN	0.012995	0.010170	1.278
P9	inc5	del	FC	0.010354	0.005513	1.878
P9	inc5	del	WMC	0.000045	0.000037	1.212
P9	inc5	ins	SEM	0.025344	0.006571	3.857
P9	inc5	ins	PRN	0.014708	0.014068	1.045
P9	inc5	ins	FC	0.011296	0.009891	1.142
P9	inc5	ins	WMC	0.000061	0.000083	0.731
P10	inc1	del	SEM	0.132803	0.048911	2.715
P10	inc1	del	PRN	0.006191	0.013722	0.451
P10	inc1	del	FC	0.000749	0.001393	0.538
P10	inc1	del	WMC	0.000035	0.000037	0.941
P10	inc1	ins	SEM	0.138227	0.011944	11.573
P10	inc1	ins	PRN	0.006139	0.014517	0.423
P10	inc1	ins	FC	0.000883	0.000937	0.942
P10	inc1	ins	WMC	0.000036	0.000035	1.029
P10	inc3	del	SEM	0.125004	0.033687	3.711
P10	inc3	del	PRN	0.005927	0.025436	0.233
P10	inc3	del	FC	0.000718	0.001452	0.494
P10	inc3	del	WMC	0.000037	0.000037	1.001
P10	inc3	ins	SEM	0.159665	0.019541	8.171
P10	inc3	ins	PRN	0.009914	0.025927	0.382
P10	inc3	ins	FC	0.000904	0.001469	0.616
P10	inc3	ins	WMC	0.000040	0.000035	1.126
P10	inc5	del	SEM	0.121538	0.042503	2.860
P10	inc5	del	PRN	0.005263	0.033952	0.155
P10	inc5	del	FC	0.000668	0.001467	0.456
P10	inc5	del	WMC	0.000035	0.000039	0.900
P10	inc5	ins	SEM	0.138532	0.026602	5.208
P10	inc5	ins	PRN	0.006426	0.037870	0.170
P10	inc5	ins	FC	0.000847	0.001769	0.479
P10	inc5	ins	WMC	0.000037	0.000042	0.889
P11	inc1	del	SEM	0.152564	0.037789	4.037
P11	inc1	del	PRN	0.006231	0.011619	0.536
P11	inc1	del	FC	0.000214	0.000109	1.962
P11	inc1	del	WMC	0.000030	0.000042	0.702
P11	inc1	ins	SEM	0.145554	0.011635	12.510
P11	inc1	ins	PRN	0.005119	0.022081	0.232
P11	inc1	ins	FC	0.000212	0.000126	1.675
P11	inc1	ins	WMC	0.000030	0.000040	0.754
P11	inc3	del	SEM	0.125391	0.039399	3.183
P11	inc3	del	PRN	0.004548	0.026764	0.170
P11	inc3	del	FC	0.000261	0.000116	2.255
P11	inc3	del	WMC	0.000067	0.000039	1.701
P11	inc3	ins	SEM	0.133104	0.018303	7.272
P11	inc3	ins	PRN	0.003578	0.026395	0.136
P11	inc3	ins	FC	0.000208	0.000102	2.034
P11	inc3	ins	WMC	0.000028	0.000032	0.877
P11	inc5	del	SEM	0.118603	0.040901	2.900
P11	inc5	del	PRN	0.004125	0.028649	0.144
P11	inc5	del	FC	0.000209	0.000115	1.823
P11	inc5	del	WMC	0.000029	0.000032	0.914
P11	inc5	ins	SEM	0.136019	0.023475	5.794
P11	inc5	ins	PRN	0.004513	0.028662	0.157
P11	inc5	ins	FC	0.000218	0.000100	2.177
P11	inc5	ins	WMC	0.000029	0.000031	0.932
P12	inc1	del	SEM	0.706162	0.069146	10.213
P12	inc1	del	PRN	0.180070	0.149637	1.203
P12	inc1	del	FC	0.112910	0.053408	2.114
P12	inc1	del	WMC	0.000218	0.000203	1.074
P12	inc1	ins	SEM	0.706354	0.039483	17.890
P12	inc1	ins	PRN	0.161958	0.163607	0.990
P12	inc1	ins	FC	0.115098	0.226074	0.509
P12	inc1	ins	WMC	0.000313	0.000512	0.611
P12	inc3	del	SEM	0.580622	0.199154	2.915
P12	inc3	del	PRN	0.120223	0.258237	0.466
P12	inc3	del	FC	0.085617	0.054422	1.573
P12	inc3	del	WMC	0.000184	0.000244	0.753
P12	inc3	ins	SEM	0.755593	0.128586	5.876
P12	inc3	ins	PRN	0.179580	0.920395	0.195
P12	inc3	ins	FC	0.124589	0.415761	0.300
P12	inc3	ins	WMC	0.000342	0.000756	0.452
P12	inc5	del	SEM	0.507442	0.343902	1.476
P12	inc5	del	PRN	0.107783	0.381928	0.282
P12	inc5	del	FC	0.082839	0.057025	1.453
P12	inc5	del	WMC	0.000177	0.000173	1.022
P12	inc5	ins	SEM	0.818368	0.263307	3.108
P12	inc5	ins	PRN	0.181420	1.027531	0.177
P12	inc5	ins	FC	0.121804	0.410560	0.297
P12	inc5	ins	WMC	0.000294	0.000583	0.504
P13	inc1	del	SEM	1.376175	0.177471	7.754
P13	inc1	del	PRN	0.209882	0.264800	0.793
P13	inc1	del	FC	0.118220	0.083413	1.417
P13	inc1	del	WMC	0.000509	0.000813	0.626
P13	inc1	ins	SEM	1.640598	0.080342	20.420
P13	inc1	ins	PRN	0.247054	0.268913	0.919
P13	inc1	ins	FC	0.348457	0.748099	0.466
P13	inc1	ins	WMC	0.001340	0.001329	1.009
P13	inc3	del	SEM	1.355083	0.412714	3.283
P13	inc3	del	PRN	0.198196	0.419911	0.472
P13	inc3	del	FC	0.100041	0.086767	1.153
P13	inc3	del	WMC	0.000278	0.000475	0.587
P13	inc3	ins	SEM	1.595062	0.193344	8.250
P13	inc3	ins	PRN	0.247303	0.560871	0.441
P13	inc3	ins	FC	0.348970	0.785185	0.444
P13	inc3	ins	WMC	0.001123	0.001694	0.663
P13	inc5	del	SEM	1.078506	0.647298	1.666
P13	inc5	del	PRN	0.170888	0.570788	0.299
P13	inc5	del	FC	0.106741	0.104288	1.024
P13	inc5	del	WMC	0.000234	0.000557	0.420
P13	inc5	ins	SEM	1.573994	0.343302	4.585
P13	inc5	ins	PRN	0.231334	0.780910	0.296
P13	inc5	ins	FC	0.340908	0.597854	0.570
P13	inc5	ins	WMC	0.001159	0.001318	0.880
P14	inc1	del	SEM	2.392938	0.301984	7.924
P14	inc1	del	PRN	0.215407	0.343602	0.627
P14	inc1	del	FC	0.387862	0.079331	4.889
P14	inc1	del	WMC	0.001033	0.001008	1.025
P14	inc1	ins	SEM	2.715976	0.117581	23.099
P14	inc1	ins	PRN	0.329294	1.526004	0.216
P14	inc1	ins	FC	0.699357	1.144845	0.611
P14	inc1	ins	WMC	0.001656	0.002153	0.769
P14	inc3	del	SEM	1.971623	0.883591	2.231
P14	inc3	del	PRN	0.178619	0.597094	0.299
P14	inc3	del	FC	0.055217	0.069663	0.793
P14	inc3	del	WMC	0.000295	0.000782	0.377
P14	inc3	ins	SEM	2.842919	0.342532	8.300
P14	inc3	ins	PRN	0.307377	2.252709	0.136
P14	inc3	ins	FC	0.690042	0.921871	0.749
P14	inc3	ins	WMC	0.001683	0.002056	0.819
P14	inc5	del	SEM	1.680476	1.129641	1.488
P14	inc5	del	PRN	0.131941	0.917638	0.144
P14	inc5	del	FC	0.030098	0.057168	0.526
P14	inc5	del	WMC	0.000226	0.000442	0.512
P14	inc5	ins	SEM	2.713648	0.704194	3.854
P14	inc5	ins	PRN	0.286903	3.357879	0.085
P14	inc5	ins	FC	0.696758	1.139095	0.612
P14	inc5	ins	WMC	0.001694	0.002355	0.720
P15	inc1	del	SEM	6.273233	1.520492	4.126
P15	inc1	del	PRN	0.435408	0.995624	0.437
P15	inc1	del	FC	0.826746	0.197307	4.190
P15	inc1	del	WMC	0.001700	0.002664	0.638
P15	inc1	ins	SEM	6.872711	0.425052	16.169
P15	inc1	ins	PRN	0.575119	1.908556	0.301
P15	inc1	ins	FC	2.280432	2.085590	1.093
P15	inc1	ins	WMC	0.005589	0.005999	0.932
P15	inc3	del	SEM	5.400065	3.166347	1.705
P15	inc3	del	PRN	0.328415	1.857592	0.177
P15	inc3	del	FC	0.080458	0.144181	0.558
P15	inc3	del	WMC	0.000534	0.000905	0.590
P15	inc3	ins	SEM	6.997433	1.436146	4.872
P15	inc3	ins	PRN	0.589018	3.593563	0.164
P15	inc3	ins	FC	2.183685	1.935416	1.128
P15	inc3	ins	WMC	0.003759	0.005819	0.646
P15	inc5	del	SEM	4.450649	3.969988	1.121
P15	inc5	del	PRN	0.300337	2.621763	0.115
P15	inc5	del	FC	0.069302	0.144472	0.480
P15	inc5	del	WMC	0.000329	0.000695	0.474
P15	inc5	ins	SEM	6.703326	2.242605	2.989
P15	inc5	ins	PRN	0.616994	4.564118	0.135
P15	inc5	ins	FC	2.192157	1.960262	1.118
P15	inc5	ins	WMC	0.003659	0.005718	0.640
P16	inc1	del	SEM	9.711456	4.291374	2.263
P16	inc1	del	PRN	0.584370	1.400244	0.417
P16	inc1	del	FC	0.716000	0.313340	2.285
P16	inc1	del	WMC	0.002050	0.002878	0.713
P16	inc1	ins	SEM	10.817472	0.645041	16.770
P16	inc1	ins	PRN	0.917932	4.842083	0.190
P16	inc1	ins	FC	3.904436	4.819968	0.810
P16	inc1	ins	WMC	0.007676	0.011812	0.650
P16	inc3	del	SEM	8.573984	5.470151	1.567
P16	inc3	del	PRN	0.450589	2.791517	0.161
P16	inc3	del	FC	0.047553	0.240114	0.198
P16	inc3	del	WMC	0.000586	0.000983	0.596
P16	inc3	ins	SEM	10.703239	1.904506	5.620
P16	inc3	ins	PRN	0.954194	8.636341	0.110
P16	inc3	ins	FC	3.835040	6.326674	0.606
P16	inc3	ins	WMC	0.007555	0.024845	0.304
P16	inc5	del	SEM	7.482854	6.587379	1.136
P16	inc5	del	PRN	0.357360	4.142917	0.086
P16	inc5	del	FC	0.022690	0.227379	0.100
P16	inc5	del	WMC	0.000500	0.000872	0.574
P16	inc5	ins	SEM	10.516854	3.441247	3.056
P16	inc5	ins	PRN	0.963656	10.789722	0.089
P16	inc5	ins	FC	4.076262	4.746248	0.859
P16	inc5	ins	WMC	0.008252	0.011902	0.693
P17	inc1	del	SEM	14.759640	7.556252	1.953
P17	inc1	del	PRN	0.789784	2.067958	0.382
P17	inc1	del	FC	1.481357	0.536880	2.759
P17	inc1	del	WMC	0.002620	0.003321	0.789
P17	inc1	ins	SEM	15.657365	0.957138	16.359
P17	inc1	ins	PRN	1.316661	7.626537	0.173
P17	inc1	ins	FC	4.878439	9.104438	0.536
P17	inc1	ins	WMC	0.011332	0.018551	0.611
P17	inc3	del	SEM	12.956262	9.191922	1.410
P17	inc3	del	PRN	0.602789	4.069167	0.148
P17	inc3	del	FC	0.041900	0.407055	0.103
P17	inc3	del	WMC	0.000781	0.001588	0.492
P17	inc3	ins	SEM	15.656860	3.018116	5.188
P17	inc3	ins	PRN	1.357313	12.754430	0.106
P17	inc3	ins	FC	4.812331	8.234357	0.584
P17	inc3	ins	WMC	0.011447	0.024424	0.469
P17	inc5	del	SEM	11.574487	10.242238	1.130
P17	inc5	del	PRN	0.536938	6.098437	0.088
P17	inc5	del	FC	0.033334	0.379668	0.088
P17	inc5	del	WMC	0.000730	0.000998	0.731
P17	inc5	ins	SEM	16.134601	4.845710	3.330
P17	inc5	ins	PRN	1.519964	15.482508	0.098
P17	inc5	ins	FC	5.096244	7.109718	0.717
P17	inc5	ins	WMC	0.011968	0.019395	0.617
P18	inc1	del	SEM	18.454210	11.865074	1.555
P18	inc1	del	PRN	1.073524	2.643020	0.406
P18	inc1	del	FC	1.716826	0.706594	2.430
P18	inc1	del	WMC	0.002866	0.004171	0.687
P18	inc1	ins	SEM	20.306671	1.276674	15.906
P18	inc1	ins	PRN	1.933512	11.709643	0.165
P18	inc1	ins	FC	5.735151	4.120599	1.392
P18	inc1	ins	WMC	0.019568	0.020720	0.944
P18	inc3	del	SEM	16.183393	13.652050	1.185
P18	inc3	del	PRN	0.791877	5.553240	0.143
P18	inc3	del	FC	0.046186	0.645300	0.072
P18	inc3	del	WMC	0.001060	0.001653	0.641
P18	inc3	ins	SEM	19.669804	4.135282	4.757
P18	inc3	ins	PRN	2.034789	18.329526	0.111
P18	inc3	ins	FC	6.092775	4.134612	1.474
P18	inc3	ins	WMC	0.020893	0.025752	0.811
P18	inc5	del	SEM	14.690028	15.338593	0.958
P18	inc5	del	PRN	0.674955	8.298910	0.081
P18	inc5	del	FC	0.021409	0.613346	0.035
P18	inc5	del	WMC	0.000939	0.001249	0.752
P18	inc5	ins	SEM	19.821241	6.282177	3.155
P18	inc5	ins	PRN	1.651563	21.486107	0.077
P18	inc5	ins	FC	5.606612	4.337210	1.293
P18	inc5	ins	WMC	0.021032	0.020639	1.019
P19	inc1	del	SEM	23.004712	19.334280	1.190
P19	inc1	del	PRN	1.326849	3.847736	0.345
P19	inc1	del	FC	1.591398	1.147028	1.387
P19	inc1	del	WMC	0.003182	0.005445	0.584
P19	inc1	ins	SEM	27.851344	2.054866	13.554
P19	inc1	ins	PRN	2.374115	17.698753	0.134
P19	inc1	ins	FC	7.861757	5.886065	1.336
P19	inc1	ins	WMC	0.026029	0.038075	0.684
P19	inc3	del	SEM	20.971962	20.105154	1.043
P19	inc3	del	PRN	1.036829	7.138445	0.145
P19	inc3	del	FC	0.045277	0.907793	0.050
P19	inc3	del	WMC	0.001421	0.002104	0.675
P19	inc3	ins	SEM	27.365268	5.281558	5.181
P19	inc3	ins	PRN	2.338283	28.766281	0.081
P19	inc3	ins	FC	7.599500	6.382484	1.191
P19	inc3	ins	WMC	0.026272	0.060380	0.435
P19	inc5	del	SEM	18.709208	21.445900	0.872
P19	inc5	del	PRN	0.830907	10.407022	0.080
P19	inc5	del	FC	0.007060	0.908383	0.008
P19	inc5	del	WMC	0.001202	0.001486	0.809
P19	inc5	ins	SEM	27.096717	8.038793	3.371
P19	inc5	ins	PRN	2.437151	34.573206	0.070
P19	inc5	ins	FC	7.664833	5.348321	1.433
P19	inc5	ins	WMC	0.025820	0.036849	0.701
P20	inc1	del	SEM	17.438649	6.625350	2.632
P20	inc1	del	PRN	2.005902	2.858654	0.702
P20	inc1	del	FC	2.771887	1.805652	1.535
P20	inc1	del	WMC	0.012785	0.013082	0.977
P20	inc1	ins	SEM	18.201902	1.006348	18.087
P20	inc1	ins	PRN	2.403779	3.228741	0.744
P20	inc1	ins	FC	5.406622	3.919304	1.379
P20	inc1	ins	WMC	0.024990	0.015374	1.625
P20	inc3	del	SEM	16.014717	15.075597	1.062
P20	inc3	del	PRN	1.474861	4.531112	0.325
P20	inc3	del	FC	0.938678	2.050426	0.458
P20	inc3	del	WMC	0.005645	0.010999	0.513
P20	inc3	ins	SEM	18.287194	2.734843	6.687
P20	inc3	ins	PRN	2.357485	6.393569	0.369
P20	inc3	ins	FC	4.475122	7.800497	0.574
P20	inc3	ins	WMC	0.015949	0.020347	0.784
P20	inc5	del	SEM	15.035446	18.383152	0.818
P20	inc5	del	PRN	1.131045	5.965972	0.190
P20	inc5	del	FC	0.674798	1.959478	0.344
P20	inc5	del	WMC	0.004394	0.005278	0.833
P20	inc5	ins	SEM	18.059595	4.419234	4.087
P20	inc5	ins	PRN	2.372707	12.468033	0.190
P20	inc5	ins	FC	4.488667	8.463204	0.530
P20	inc5	ins	WMC	0.017387	0.019268	0.902
```

### SEM del/ins time (inc vs full, 2026-01-10 trimmed ruleset)
Speedup = Full_SEM_s / Inc_SEM_s (>1.0 means inc faster).
```tsv
Case	Delta	IncDelSem_s	FullDelSem_s	DelSpeedup	IncInsSem_s	FullInsSem_s	InsSpeedup
P1	inc1	0.001535	0.385245	250.984	0.001224	0.521563	426.080
P1	inc3	0.000876	0.298912	341.231	0.000835	0.377565	452.009
P1	inc5	0.151882	0.052206	0.344	0.139657	0.295593	2.117
P3	inc1	0.137226	0.085397	0.622	0.113269	0.295461	2.609
P3	inc3	0.129838	0.081318	0.626	0.120856	0.301446	2.494
P3	inc5	0.133459	0.078053	0.585	0.122928	0.289006	2.351
P4	inc1	0.001424	0.000267	0.188	0.000782	0.000285	0.365
P4	inc3	0.001536	0.000271	0.177	0.004219	0.000290	0.069
P4	inc5	0.000460	0.000275	0.599	0.000682	0.000288	0.422
P5	inc1	0.001921	0.000353	0.184	0.001445	0.000357	0.247
P5	inc3	0.001559	0.000321	0.206	0.001460	0.000486	0.333
P5	inc5	0.001764	0.000318	0.180	0.001349	0.000369	0.274
P6	inc1	0.001331	0.000577	0.434	0.001175	0.000511	0.435
P6	inc3	0.001073	0.000498	0.464	0.001593	0.000551	0.346
P6	inc5	0.001005	0.000500	0.497	0.001087	0.000567	0.522
P7	inc1	0.001729	0.001283	0.742	0.002042	0.001337	0.655
P7	inc3	0.002003	0.001144	0.571	0.002106	0.001355	0.643
P7	inc5	0.002202	0.001134	0.515	0.002157	0.001322	0.613
P8	inc1	0.002342	0.002143	0.915	0.002291	0.002062	0.900
P8	inc3	0.002270	0.001668	0.735	0.001243	0.002148	1.728
P8	inc5	0.001689	0.001627	0.963	0.002270	0.002301	1.014
P9	inc1	0.001792	0.001482	0.827	0.001506	0.001361	0.904
P9	inc3	0.001140	0.001557	1.366	0.002248	0.001411	0.628
P9	inc5	0.001722	0.001309	0.760	0.001698	0.001515	0.892
P10	inc1	0.007109	0.016497	2.321	0.007115	0.022307	3.135
P10	inc3	0.007572	0.011049	1.459	0.006946	0.027290	3.929
P10	inc5	0.009497	0.010862	1.144	0.008745	0.020957	2.396
P11	inc1	0.006090	0.014723	2.418	0.005739	0.020583	3.586
P11	inc3	0.006840	0.011051	1.616	0.007046	0.021368	3.033
P11	inc5	0.006432	0.010305	1.602	0.007001	0.024023	3.431
P12	inc1	0.003826	0.017927	4.686	0.003104	0.018963	6.108
P12	inc3	0.003982	0.015555	3.906	0.003466	0.018792	5.422
P12	inc5	0.005125	0.013238	2.583	0.004588	0.020125	4.386
P13	inc1	0.006497	0.031940	4.916	0.005295	0.038475	7.266
P13	inc3	0.009377	0.026386	2.814	0.009386	0.041025	4.371
P13	inc5	0.009931	0.024283	2.445	0.008919	0.044441	4.983
P14	inc1	0.009290	0.047071	5.067	0.008696	0.056581	6.507
P14	inc3	0.014381	0.036507	2.539	0.017245	0.067469	3.912
P14	inc5	0.015297	0.032569	2.129	0.015090	0.060942	4.039
P15	inc1	0.032168	0.113367	3.524	0.031465	0.147830	4.698
P15	inc3	0.037838	0.095963	2.536	0.041470	0.130419	3.145
P15	inc5	0.048718	0.109446	2.247	0.057390	0.158768	2.766
P16	inc1	0.057738	0.168664	2.921	0.057660	0.239183	4.148
P16	inc3	0.065251	0.151248	2.318	0.070245	0.240819	3.428
P16	inc5	0.076431	0.241312	3.157	0.075052	0.242772	3.235
P17	inc1	0.100536	0.230810	2.296	0.096283	0.396708	4.120
P17	inc3	0.111565	0.198731	1.781	0.107714	0.390036	3.621
P17	inc5	0.110010	0.194008	1.764	0.117621	0.374737	3.186
P18	inc1	0.130858	0.335190	2.561	0.153409	0.461087	3.006
P18	inc3	0.128429	0.285182	2.221	0.152332	0.455197	2.988
P18	inc5	0.119644	0.234277	1.958	0.156283	0.508392	3.253
P19	inc1	0.179769	0.472441	2.628	0.225704	0.706978	3.132
P19	inc3	0.177076	0.370329	2.091	0.220714	0.624964	2.832
P19	inc5	0.187138	0.345540	1.846	0.260906	0.608440	2.332
P20	inc1	0.092535	0.629298	6.801	0.099873	0.673522	6.744
P20	inc3	0.146947	0.461004	3.137	0.136890	0.728237	5.320
P20	inc5	0.150087	0.471920	3.144	0.149270	0.609385	4.082
```

### Pre-prune delta edge ratios (apply_delta_view / total edges, 2026-01-10 trimmed ruleset)
DeltaEdges come from apply_delta_view (pre-prune); total edges come from dumpStatisticsInc (full graph).
Delete ratios can exceed 1.0 when removed edges outnumber the remaining edges after the delete turn.
```tsv
Case	Delta	DelEdges	DelTotalEdges	DelEdgeRatio	InsEdges	InsTotalEdges	InsEdgeRatio
P1	inc1	0	88699	0.000000	0	88699	0.000000
P1	inc3	0	88699	0.000000	0	88699	0.000000
P1	inc5	73343	15356	4.776179	73343	88699	0.826875
P3	inc1	60097	28602	2.101147	60097	88699	0.677539
P3	inc3	60426	28273	2.137233	60426	88699	0.681248
P3	inc5	62773	25926	2.421237	62773	88699	0.707708
P4	inc1	5	33	0.151515	5	38	0.131579
P4	inc3	5	33	0.151515	5	38	0.131579
P4	inc5	6	32	0.187500	6	38	0.157895
P5	inc1	6	52	0.115385	6	58	0.103448
P5	inc3	6	52	0.115385	6	58	0.103448
P5	inc5	12	46	0.260870	12	58	0.206897
P6	inc1	0	108	0.000000	0	108	0.000000
P6	inc3	22	86	0.255814	22	108	0.203704
P6	inc5	24	84	0.285714	24	108	0.222222
P7	inc1	25	278	0.089928	25	303	0.082508
P7	inc3	52	251	0.207171	52	303	0.171617
P7	inc5	76	227	0.334802	76	303	0.250825
P8	inc1	32	504	0.063492	32	536	0.059701
P8	inc3	136	400	0.340000	136	536	0.253731
P8	inc5	175	361	0.484765	175	536	0.326493
P9	inc1	20	351	0.056980	20	371	0.053908
P9	inc3	31	340	0.091176	31	371	0.083558
P9	inc5	68	303	0.224422	68	371	0.183288
P10	inc1	1206	2933	0.411183	1206	4139	0.291375
P10	inc3	1955	2184	0.895147	1955	4139	0.472336
P10	inc5	2043	2096	0.974714	2043	4139	0.493597
P11	inc1	1365	2759	0.494744	1365	4124	0.330989
P11	inc3	2027	2097	0.966619	2027	4124	0.491513
P11	inc5	2120	2004	1.057884	2120	4124	0.514064
P12	inc1	405	3800	0.106579	405	4205	0.096314
P12	inc3	889	3316	0.268094	889	4205	0.211415
P12	inc5	1234	2971	0.415348	1234	4205	0.293460
P13	inc1	1033	7202	0.143432	1033	8235	0.125440
P13	inc3	2406	5829	0.412764	2406	8235	0.292168
P13	inc5	3151	5084	0.619788	3151	8235	0.382635
P14	inc1	1934	10331	0.187204	1934	12265	0.157684
P14	inc3	4131	8134	0.507868	4131	12265	0.336812
P14	inc5	5024	7241	0.693827	5024	12265	0.409621
P15	inc1	4799	22061	0.217533	4799	26860	0.178667
P15	inc3	9144	17716	0.516144	9144	26860	0.340432
P15	inc5	11606	15254	0.760850	11606	26860	0.432092
P16	inc1	13796	30725	0.449015	13796	44521	0.309876
P16	inc3	18388	26133	0.703631	18388	44521	0.413019
P16	inc5	20505	24016	0.853806	20505	44521	0.460569
P17	inc1	19763	40886	0.483368	19763	60649	0.325859
P17	inc3	26588	34061	0.780600	26588	60649	0.438391
P17	inc5	28621	32028	0.893624	28621	60649	0.471912
P18	inc1	26192	50577	0.517864	26192	76769	0.341179
P18	inc3	33500	43269	0.774226	33500	76769	0.436374
P18	inc5	36274	40495	0.895765	36274	76769	0.472508
P19	inc1	33439	63496	0.526632	33439	96935	0.344963
P19	inc3	43103	53832	0.800695	43103	96935	0.444659
P19	inc5	46087	50848	0.906368	46087	96935	0.475442
P20	inc1	7959	86265	0.092262	7959	94224	0.084469
P20	inc3	19980	74244	0.269113	19980	94224	0.212048
P20	inc5	27193	67031	0.405678	27193	94224	0.288600
```

### SEMINAIVE stage vs delta counts (DeltaNodes/DeltaEdges)
Delta* columns come from `[inc-naive] delta counts` in FORWARD_COMPILATION logs.
```tsv
Case	Delta	Turn	Full_SEM_s	Inc_SEM_s	Speedup	DeltaInsNodes	DeltaInsEdges	DeltaDelNodes	DeltaDelEdges
P1	inc1	del	0.036916	0.107275	0.344	0	0	0	0
P1	inc1	ins	0.146112	0.087999	1.660	0	0	0	0
P1	inc3	del	0.027843	0.140441	0.198	0	0	0	0
P1	inc3	ins	0.166298	0.146420	1.136	0	0	0	0
P1	inc5	del	0.028716	0.117594	0.244	0	0	5	20
P1	inc5	ins	0.156379	0.114796	1.362	41	126	0	0
P3	inc1	del	0.048905	0.097985	0.499	0	0	0	0
P3	inc1	ins	0.141773	0.092253	1.537	0	0	0	0
P3	inc3	del	0.048009	0.088201	0.544	0	0	0	0
P3	inc3	ins	0.150190	0.072500	2.072	0	0	0	0
P3	inc5	del	0.025540	0.098974	0.258	0	0	0	0
P3	inc5	ins	0.159499	0.117883	1.353	0	0	0	0
P4	inc1	del	0.000921	0.000961	0.958	0	0	0	0
P4	inc1	ins	0.000884	0.000866	1.021	0	0	0	0
P4	inc3	del	0.000903	0.000894	1.010	0	0	0	0
P4	inc3	ins	0.001640	0.000787	2.085	0	0	0	0
P4	inc5	del	0.000886	0.001143	0.775	0	0	0	0
P4	inc5	ins	0.000891	0.001432	0.622	0	0	0	0
P5	inc1	del	0.000258	0.001385	0.186	0	0	7	6
P5	inc1	ins	0.000278	0.002362	0.118	49	24	0	0
P5	inc3	del	0.000261	0.001015	0.257	0	0	7	6
P5	inc3	ins	0.000275	0.001243	0.221	49	24	0	0
P5	inc5	del	0.000259	0.001801	0.144	0	0	9	7
P5	inc5	ins	0.000280	0.001702	0.164	49	24	0	0
P6	inc1	del	0.003643	0.003390	1.075	0	0	8	7
P6	inc1	ins	0.004171	0.003336	1.250	73	35	0	0
P6	inc3	del	0.002740	0.005383	0.509	0	0	58	561
P6	inc3	ins	0.003907	0.003790	1.031	143	609	0	0
P6	inc5	del	0.002669	0.004438	0.601	0	0	60	562
P6	inc5	ins	0.004169	0.002740	1.522	145	610	0	0
P7	inc1	del	0.013023	0.004702	2.770	0	0	1	1
P7	inc1	ins	0.013293	0.002075	6.405	1	1	0	0
P7	inc3	del	0.011901	0.004278	2.782	0	0	10	8
P7	inc3	ins	0.011909	0.002934	4.059	20	12	0	0
P7	inc5	del	0.011736	0.004673	2.512	0	0	16	13
P7	inc5	ins	0.011901	0.003143	3.787	35	21	0	0
P8	inc1	del	0.048436	0.002659	18.213	0	0	22	20
P8	inc1	ins	0.049501	0.002519	19.653	78	47	0	0
P8	inc3	del	0.045830	0.004410	10.392	0	0	48	42
P8	inc3	ins	0.048340	0.005250	9.208	207	130	0	0
P8	inc5	del	0.043373	0.012522	3.464	0	0	159	2039
P8	inc5	ins	0.048339	0.007430	6.505	312	2127	0	0
P9	inc1	del	0.023308	0.009787	2.382	0	0	40	38
P9	inc1	ins	0.025373	0.003356	7.560	169	94	0	0
P9	inc3	del	0.020228	0.010211	1.981	0	0	61	54
P9	inc3	ins	0.025115	0.006206	4.047	283	155	0	0
P9	inc5	del	0.020036	0.011923	1.680	0	0	121	107
P9	inc5	ins	0.025344	0.006571	3.857	380	207	0	0
P10	inc1	del	0.132803	0.048911	2.715	0	0	6	12
P10	inc1	ins	0.138227	0.011944	11.573	20	29	0	0
P10	inc3	del	0.125004	0.033687	3.711	0	0	18	36
P10	inc3	ins	0.159665	0.019541	8.171	60	87	0	0
P10	inc5	del	0.121538	0.042503	2.860	0	0	26	48
P10	inc5	ins	0.138532	0.026602	5.208	69	100	0	0
P11	inc1	del	0.152564	0.037789	4.037	0	0	0	0
P11	inc1	ins	0.145554	0.011635	12.510	0	0	0	0
P11	inc3	del	0.125391	0.039399	3.183	0	0	0	0
P11	inc3	ins	0.133104	0.018303	7.272	0	0	0	0
P11	inc5	del	0.118603	0.040901	2.900	0	0	0	0
P11	inc5	ins	0.136019	0.023475	5.794	0	0	0	0
P12	inc1	del	0.706162	0.069146	10.213	0	0	261	974
P12	inc1	ins	0.706354	0.039483	17.890	847	1240	0	0
P12	inc3	del	0.580622	0.199154	2.915	0	0	932	6519
P12	inc3	ins	0.755593	0.128586	5.876	2883	9272	0	0
P12	inc5	del	0.507442	0.343902	1.476	0	0	1133	6690
P12	inc5	ins	0.818368	0.263307	3.108	3429	9586	0	0
P13	inc1	del	1.376175	0.177471	7.754	0	0	825	784
P13	inc1	ins	1.640598	0.080342	20.420	3341	1903	0	0
P13	inc3	del	1.355083	0.412714	3.283	0	0	1524	2731
P13	inc3	ins	1.595062	0.193344	8.250	5704	4583	0	0
P13	inc5	del	1.078506	0.647298	1.666	0	0	2263	3392
P13	inc5	ins	1.573994	0.343302	4.585	8322	6105	0	0
P14	inc1	del	2.392938	0.301984	7.924	0	0	1913	11418
P14	inc1	ins	2.715976	0.117581	23.099	6366	13718	0	0
P14	inc3	del	1.971623	0.883591	2.231	0	0	3361	15286
P14	inc3	ins	2.842919	0.342532	8.300	11081	19094	0	0
P14	inc5	del	1.680476	1.129641	1.488	0	0	4398	20790
P14	inc5	ins	2.713648	0.704194	3.854	13478	25687	0	0
P15	inc1	del	6.273233	1.520492	4.126	0	0	4641	9740
P15	inc1	ins	6.872711	0.425052	16.169	16239	14719	0	0
P15	inc3	del	5.400065	3.166347	1.705	0	0	7907	13675
P15	inc3	ins	6.997433	1.436146	4.872	25469	20970	0	0
P15	inc5	del	4.450649	3.969988	1.121	0	0	9591	15183
P15	inc5	ins	6.703326	2.242605	2.989	28603	22756	0	0
P16	inc1	del	9.711456	4.291374	2.263	0	0	10667	11358
P16	inc1	ins	10.817472	0.645041	16.770	37555	30695	0	0
P16	inc3	del	8.573984	5.470151	1.567	0	0	15306	20180
P16	inc3	ins	10.703239	1.904506	5.620	48378	42378	0	0
P16	inc5	del	7.482854	6.587379	1.136	0	0	16867	21450
P16	inc5	ins	10.516854	3.441247	3.056	50417	49738	0	0
P17	inc1	del	14.759640	7.556252	1.953	0	0	15085	14826
P17	inc1	ins	15.657365	0.957138	16.359	51854	41787	0	0
P17	inc3	del	12.956262	9.191922	1.410	0	0	21043	24797
P17	inc3	ins	15.656860	3.018116	5.188	65907	55114	0	0
P17	inc5	del	11.574487	10.242238	1.130	0	0	23414	31172
P17	inc5	ins	16.134601	4.845710	3.330	69310	57072	0	0
P18	inc1	del	18.454210	11.865074	1.555	0	0	22006	27139
P18	inc1	ins	20.306671	1.276674	15.906	72790	49800	0	0
P18	inc3	del	16.183393	13.652050	1.185	0	0	27665	32015
P18	inc3	ins	19.669804	4.135282	4.757	84577	56586	0	0
P18	inc5	del	14.690028	15.338593	0.958	0	0	29782	38152
P18	inc5	ins	19.821241	6.282177	3.155	86647	63407	0	0
P19	inc1	del	23.004712	19.334280	1.190	0	0	28104	31623
P19	inc1	ins	27.851344	2.054866	13.554	92331	57249	0	0
P19	inc3	del	20.971962	20.105154	1.043	0	0	35264	48592
P19	inc3	ins	27.365268	5.281558	5.181	107060	78040	0	0
P19	inc5	del	18.709208	21.445900	0.872	0	0	37900	50323
P19	inc5	ins	27.096717	8.038793	3.371	110344	89927	0	0
P20	inc1	del	17.438649	6.625350	2.632	0	0	8029	7494
P20	inc1	ins	18.201902	1.006348	18.087	34920	19243	0	0
P20	inc3	del	16.014717	15.075597	1.062	0	0	19218	20656
P20	inc3	ins	18.287194	2.734843	6.687	77874	47293	0	0
P20	inc5	del	15.035446	18.383152	0.818	0	0	26642	39472
P20	inc5	ins	18.059595	4.419234	4.087	97639	73529	0	0
```

## Notes
- Do not commit anything under `experiments/`; keep it local.
- If you edit delta files manually (`delta/inc1*.txt`), rerun compile+run so
  probabilities match.
- If you edit legacy delta files (`delta/inc10_*.txt`), rerun compile+run so
  probabilities match.
- `reordering_runtime` is a CUDD delta timer with millisecond resolution
  (computed from `Cudd_ReadReorderingTime()`).
- If `time` reports `user > real`, some work is running in parallel threads
  (OpenMP or other). To force single-thread runs: `OMP_NUM_THREADS=1
  OMP_THREAD_LIMIT=1` or pass `-j 1`.
- CLI runs invoke `tryGarbageCollection()` at the end of each turn to encourage
  BDD cleanup after deletions.
- CLI accepts non-interactive stdin (e.g., `< delta/inc1.txt`) and will process
  `insert/delete/commit/q` lines.
- `results-souffle-inc.tsv` includes all `delta-*.json` present under each
  `output/` directory; remove stale delta outputs (e.g., old `inc10`) before
  collecting if you want a clean inc1/inc3/inc5-only TSV.

## How I run the inc experiments (exact commands)
From repo root, with build Souffle on PATH:
```bash
export PATH="/home/hugh/research/datalog/souffle/build/src:$PATH"

# Generate cases (P1, P3, P4-P20)
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval generate --cases 1,3,4-20 --cleanup

# Generate deltas (default change spec inc1/inc3/inc5)
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval delta --cases 1,3,4-20 --cleanup

# Compile compute for each case using online CLI support
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval compile --cases 1,3,4-20 --timeout 600

# Run baseline (full+inc) and one sample of inc1/inc3/inc5 per case
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval run   --cases 1,3,4-20 --delta-labels inc1,inc3,inc5 --delta-samples 1 --timeout 600

# Collect TSV
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py   --base-dir experiments/side_channel_inc_eval collect --cases 1,3,4-20
```

### Trimmed ruleset run (no equal_assign)
```bash
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval \
  generate --cases 1,3,4-20 --cleanup --rule-set trimmed

python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval \
  delta --cases 1,3,4-20 --cleanup

JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval \
  compile --cases 1,3,4-20 --timeout 600 --jobs ${JOBS}

python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval \
  run --cases 1,3,4-20 --delta-labels inc1,inc3,inc5 --delta-samples 1 --timeout 600

python /home/hugh/research/datalog/problog-benchmark/side_channel_inc.py \
  --base-dir experiments/side_channel_inc_trimmed_eval \
  collect --cases 1,3,4-20
```
### Legacy inc10 run (2025-12-21)
```bash
export PATH="/home/hugh/research/datalog/souffle/cmake-build-release/src:$PATH"

# Generate cases P4-P13 (skip P1/P3 due to current inc loop issue)
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
- Stage logs: `output/log_P*_<label>_1_{inc,full}_*.json`.
- Consistency + paths: `output/delta-<label>-1.json` (base/iter comparisons,
  `max|d|`, etc).
- Prob outputs: `output/facts.{inc,full}.prob`,
  `output/fact-iterN-{inc-naive,inc-regional,full}.prob`, plus runner-prefixed
  `output/delta-<label>-1-{inc-naive,inc-regional,full}-fact-iterN-<mode>.prob`.
