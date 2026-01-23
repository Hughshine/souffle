# OPT

## Source references
- [problog-benchmark/side_channel_inc.py](problog-benchmark/side_channel_inc.py)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [src/include/souffle/problog/RegionalIncremental.h](src/include/souffle/problog/RegionalIncremental.h)

This note captures the current execution flow and the data collected for
inc-regional evaluation on the Side-Channel benchmark.

## Execution (fresh dataset)

Build Souffle:
```
cmake -S . -B build
JOBS=$(nproc || sysctl -n hw.ncpu || echo 2)
cmake --build build -j${JOBS}
```

Compile per-case compute binaries (use the repo build binary):
```
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 problog-benchmark/side_channel_inc.py \
  --base-dir problog-benchmark/side_channel_inc_strengthen_fresh \
  compile --cases 1-20 --jobs 4 --timeout 600
```

Run compare-all (det-opt required):
```
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 problog-benchmark/side_channel_inc.py \
  --base-dir problog-benchmark/side_channel_inc_strengthen_fresh \
  run --cases 1-20 --timeout 900 --compare-all --run-arg=--det-opt
```

If the full run times out near the end, re-run the tail:
```
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 problog-benchmark/side_channel_inc.py \
  --base-dir problog-benchmark/side_channel_inc_strengthen_fresh \
  run --cases 19-20 --timeout 900 --compare-all --run-arg=--det-opt
```

Collect summary tables:
```
python3 problog-benchmark/side_channel_inc.py \
  --base-dir problog-benchmark/side_channel_inc_strengthen_fresh \
  collect --cases 1-20
```

## Collected Information

### End-to-end correctness + runtime
- `problog-benchmark/side_channel_inc_strengthen_fresh/results-souffle-inc.tsv`
  - per-delta correctness and total wall time for `full` vs `inc` (classic incremental).
  - `inc-naive`/`inc-regional` end-to-end timings are available in the stage breakdown
    table (see below).

### Per-turn stage timings
- Per-run logs are in `problog-benchmark/side_channel_inc_strengthen_fresh/P*/output/log_*.json`.
  Each log file contains `turns[]` with `stages[]` entries. Use these logs if you
  need fine-grained stage timing breakdowns.

### inc-regional final region/dr ratio (default output)
- `[inc-regional-final]` is now always printed without `--profile-inc-regional`.
- Parsed into each delta JSON at:
  - `problog-benchmark/side_channel_inc_strengthen_fresh/P*/output/delta-*.json`
  - field: `inc_regional.stdout_stats.inc_regional_final`
  - keys: `region_nodes`, `dr_nodes`, `ratio`
- If `deltaReachable` is empty (reachNodes=0), no `inc_regional_final` line is
  emitted; treat ratio as 0 for those cases.

Helper snippet (dump final ratios to TSV):
```
python3 - <<'PY'
import json
from pathlib import Path
base = Path('problog-benchmark/side_channel_inc_strengthen_fresh')
order = {'inc1':0,'inc3':1,'inc5':2}
rows = []
for case_dir in sorted([p for p in base.iterdir() if p.is_dir() and p.name.startswith('P')],
                       key=lambda p: int(p.name[1:])):
    outdir = case_dir / 'output'
    for delta_json in sorted(outdir.glob('delta-*.json')):
        obj = json.loads(delta_json.read_text())
        stats = obj.get('inc_regional', {}).get('stdout_stats', {})
        final = stats.get('inc_regional_final')
        if final:
            entry = final[-1]
            rows.append((case_dir.name, obj.get('delta_label'), obj.get('delta_sample'),
                         entry.get('region_nodes'), entry.get('dr_nodes'), entry.get('ratio')))
        else:
            # dr=0 cases: no final ratio printed
            rows.append((case_dir.name, obj.get('delta_label'), obj.get('delta_sample'), '', 0, ''))
rows.sort(key=lambda r: (int(r[0][1:]), order.get(r[1], 99)))
print('Case\tDelta\tSample\tRegionNodes\tDRNodes\tRatio')
for r in rows:
    print(f"{r[0]}\t{r[1]}\t{r[2]}\t{r[3]}\t{r[4]}\t{r[5]}")
PY
```

## Experiment: 2026-01-23 (fresh dataset, compare-all, --det-opt)

### End-to-end results (results-souffle-inc.tsv)
```
Case	DeltaLabel	DeltaSample	IncTime	FullTime	IncExit	FullExit	Comment	OK
P1	inc1	1	0.059028	0.111764	0	0		1
P1	inc3	1	0.058556	0.139184	0	0		1
P1	inc5	1	0.059504	0.121387	0	0		1
P3	inc1	1	0.049981	0.122579	0	0		1
P3	inc3	1	0.049590	0.107805	0	0		1
P3	inc5	1	0.049940	0.110915	0	0		1
P4	inc1	1	0.024965	0.016250	0	0		1
P4	inc3	1	0.017878	0.017396	0	0		1
P4	inc5	1	0.021198	0.017023	0	0		1
P5	inc1	1	0.024643	0.019356	0	0		1
P5	inc3	1	0.026327	0.021990	0	0		1
P5	inc5	1	0.036046	0.024475	0	0		1
P6	inc1	1	0.048676	0.031215	0	0		1
P6	inc3	1	0.047180	0.030392	0	0		1
P6	inc5	1	0.065433	0.032072	0	0		1
P7	inc1	1	0.052828	0.040469	0	0		1
P7	inc3	1	0.050005	0.043125	0	0		1
P7	inc5	1	0.050281	0.039646	0	0		1
P8	inc1	1	0.056428	0.051907	0	0		1
P8	inc3	1	0.067396	0.119643	0	0		1
P8	inc5	1	0.064542	0.055924	0	0		1
P9	inc1	1	0.160477	0.311122	0	0		1
P9	inc3	1	0.173882	0.279172	0	0		1
P9	inc5	1	0.186614	0.291672	0	0		1
P10	inc1	1	0.114897	0.190471	0	0		1
P10	inc3	1	0.125651	0.200395	0	0		1
P10	inc5	1	0.139154	0.193830	0	0		1
P11	inc1	1	0.096387	0.166379	0	0		1
P11	inc3	1	0.107937	0.168690	0	0		1
P11	inc5	1	0.132378	0.192735	0	0		1
P12	inc1	1	0.630065	1.685182	0	0		1
P12	inc3	1	0.704620	1.650426	0	0		1
P12	inc5	1	0.645334	1.684348	0	0		1
P13	inc1	1	1.807953	4.708074	0	0		1
P13	inc3	1	2.010746	4.905165	0	0		1
P13	inc5	1	1.934392	4.790488	0	0		1
P14	inc1	1	4.047202	11.815426	0	0		1
P14	inc3	1	4.311342	10.749469	0	0		1
P14	inc5	1	4.313414	10.842695	0	0		1
P15	inc1	1	9.655483	24.276063	0	0		1
P15	inc3	1	9.878095	24.334155	0	0		1
P15	inc5	1	10.705408	23.985745	0	0		1
P16	inc1	1	14.902040	40.712748	0	0		1
P16	inc3	1	15.643838	37.398356	0	0		1
P16	inc5	1	16.975747	36.799365	0	0		1
P17	inc1	1	10.391966	25.953893	0	0		1
P17	inc3	1	10.665268	26.000825	0	0		1
P17	inc5	1	11.133931	25.941732	0	0		1
P18	inc1	1	20.712797	55.428410	0	0		1
P18	inc3	1	21.736876	55.165045	0	0		1
P18	inc5	1	22.647322	50.312594	0	0		1
P19	inc1	1	32.816692	84.255178	0	0		1
P19	inc3	1	34.594704	84.530007	0	0		1
P19	inc5	1	35.480351	86.556811	0	0		1
P20	inc1	1	14.851109	22.323774	0	0		1
P20	inc3	1	15.371362	21.677584	0	0		1
P20	inc5	1	15.049890	21.438225	0	0		1
```

### Stage breakdown (results-stage-breakdown.tsv)
```
Case	DeltaLabel	DeltaSample	Mode	Turn	Total_s	SEM	PRN	FC	WMC
P1	inc1	1	full	delete	0.035887	0.031886	0.000184	0.000873	0.000031
P1	inc1	1	full	insert	0.032754	0.030687	0.000180	0.000855	0.000032
P1	inc1	1	inc-naive	delete	0.006118	0.004917	0.000419	0.000556	0.000062
P1	inc1	1	inc-naive	insert	0.003800	0.002959	0.000274	0.000386	0.000049
P1	inc1	1	inc-regional	delete	0.006218	0.005308	0.000323	0.000042	0.000368
P1	inc1	1	inc-regional	insert	0.004343	0.003359	0.000330	0.000040	0.000447
P1	inc3	1	full	delete	0.037255	0.031740	0.000240	0.000933	0.000035
P1	inc3	1	full	insert	0.035943	0.032908	0.000191	0.001052	0.000035
P1	inc3	1	inc-naive	delete	0.005537	0.004549	0.000424	0.000374	0.000050
P1	inc3	1	inc-naive	insert	0.004057	0.003297	0.000257	0.000321	0.000048
P1	inc3	1	inc-regional	delete	0.005666	0.004756	0.000349	0.000044	0.000380
P1	inc3	1	inc-regional	insert	0.004125	0.003346	0.000277	0.000033	0.000332
P1	inc5	1	full	delete	0.036725	0.032343	0.000210	0.000930	0.000052
P1	inc5	1	full	insert	0.039793	0.037382	0.000192	0.001083	0.000035
P1	inc5	1	inc-naive	delete	0.005908	0.004766	0.000461	0.000454	0.000091
P1	inc5	1	inc-naive	insert	0.003720	0.002930	0.000274	0.000328	0.000049
P1	inc5	1	inc-regional	delete	0.006028	0.004867	0.000482	0.000046	0.000470
P1	inc5	1	inc-regional	insert	0.003843	0.002891	0.000255	0.000033	0.000354
P3	inc1	1	full	delete	0.034721	0.030065	0.000218	0.000965	0.000033
P3	inc1	1	full	insert	0.040037	0.037636	0.000216	0.000992	0.000035
P3	inc1	1	inc-naive	delete	0.003513	0.002562	0.000282	0.000442	0.000084
P3	inc1	1	inc-naive	insert	0.004129	0.003246	0.000218	0.000479	0.000051
P3	inc1	1	inc-regional	delete	0.003059	0.002226	0.000238	0.000040	0.000414
P3	inc1	1	inc-regional	insert	0.004519	0.003461	0.000362	0.000040	0.000478
P3	inc3	1	full	delete	0.034045	0.029459	0.000304	0.001159	0.000038
P3	inc3	1	full	insert	0.031810	0.029293	0.000258	0.000973	0.000035
P3	inc3	1	inc-naive	delete	0.002906	0.002065	0.000228	0.000428	0.000052
P3	inc3	1	inc-naive	insert	0.003977	0.003256	0.000203	0.000336	0.000049
P3	inc3	1	inc-regional	delete	0.002721	0.001881	0.000249	0.000034	0.000420
P3	inc3	1	inc-regional	insert	0.003739	0.002956	0.000218	0.000033	0.000397
P3	inc5	1	full	delete	0.034327	0.029346	0.000220	0.001082	0.000036
P3	inc5	1	full	insert	0.032181	0.029457	0.000273	0.001093	0.000034
P3	inc5	1	inc-naive	delete	0.003831	0.002850	0.000233	0.000533	0.000064
P3	inc5	1	inc-naive	insert	0.004236	0.003186	0.000335	0.000478	0.000061
P3	inc5	1	inc-regional	delete	0.003046	0.002166	0.000226	0.000042	0.000446
P3	inc5	1	inc-regional	insert	0.002187	0.001410	0.000218	0.000033	0.000389
P4	inc1	1	full	delete	0.003618	0.000571	0.000096	0.000080	0.000024
P4	inc1	1	full	insert	0.001424	0.000468	0.000087	0.000098	0.000025
P4	inc1	1	inc-naive	delete	0.001126	0.000720	0.000196	0.000044	0.000027
P4	inc1	1	inc-naive	insert	0.004890	0.004376	0.000233	0.000057	0.000038
P4	inc1	1	inc-regional	delete	0.001292	0.000809	0.000278	0.000037	0.000033
P4	inc1	1	inc-regional	insert	0.004733	0.004326	0.000171	0.000040	0.000039
P4	inc3	1	full	delete	0.003521	0.000567	0.000119	0.000137	0.000031
P4	inc3	1	full	insert	0.001616	0.000609	0.000106	0.000120	0.000030
P4	inc3	1	inc-naive	delete	0.001451	0.001071	0.000163	0.000044	0.000038
P4	inc3	1	inc-naive	insert	0.004589	0.004230	0.000166	0.000040	0.000025
P4	inc3	1	inc-regional	delete	0.002888	0.002494	0.000153	0.000041	0.000046
P4	inc3	1	inc-regional	insert	0.004628	0.004211	0.000190	0.000033	0.000057
P4	inc5	1	full	delete	0.003774	0.000589	0.000139	0.000124	0.000037
P4	inc5	1	full	insert	0.001234	0.000459	0.000085	0.000097	0.000024
P4	inc5	1	inc-naive	delete	0.003740	0.003239	0.000245	0.000053	0.000032
P4	inc5	1	inc-naive	insert	0.004415	0.004011	0.000168	0.000047	0.000031
P4	inc5	1	inc-regional	delete	0.001587	0.001240	0.000147	0.000037	0.000032
P4	inc5	1	inc-regional	insert	0.004830	0.004401	0.000193	0.000039	0.000039
P5	inc1	1	full	delete	0.003348	0.000470	0.000129	0.000296	0.000031
P5	inc1	1	full	insert	0.001774	0.000690	0.000095	0.000306	0.000032
P5	inc1	1	inc-naive	delete	0.005654	0.005051	0.000228	0.000173	0.000043
P5	inc1	1	inc-naive	insert	0.004226	0.003671	0.000209	0.000146	0.000038
P5	inc1	1	inc-regional	delete	0.005557	0.005076	0.000169	0.000040	0.000138
P5	inc1	1	inc-regional	insert	0.004399	0.003938	0.000167	0.000033	0.000124
P5	inc3	1	full	delete	0.003857	0.000490	0.000125	0.000308	0.000032
P5	inc3	1	full	insert	0.001736	0.000493	0.000184	0.000357	0.000033
P5	inc3	1	inc-naive	delete	0.006047	0.005558	0.000167	0.000148	0.000035
P5	inc3	1	inc-naive	insert	0.004725	0.004185	0.000190	0.000150	0.000039
P5	inc3	1	inc-regional	delete	0.005676	0.005172	0.000192	0.000040	0.000138
P5	inc3	1	inc-regional	insert	0.004820	0.004126	0.000277	0.000049	0.000178
P5	inc5	1	full	delete	0.004879	0.000667	0.000138	0.000399	0.000041
P5	inc5	1	full	insert	0.002026	0.000696	0.000141	0.000382	0.000042
P5	inc5	1	inc-naive	delete	0.015211	0.014707	0.000196	0.000144	0.000035
P5	inc5	1	inc-naive	insert	0.004711	0.004118	0.000230	0.000148	0.000049
P5	inc5	1	inc-regional	delete	0.005596	0.004988	0.000218	0.000044	0.000167
P5	inc5	1	inc-regional	insert	0.004366	0.003889	0.000140	0.000040	0.000145
P6	inc1	1	full	delete	0.004277	0.000992	0.000167	0.000758	0.000046
P6	inc1	1	full	insert	0.002752	0.000936	0.000182	0.000788	0.000052
P6	inc1	1	inc-naive	delete	0.010760	0.009449	0.000296	0.000833	0.000051
P6	inc1	1	inc-naive	insert	0.011016	0.009950	0.000254	0.000290	0.000345
P6	inc1	1	inc-regional	delete	0.011267	0.009886	0.000291	0.000868	0.000057
P6	inc1	1	inc-regional	insert	0.009312	0.008219	0.000220	0.000421	0.000321
P6	inc3	1	full	delete	0.004482	0.001038	0.000175	0.000778	0.000047
P6	inc3	1	full	insert	0.003699	0.000983	0.000150	0.000819	0.000053
P6	inc3	1	inc-naive	delete	0.011779	0.010240	0.000303	0.001017	0.000058
P6	inc3	1	inc-naive	insert	0.011960	0.010810	0.000282	0.000316	0.000394
P6	inc3	1	inc-regional	delete	0.011254	0.009888	0.000292	0.000857	0.000057
P6	inc3	1	inc-regional	insert	0.011888	0.010771	0.000238	0.000403	0.000343
P6	inc5	1	full	delete	0.005641	0.001209	0.000208	0.000945	0.000053
P6	inc5	1	full	insert	0.003350	0.001158	0.000191	0.000832	0.000055
P6	inc5	1	inc-naive	delete	0.010879	0.009610	0.000235	0.000854	0.000049
P6	inc5	1	inc-naive	insert	0.011672	0.010499	0.000293	0.000321	0.000395
P6	inc5	1	inc-regional	delete	0.011403	0.009993	0.000284	0.000903	0.000058
P6	inc5	1	inc-regional	insert	0.012537	0.010440	0.000239	0.000864	0.000829
P7	inc1	1	full	delete	0.007799	0.002970	0.000313	0.001221	0.000076
P7	inc1	1	full	insert	0.005377	0.002696	0.000312	0.001196	0.000081
P7	inc1	1	inc-naive	delete	0.011014	0.009613	0.000455	0.000704	0.000080
P7	inc1	1	inc-naive	insert	0.014673	0.013625	0.000359	0.000480	0.000061
P7	inc1	1	inc-regional	delete	0.010593	0.009416	0.000384	0.000072	0.000580
P7	inc1	1	inc-regional	insert	0.011494	0.010426	0.000407	0.000034	0.000487
P7	inc3	1	full	delete	0.008048	0.002862	0.000313	0.001174	0.000076
P7	inc3	1	full	insert	0.005526	0.002721	0.000287	0.001142	0.000079
P7	inc3	1	inc-naive	delete	0.010135	0.008848	0.000399	0.000683	0.000066
P7	inc3	1	inc-naive	insert	0.011001	0.009791	0.000390	0.000584	0.000075
P7	inc3	1	inc-regional	delete	0.026529	0.025111	0.000563	0.000052	0.000641
P7	inc3	1	inc-regional	insert	0.010888	0.009830	0.000410	0.000035	0.000477
P7	inc5	1	full	delete	0.007403	0.002792	0.000263	0.001127	0.000099
P7	inc5	1	full	insert	0.005232	0.002714	0.000268	0.001133	0.000104
P7	inc5	1	inc-naive	delete	0.010092	0.008955	0.000401	0.000538	0.000064
P7	inc5	1	inc-naive	insert	0.011202	0.010008	0.000428	0.000533	0.000073
P7	inc5	1	inc-regional	delete	0.010295	0.008914	0.000464	0.000050	0.000703
P7	inc5	1	inc-regional	insert	0.010243	0.009248	0.000363	0.000035	0.000462
P8	inc1	1	full	delete	0.010950	0.005445	0.000337	0.001838	0.000113
P8	inc1	1	full	insert	0.009464	0.005835	0.000376	0.001836	0.000129
P8	inc1	1	inc-naive	delete	0.011062	0.009215	0.000602	0.000979	0.000108
P8	inc1	1	inc-naive	insert	0.010774	0.009497	0.000421	0.000640	0.000081
P8	inc1	1	inc-regional	delete	0.011142	0.009507	0.000509	0.000042	0.000946
P8	inc1	1	inc-regional	insert	0.010464	0.009132	0.000449	0.000034	0.000691
P8	inc3	1	full	delete	0.074111	0.006670	0.000454	0.063361	0.000094
P8	inc3	1	full	insert	0.010118	0.005275	0.000379	0.002337	0.000163
P8	inc3	1	inc-naive	delete	0.012820	0.010946	0.000769	0.000814	0.000131
P8	inc3	1	inc-naive	insert	0.012410	0.010914	0.000610	0.000654	0.000087
P8	inc3	1	inc-regional	delete	0.015095	0.012807	0.000918	0.000045	0.001149
P8	inc3	1	inc-regional	insert	0.012017	0.010478	0.000667	0.000035	0.000695
P8	inc5	1	full	delete	0.012686	0.006542	0.000404	0.001836	0.000114
P8	inc5	1	full	insert	0.009251	0.005489	0.000361	0.001871	0.000130
P8	inc5	1	inc-naive	delete	0.015377	0.010610	0.000799	0.003686	0.000123
P8	inc5	1	inc-naive	insert	0.012969	0.010432	0.000759	0.000868	0.000734
P8	inc5	1	inc-regional	delete	0.013408	0.010275	0.000624	0.002217	0.000140
P8	inc5	1	inc-regional	insert	0.013064	0.010538	0.000568	0.001012	0.000792
P9	inc1	1	full	delete	0.093430	0.003631	0.000365	0.085363	0.000134
P9	inc1	1	full	insert	0.067539	0.003398	0.000320	0.061094	0.000111
P9	inc1	1	inc-naive	delete	0.012020	0.009478	0.000743	0.001487	0.000145
P9	inc1	1	inc-naive	insert	0.010812	0.008717	0.000620	0.001182	0.000117
P9	inc1	1	inc-regional	delete	0.012132	0.009387	0.000807	0.000053	0.001713
P9	inc1	1	inc-regional	insert	0.012676	0.010708	0.000531	0.000041	0.001229
P9	inc3	1	full	delete	0.075406	0.003712	0.000423	0.067502	0.000111
P9	inc3	1	full	insert	0.063751	0.003687	0.000445	0.057024	0.000105
P9	inc3	1	inc-naive	delete	0.011592	0.008945	0.000696	0.001698	0.000073
P9	inc3	1	inc-naive	insert	0.010064	0.008257	0.000460	0.000277	0.000925
P9	inc3	1	inc-regional	delete	0.011230	0.008618	0.000685	0.001713	0.000062
P9	inc3	1	inc-regional	insert	0.010685	0.008385	0.000427	0.000790	0.000935
P9	inc5	1	full	delete	0.088314	0.003627	0.000396	0.080721	0.000118
P9	inc5	1	full	insert	0.069658	0.003685	0.000339	0.063812	0.000118
P9	inc5	1	inc-naive	delete	0.027330	0.024860	0.000703	0.001576	0.000052
P9	inc5	1	inc-naive	insert	0.012900	0.010803	0.000595	0.000332	0.001009
P9	inc5	1	inc-regional	delete	0.011213	0.008583	0.000708	0.001709	0.000055
P9	inc5	1	inc-regional	insert	0.012898	0.010482	0.000541	0.000764	0.000971
P10	inc1	1	full	delete	0.048646	0.040569	0.002700	0.000414	0.000038
P10	inc1	1	full	insert	0.044099	0.039773	0.001539	0.000398	0.000057
P10	inc1	1	inc-naive	delete	0.014323	0.011735	0.002243	0.000165	0.000042
P10	inc1	1	inc-naive	insert	0.011922	0.009442	0.002137	0.000165	0.000041
P10	inc1	1	inc-regional	delete	0.015318	0.012510	0.002256	0.000048	0.000313
P10	inc1	1	inc-regional	insert	0.011956	0.009784	0.001680	0.000050	0.000247
P10	inc3	1	full	delete	0.049068	0.041534	0.002182	0.000392	0.000040
P10	inc3	1	full	insert	0.045468	0.040831	0.001615	0.000387	0.000038
P10	inc3	1	inc-naive	delete	0.012993	0.010001	0.002558	0.000206	0.000052
P10	inc3	1	inc-naive	insert	0.011996	0.009534	0.002126	0.000161	0.000041
P10	inc3	1	inc-regional	delete	0.013154	0.009856	0.002940	0.000043	0.000173
P10	inc3	1	inc-regional	insert	0.012523	0.009626	0.002559	0.000034	0.000170
P10	inc5	1	full	delete	0.042239	0.036107	0.001257	0.000374	0.000038
P10	inc5	1	full	insert	0.040893	0.036935	0.001197	0.000380	0.000038
P10	inc5	1	inc-naive	delete	0.017589	0.014966	0.002256	0.000170	0.000043
P10	inc5	1	inc-naive	insert	0.011997	0.009804	0.001855	0.000161	0.000040
P10	inc5	1	inc-regional	delete	0.022883	0.018695	0.003794	0.000066	0.000188
P10	inc5	1	inc-regional	insert	0.013104	0.009751	0.003000	0.000035	0.000177
P11	inc1	1	full	delete	0.044399	0.038222	0.001401	0.000198	0.000031
P11	inc1	1	full	insert	0.041513	0.036195	0.001366	0.000212	0.000032
P11	inc1	1	inc-naive	delete	0.008907	0.007117	0.001537	0.000086	0.000033
P11	inc1	1	inc-naive	insert	0.007258	0.005594	0.001410	0.000086	0.000032
P11	inc1	1	inc-regional	delete	0.009451	0.007181	0.001623	0.000034	0.000459
P11	inc1	1	inc-regional	insert	0.007182	0.005602	0.001331	0.000033	0.000084
P11	inc3	1	full	delete	0.040658	0.035085	0.001124	0.000208	0.000029
P11	inc3	1	full	insert	0.039379	0.036138	0.001194	0.000191	0.000029
P11	inc3	1	inc-naive	delete	0.010484	0.008126	0.002077	0.000112	0.000034
P11	inc3	1	inc-naive	insert	0.007367	0.005636	0.001482	0.000085	0.000032
P11	inc3	1	inc-regional	delete	0.011739	0.008188	0.003267	0.000036	0.000104
P11	inc3	1	inc-regional	insert	0.008138	0.005384	0.002496	0.000035	0.000086
P11	inc5	1	full	delete	0.040157	0.035691	0.001184	0.000195	0.000029
P11	inc5	1	full	insert	0.046158	0.041723	0.001997	0.000215	0.000031
P11	inc5	1	inc-naive	delete	0.014908	0.011595	0.003039	0.000105	0.000034
P11	inc5	1	inc-naive	insert	0.008811	0.005899	0.002654	0.000091	0.000033
P11	inc5	1	inc-regional	delete	0.014154	0.010506	0.003376	0.000036	0.000101
P11	inc5	1	inc-regional	insert	0.009087	0.005589	0.003238	0.000035	0.000090
P12	inc1	1	full	delete	0.509349	0.059522	0.002820	0.420457	0.000837
P12	inc1	1	full	insert	0.535213	0.056981	0.002939	0.445543	0.000898
P12	inc1	1	inc-naive	delete	0.032376	0.008270	0.005977	0.017447	0.000521
P12	inc1	1	inc-naive	insert	0.021728	0.007830	0.004004	0.001964	0.007760
P12	inc1	1	inc-regional	delete	0.029988	0.009391	0.004730	0.015054	0.000656
P12	inc1	1	inc-regional	insert	0.025006	0.007802	0.003543	0.005864	0.007644
P12	inc3	1	full	delete	0.523655	0.056696	0.002883	0.436150	0.000850
P12	inc3	1	full	insert	0.542792	0.058502	0.003125	0.451216	0.000908
P12	inc3	1	inc-naive	delete	0.035553	0.011484	0.005706	0.017573	0.000623
P12	inc3	1	inc-naive	insert	0.028100	0.010028	0.005470	0.002824	0.009622
P12	inc3	1	inc-regional	delete	0.036877	0.011714	0.006075	0.018405	0.000530
P12	inc3	1	inc-regional	insert	0.028884	0.010340	0.004424	0.006061	0.007898
P12	inc5	1	full	delete	0.555511	0.061518	0.002957	0.460656	0.000916
P12	inc5	1	full	insert	0.531157	0.053471	0.002211	0.425605	0.000817
P12	inc5	1	inc-naive	delete	0.037602	0.011981	0.006117	0.018888	0.000464
P12	inc5	1	inc-naive	insert	0.024862	0.010734	0.003871	0.002638	0.007468
P12	inc5	1	inc-regional	delete	0.033811	0.012041	0.004996	0.016110	0.000513
P12	inc5	1	inc-regional	insert	0.030135	0.010910	0.003520	0.008035	0.007503
P13	inc1	1	full	delete	1.542373	0.113259	0.005532	1.362848	0.004382
P13	inc1	1	full	insert	1.528865	0.102900	0.005418	1.356840	0.005256
P13	inc1	1	inc-naive	delete	0.065277	0.021793	0.010050	0.030208	0.003046
P13	inc1	1	inc-naive	insert	0.054323	0.021030	0.008436	0.004262	0.020430
P13	inc1	1	inc-regional	delete	0.077056	0.022378	0.013141	0.037184	0.004182
P13	inc1	1	inc-regional	insert	0.074399	0.021085	0.011431	0.017775	0.023922
P13	inc3	1	full	delete	1.659105	0.109793	0.005371	1.489005	0.004827
P13	inc3	1	full	insert	1.554097	0.107036	0.005686	1.377127	0.004320
P13	inc3	1	inc-naive	delete	0.109153	0.025466	0.015072	0.063545	0.004902
P13	inc3	1	inc-naive	insert	0.072753	0.023065	0.013810	0.009607	0.026067
P13	inc3	1	inc-regional	delete	0.097116	0.025273	0.012466	0.054881	0.004322
P13	inc3	1	inc-regional	insert	0.081727	0.023132	0.010746	0.021283	0.026368
P13	inc5	1	full	delete	1.540429	0.110842	0.006850	1.356367	0.004155
P13	inc5	1	full	insert	1.523457	0.107803	0.007716	1.366437	0.004875
P13	inc5	1	inc-naive	delete	0.135812	0.030311	0.011310	0.089802	0.004223
P13	inc5	1	inc-naive	insert	0.080693	0.023645	0.014521	0.017334	0.025009
P13	inc5	1	inc-regional	delete	0.133148	0.028843	0.013199	0.086057	0.004867
P13	inc5	1	inc-regional	insert	0.091581	0.027192	0.012575	0.026547	0.025071
P14	inc1	1	full	delete	3.966360	0.162009	0.011727	3.516247	0.049302
P14	inc1	1	full	insert	3.911018	0.171770	0.007460	3.468051	0.047167
P14	inc1	1	inc-naive	delete	0.138696	0.027357	0.021154	0.066689	0.023316
P14	inc1	1	inc-naive	insert	0.090086	0.026992	0.015262	0.014715	0.032940
P14	inc1	1	inc-regional	delete	0.134403	0.025979	0.016669	0.058054	0.033515
P14	inc1	1	inc-regional	insert	0.110444	0.026948	0.019468	0.027539	0.036301
P14	inc3	1	full	delete	3.070338	0.174011	0.012315	2.609579	0.034607
P14	inc3	1	full	insert	3.792821	0.148911	0.007001	3.401992	0.050927
P14	inc3	1	inc-naive	delete	0.155580	0.031106	0.017313	0.082927	0.024045
P14	inc3	1	inc-naive	insert	0.127019	0.029361	0.017008	0.029341	0.051074
P14	inc3	1	inc-regional	delete	0.167776	0.031842	0.020561	0.089106	0.026042
P14	inc3	1	inc-regional	insert	0.142408	0.029366	0.021861	0.046096	0.044847
P14	inc5	1	full	delete	2.991980	0.159640	0.010056	2.561935	0.032238
P14	inc5	1	full	insert	3.728167	0.150004	0.007892	3.346173	0.039115
P14	inc5	1	inc-naive	delete	0.228043	0.039054	0.023562	0.136545	0.028695
P14	inc5	1	inc-naive	insert	0.160141	0.031809	0.017816	0.057200	0.053076
P14	inc5	1	inc-regional	delete	0.205923	0.045224	0.019957	0.115354	0.025207
P14	inc5	1	inc-regional	insert	0.141975	0.029427	0.016461	0.051440	0.044411
P15	inc1	1	full	delete	7.827974	0.328527	0.019877	6.797293	0.291000
P15	inc1	1	full	insert	8.030749	0.375333	0.020232	6.917688	0.325643
P15	inc1	1	inc-naive	delete	0.962509	0.070323	0.032546	0.690260	0.169086
P15	inc1	1	inc-naive	insert	0.312146	0.061955	0.032671	0.106491	0.110741
P15	inc1	1	inc-regional	delete	0.929030	0.079461	0.045978	0.655009	0.148340
P15	inc1	1	inc-regional	insert	0.358843	0.057742	0.036748	0.114699	0.149336
P15	inc3	1	full	delete	7.841211	0.347951	0.022395	6.771028	0.253851
P15	inc3	1	full	insert	7.918588	0.345630	0.017255	6.876859	0.307334
P15	inc3	1	inc-naive	delete	1.105858	0.079887	0.032206	0.834705	0.158836
P15	inc3	1	inc-naive	insert	0.445049	0.071276	0.044546	0.189422	0.139429
P15	inc3	1	inc-regional	delete	1.112514	0.092282	0.044335	0.800583	0.175077
P15	inc3	1	inc-regional	insert	0.521243	0.064327	0.039859	0.182869	0.233723
P15	inc5	1	full	delete	7.424425	0.314646	0.016685	6.528404	0.198130
P15	inc5	1	full	insert	8.042398	0.328569	0.017220	7.037423	0.328726
P15	inc5	1	inc-naive	delete	1.512438	0.103005	0.043250	1.194177	0.171767
P15	inc5	1	inc-naive	insert	0.602239	0.069180	0.040044	0.326732	0.165789
P15	inc5	1	inc-regional	delete	1.340680	0.091549	0.043812	1.013492	0.191589
P15	inc5	1	inc-regional	insert	0.729799	0.073310	0.036259	0.341808	0.277913
P16	inc1	1	full	delete	12.947464	0.480361	0.031803	11.415699	0.514505
P16	inc1	1	full	insert	13.508969	0.521861	0.032196	12.044019	0.471509
P16	inc1	1	inc-naive	delete	0.690001	0.094482	0.046287	0.315997	0.232938
P16	inc1	1	inc-naive	insert	0.418978	0.099298	0.052198	0.109064	0.158085
P16	inc1	1	inc-regional	delete	0.750769	0.098553	0.051440	0.317221	0.283230
P16	inc1	1	inc-regional	insert	0.523747	0.097827	0.056068	0.203063	0.166411
P16	inc3	1	full	delete	10.212527	0.522366	0.036588	8.788597	0.339735
P16	inc3	1	full	insert	13.218081	0.504835	0.028966	11.842668	0.501355
P16	inc3	1	inc-naive	delete	1.235061	0.148906	0.062098	0.731053	0.292621
P16	inc3	1	inc-naive	insert	0.702634	0.097300	0.047523	0.360335	0.196974
P16	inc3	1	inc-regional	delete	1.116350	0.138581	0.059319	0.649118	0.269028
P16	inc3	1	inc-regional	insert	0.967514	0.106757	0.056579	0.493680	0.309790
P16	inc5	1	full	delete	9.332844	0.529542	0.032796	7.946199	0.291063
P16	inc5	1	full	insert	13.325958	0.502439	0.035443	11.891787	0.486391
P16	inc5	1	inc-naive	delete	1.703859	0.153933	0.054041	1.176046	0.319530
P16	inc5	1	inc-naive	insert	1.229754	0.109262	0.057200	0.783856	0.278754
P16	inc5	1	inc-regional	delete	1.629015	0.158404	0.059731	1.094589	0.315960
P16	inc5	1	inc-regional	insert	1.430770	0.110280	0.060811	0.787923	0.470972
P17	inc1	1	full	delete	8.351644	0.686098	0.051083	7.138927	0.123655
P17	inc1	1	full	insert	8.494269	0.706776	0.051722	7.253967	0.149407
P17	inc1	1	inc-naive	delete	0.691885	0.147564	0.075132	0.402128	0.066682
P17	inc1	1	inc-naive	insert	0.483441	0.128717	0.067663	0.103716	0.182811
P17	inc1	1	inc-regional	delete	0.602062	0.148886	0.063296	0.323337	0.066154
P17	inc1	1	inc-regional	insert	0.598566	0.130006	0.057741	0.224889	0.185366
P17	inc3	1	full	delete	8.428626	0.688614	0.041255	7.226472	0.113067
P17	inc3	1	full	insert	8.403288	0.723205	0.051598	7.162495	0.144405
P17	inc3	1	inc-naive	delete	0.965972	0.206469	0.073064	0.615073	0.070960
P17	inc3	1	inc-naive	insert	0.597425	0.143937	0.064983	0.189343	0.198536
P17	inc3	1	inc-regional	delete	0.940855	0.197538	0.070024	0.597939	0.074966
P17	inc3	1	inc-regional	insert	0.732054	0.141168	0.064069	0.328331	0.197680
P17	inc5	1	full	delete	8.368943	0.667518	0.057587	7.205483	0.089750
P17	inc5	1	full	insert	8.262870	0.669815	0.039552	7.092943	0.152520
P17	inc5	1	inc-naive	delete	1.223976	0.221409	0.074577	0.853729	0.073852
P17	inc5	1	inc-naive	insert	0.728911	0.159246	0.076181	0.267610	0.225156
P17	inc5	1	inc-regional	delete	1.127522	0.223526	0.069382	0.764887	0.069354
P17	inc5	1	inc-regional	insert	0.957348	0.151623	0.080224	0.481275	0.243356
P18	inc1	1	full	delete	17.926662	0.928180	0.067228	15.859014	0.380314
P18	inc1	1	full	insert	18.359884	1.009779	0.079559	16.164624	0.418124
P18	inc1	1	inc-naive	delete	1.154217	0.202963	0.093065	0.652536	0.205022
P18	inc1	1	inc-naive	insert	0.646036	0.163881	0.089703	0.166822	0.224919
P18	inc1	1	inc-regional	delete	1.120092	0.212721	0.101427	0.614448	0.190831
P18	inc1	1	inc-regional	insert	0.944457	0.163999	0.093374	0.372656	0.312475
P18	inc3	1	full	delete	17.858593	0.882742	0.066063	15.832376	0.338364
P18	inc3	1	full	insert	18.156626	0.900725	0.061364	16.138917	0.446007
P18	inc3	1	inc-naive	delete	1.703284	0.278203	0.104215	1.127658	0.192682
P18	inc3	1	inc-naive	insert	1.024603	0.165650	0.097577	0.453783	0.306597
P18	inc3	1	inc-regional	delete	1.710586	0.282123	0.097227	1.112905	0.217710
P18	inc3	1	inc-regional	insert	1.420770	0.185168	0.090364	0.707006	0.436184
P18	inc5	1	full	delete	13.500480	0.900108	0.078907	11.464355	0.336619
P18	inc5	1	full	insert	18.013412	0.896154	0.063729	16.052808	0.409605
P18	inc5	1	inc-naive	delete	2.233670	0.323911	0.096380	1.603058	0.209817
P18	inc5	1	inc-naive	insert	1.287227	0.178109	0.096133	0.635559	0.376280
P18	inc5	1	inc-regional	delete	2.173733	0.316214	0.092836	1.531788	0.232386
P18	inc5	1	inc-regional	insert	1.635417	0.173579	0.085991	0.894700	0.479539
P19	inc1	1	full	delete	27.414056	1.246928	0.102130	23.743273	0.695340
P19	inc1	1	full	insert	27.530900	1.207567	0.093375	24.636473	0.820652
P19	inc1	1	inc-naive	delete	2.123032	0.377960	0.124475	1.226117	0.393761
P19	inc1	1	inc-naive	insert	1.423707	0.224419	0.124443	0.642524	0.431107
P19	inc1	1	inc-regional	delete	1.981979	0.340818	0.126767	1.108052	0.405409
P19	inc1	1	inc-regional	insert	1.791633	0.212892	0.106035	1.044773	0.425782
P19	inc3	1	full	delete	26.923349	1.102215	0.100696	23.733943	0.535723
P19	inc3	1	full	insert	27.923859	1.198766	0.094899	24.956084	0.855622
P19	inc3	1	inc-naive	delete	2.946657	0.471577	0.106238	1.940984	0.427049
P19	inc3	1	inc-naive	insert	2.113576	0.209809	0.118834	1.217935	0.565329
P19	inc3	1	inc-regional	delete	2.844038	0.510017	0.137914	1.779969	0.415525
P19	inc3	1	inc-regional	insert	2.787659	0.224236	0.127461	1.822275	0.610912
P19	inc5	1	full	delete	26.914924	1.355524	0.103964	23.717044	0.545326
P19	inc5	1	full	insert	27.797793	1.205109	0.101402	24.866254	0.878174
P19	inc5	1	inc-naive	delete	3.891068	0.558357	0.132376	2.782569	0.417137
P19	inc5	1	inc-naive	insert	2.255719	0.222015	0.116967	1.370137	0.544510
P19	inc5	1	inc-regional	delete	3.714897	0.567234	0.146847	2.596591	0.403637
P19	inc5	1	inc-regional	insert	3.168452	0.227794	0.162888	1.915983	0.858486
P20	inc1	1	full	delete	5.719586	1.152281	0.112241	3.898715	0.021171
P20	inc1	1	full	insert	8.468320	1.186135	0.113725	6.675295	0.025644
P20	inc1	1	inc-naive	delete	6.646623	0.257691	0.119356	6.245278	0.023070
P20	inc1	1	inc-naive	insert	0.866164	0.265606	0.115038	0.060246	0.424530
P20	inc1	1	inc-regional	delete	6.669362	0.254654	0.121936	6.268773	0.022990
P20	inc1	1	inc-regional	insert	1.220533	0.267310	0.107859	0.340536	0.503570
P20	inc3	1	full	delete	5.620096	1.105200	0.120895	3.897087	0.022505
P20	inc3	1	full	insert	8.397834	1.087752	0.104992	6.714585	0.026652
P20	inc3	1	inc-naive	delete	6.720270	0.272989	0.139638	6.279749	0.026338
P20	inc3	1	inc-naive	insert	1.080203	0.273391	0.170930	0.076261	0.558369
P20	inc3	1	inc-regional	delete	6.839399	0.278275	0.137596	6.396859	0.025382
P20	inc3	1	inc-regional	insert	1.680863	0.338971	0.185419	0.493131	0.661618
P20	inc5	1	full	delete	5.570479	1.063282	0.127028	3.941258	0.025266
P20	inc5	1	full	insert	8.498850	1.230244	0.182180	6.637101	0.023398
P20	inc5	1	inc-naive	delete	6.536413	0.272109	0.109815	6.133355	0.020035
P20	inc5	1	inc-naive	insert	0.914193	0.268281	0.129174	0.067207	0.448542
P20	inc5	1	inc-regional	delete	7.553919	0.286173	0.155851	7.078235	0.032113
P20	inc5	1	inc-regional	insert	1.206386	0.267494	0.122707	0.361015	0.453968
```

### inc-regional final region/dr ratio
```
Case  Delta  RegionNodes  DRNodes  Ratio  Status
P1    inc1               0        dr=0
P1    inc3               0        dr=0
P1    inc5               0        dr=0
P3    inc1               0        dr=0
P3    inc3               0        dr=0
P3    inc5               0        dr=0
P4    inc1               0        dr=0
P4    inc3               0        dr=0
P4    inc5               0        dr=0
P5    inc1               0        dr=0
P5    inc3               0        dr=0
P5    inc5               0        dr=0
P6    inc1   8           11       0.727  final
P6    inc3   8           11       0.727  final
P6    inc5   8           11       0.727  final
P7    inc1               0        dr=0
P7    inc3               0        dr=0
P7    inc5               0        dr=0
P8    inc1               0        dr=0
P8    inc3               0        dr=0
P8    inc5   40          59       0.678  final
P9    inc1               0        dr=0
P9    inc3   4           4        1.0    final
P9    inc5   4           4        1.0    final
P10   inc1               0        dr=0
P10   inc3               0        dr=0
P10   inc5               0        dr=0
P11   inc1               0        dr=0
P11   inc3               0        dr=0
P11   inc5               0        dr=0
P12   inc1   22          22       1.0    final
P12   inc3   46          76       0.605  final
P12   inc5   86          116      0.741  final
P13   inc1   28          31       0.903  final
P13   inc3   117         224      0.522  final
P13   inc5   317         424      0.748  final
P14   inc1   92          129      0.713  final
P14   inc3   306         400      0.765  final
P14   inc5   532         793      0.671  final
P15   inc1   200         472      0.424  final
P15   inc3   695         978      0.711  final
P15   inc5   1617        1693     0.955  final
P16   inc1   629         629      1.0    final
P16   inc3   1695        1733     0.978  final
P16   inc5   2982        3015     0.989  final
P17   inc1   1271        1274     0.998  final
P17   inc3   2526        2529     0.999  final
P17   inc5   3243        3246     0.999  final
P18   inc1   1338        1349     0.992  final
P18   inc3   3002        3013     0.996  final
P18   inc5   4114        4125     0.997  final
P19   inc1   3496        3496     1.0    final
P19   inc3   6267        6267     1.0    final
P19   inc5   7210        7225     0.998  final
P20   inc1   326         342      0.953  final
P20   inc3   672         697      0.964  final
P20   inc5   982         1027     0.956  final
```

## Experiment: 2026-01-23 (P20, --profile-inc-delete, --det-opt)

Note: `--profile-inc-delete` is captured from the inc-naive pipeline (delete path
is shared), then stored under `inc_naive.stdout_stats.inc_delete_profile` in the
delta JSON.

### inc-delete-profile (P20, inc-naive, delete turn)
```
P20 inc1 timing: total_ms=6126.732569 prep_ms=59.450001 cond_ms=0.037301 overdelete_ms=0.149704 varorder_ms=0.029055 rederive_ms=6066.911074
P20 inc1 cond: make_condition_calls=0 make_condition_ms=0.0 node_updated=0 edge_updated=0 delta_live_nodes=0 live_nodes=147275
P20 inc1 overdelete: det_imp_nodes=173 det_imp_edges=109 delta_live_nodes=17 live_nodes=146663
P20 inc1 rederive: edge_processed=17 edge_requeued=0 edge_updated=16 edge_const=0 edge_nonconst=17 node_recomputed=17 node_updated=17 node_const=0 make_and_calls=17 make_and_ms=0.031803 make_or_calls=0 make_or_ms=0.0 make_condition_calls=33 make_condition_ms=6032.582248 input_literal_calls=44 input_literal_missing=0 input_literal_ms=0.051484

P20 inc3 timing: total_ms=5735.535854 prep_ms=61.891888 cond_ms=0.061416 overdelete_ms=0.211311 varorder_ms=0.04773 rederive_ms=5673.224009
P20 inc3 cond: make_condition_calls=0 make_condition_ms=0.0 node_updated=0 edge_updated=0 delta_live_nodes=0 live_nodes=147275
P20 inc3 overdelete: det_imp_nodes=175 det_imp_edges=112 delta_live_nodes=17 live_nodes=146663
P20 inc3 rederive: edge_processed=17 edge_requeued=0 edge_updated=16 edge_const=0 edge_nonconst=17 node_recomputed=17 node_updated=17 node_const=0 make_and_calls=17 make_and_ms=0.035085 make_or_calls=0 make_or_ms=0.0 make_condition_calls=33 make_condition_ms=5636.280764 input_literal_calls=44 input_literal_missing=0 input_literal_ms=0.052452

P20 inc5 timing: total_ms=5759.30464 prep_ms=67.79533 cond_ms=0.083829 overdelete_ms=0.3215 varorder_ms=0.05297 rederive_ms=5690.849819
P20 inc5 cond: make_condition_calls=0 make_condition_ms=0.0 node_updated=0 edge_updated=0 delta_live_nodes=0 live_nodes=147275
P20 inc5 overdelete: det_imp_nodes=175 det_imp_edges=112 delta_live_nodes=17 live_nodes=146663
P20 inc5 rederive: edge_processed=17 edge_requeued=0 edge_updated=16 edge_const=0 edge_nonconst=17 node_recomputed=17 node_updated=17 node_const=0 make_and_calls=17 make_and_ms=0.038155 make_or_calls=0 make_or_ms=0.0 make_condition_calls=33 make_condition_ms=5652.146484 input_literal_calls=44 input_literal_missing=0 input_literal_ms=0.051611
```

### P20 delete slowdown analysis
- Delete time is dominated by rederive (`~5.7–6.1s`), while prep/cond/overdelete/varorder are negligible.
- Inside rederive, `make_condition_ms` accounts for almost all time (`~5.6–6.0s`), despite only 33 condition calls.
- The worklist is small (17 edges processed), so per-call conditioning cost is very high; this points to expensive BDD
  conditioning on large formulas for non-deterministic deletions, not to overdelete traversal or AND/OR construction.

## Experiment: 2026-01-23 (P20, --profile-inc-delete, after removing rederive-condition)

### inc-delete-profile (P20, inc-naive, delete turn)
```
P20 inc1 timing: total_ms=91.860761 prep_ms=58.977898 cond_ms=0.03712 overdelete_ms=0.15452 varorder_ms=0.032461 rederive_ms=32.58294
P20 inc1 cond: make_condition_calls=0 make_condition_ms=0.0 node_updated=0 edge_updated=0 delta_live_nodes=0 live_nodes=147275
P20 inc1 overdelete: det_imp_nodes=173 det_imp_edges=109 delta_live_nodes=17 live_nodes=146663
P20 inc1 rederive: edge_processed=17 edge_requeued=0 edge_updated=16 edge_const=0 edge_nonconst=17 node_recomputed=17 node_updated=17 node_const=0 make_and_calls=17 make_and_ms=0.020286 make_or_calls=0 make_or_ms=0.0 make_condition_calls=0 make_condition_ms=0.0 input_literal_calls=44 input_literal_missing=0 input_literal_ms=0.049273

P20 inc3 timing: total_ms=90.561648 prep_ms=59.578416 cond_ms=0.057988 overdelete_ms=0.221595 varorder_ms=0.049463 rederive_ms=30.6351
P20 inc3 cond: make_condition_calls=0 make_condition_ms=0.0 node_updated=0 edge_updated=0 delta_live_nodes=0 live_nodes=146853
P20 inc3 overdelete: det_imp_nodes=175 det_imp_edges=112 delta_live_nodes=17 live_nodes=146663
P20 inc3 rederive: edge_processed=28 edge_requeued=0 edge_updated=25 edge_const=0 edge_nonconst=28 node_recomputed=28 node_updated=27 node_const=0 make_and_calls=28 make_and_ms=0.031245 make_or_calls=2 make_or_ms=0.002566 make_condition_calls=0 make_condition_ms=0.0 input_literal_calls=72 input_literal_missing=0 input_literal_ms=0.064981

P20 inc5 timing: total_ms=106.849998 prep_ms=71.566053 cond_ms=0.183686 overdelete_ms=0.411324 varorder_ms=0.074891 rederive_ms=34.68052
P20 inc5 cond: make_condition_calls=0 make_condition_ms=0.0 node_updated=0 edge_updated=0 delta_live_nodes=0 live_nodes=146478
P20 inc5 overdelete: det_imp_nodes=175 det_imp_edges=112 delta_live_nodes=17 live_nodes=146663
P20 inc5 rederive: edge_processed=50 edge_requeued=0 edge_updated=45 edge_const=0 edge_nonconst=50 node_recomputed=50 node_updated=49 node_const=0 make_and_calls=50 make_and_ms=0.055375 make_or_calls=2 make_or_ms=0.002446 make_condition_calls=0 make_condition_ms=0.0 input_literal_calls=132 input_literal_missing=0 input_literal_ms=0.125156
```

### P20 delete analysis (after change)
- Re-derive no longer spends time in `makeCondition` (all zero).
- Total delete time drops from ~5.7–6.1s to ~0.09–0.11s.
- The remaining delete cost is dominated by delete prep (~60–70ms) and a small rederive loop (~30–35ms).


## Experiment: 2026-01-23 (fresh dataset, compare-all, --det-opt, no profile, after removing rederive-condition)

### End-to-end results (results-souffle-inc.tsv)
```
Case	DeltaLabel	DeltaSample	IncTime	FullTime	IncExit	FullExit	Comment	OK
P1	inc1	1	0.052832	0.109493	0	0		1
P1	inc3	1	0.048843	0.103829	0	0		1
P1	inc5	1	0.049242	0.105421	0	0		1
P3	inc1	1	0.044617	0.096825	0	0		1
P3	inc3	1	0.044598	0.099323	0	0		1
P3	inc5	1	0.044426	0.097688	0	0		1
P4	inc1	1	0.031191	0.014671	0	0		1
P4	inc3	1	0.017489	0.014485	0	0		1
P4	inc5	1	0.017594	0.014810	0	0		1
P5	inc1	1	0.022123	0.019129	0	0		1
P5	inc3	1	0.022127	0.018692	0	0		1
P5	inc5	1	0.022446	0.019326	0	0		1
P6	inc1	1	0.049758	0.031666	0	0		1
P6	inc3	1	0.062316	0.029272	0	0		1
P6	inc5	1	0.042711	0.031238	0	0		1
P7	inc1	1	0.049166	0.038846	0	0		1
P7	inc3	1	0.048031	0.040381	0	0		1
P7	inc5	1	0.049379	0.039854	0	0		1
P8	inc1	1	0.050603	0.047112	0	0		1
P8	inc3	1	0.033417	0.086775	0	0		1
P8	inc5	1	0.035455	0.041947	0	0		1
P9	inc1	1	0.137465	0.310017	0	0		1
P9	inc3	1	0.151330	0.288345	0	0		1
P9	inc5	1	0.151329	0.300562	0	0		1
P10	inc1	1	0.098627	0.157121	0	0		1
P10	inc3	1	0.107820	0.176328	0	0		1
P10	inc5	1	0.137155	0.197217	0	0		1
P11	inc1	1	0.098072	0.185297	0	0		1
P11	inc3	1	0.106301	0.168587	0	0		1
P11	inc5	1	0.122436	0.187736	0	0		1
P12	inc1	1	0.590834	1.569721	0	0		1
P12	inc3	1	0.669448	1.586031	0	0		1
P12	inc5	1	0.684412	1.791424	0	0		1
P13	inc1	1	1.638105	4.278508	0	0		1
P13	inc3	1	1.723622	4.435397	0	0		1
P13	inc5	1	1.898844	4.366790	0	0		1
P14	inc1	1	3.668338	10.236745	0	0		1
P14	inc3	1	3.734792	10.336757	0	0		1
P14	inc5	1	4.092763	9.854547	0	0		1
P15	inc1	1	8.642900	22.230204	0	0		1
P15	inc3	1	8.766481	22.275775	0	0		1
P15	inc5	1	9.010533	24.123890	0	0		1
P16	inc1	1	14.649517	39.478949	0	0		1
P16	inc3	1	15.451219	37.000411	0	0		1
P16	inc5	1	16.034238	35.743364	0	0		1
P17	inc1	1	10.085977	25.106139	0	0		1
P17	inc3	1	10.073431	25.694716	0	0		1
P17	inc5	1	10.432677	25.477730	0	0		1
P18	inc1	1	19.966148	53.668386	0	0		1
P18	inc3	1	20.619335	53.289658	0	0		1
P18	inc5	1	20.980040	49.421191	0	0		1
P19	inc1	1	32.683105	84.306038	0	0		1
P19	inc3	1	33.607160	84.279047	0	0		1
P19	inc5	1	34.077142	84.498068	0	0		1
P20	inc1	1	8.332795	19.622297	0	0		1
P20	inc3	1	9.934499	18.863506	0	0		1
P20	inc5	1	11.472925	23.793491	0	0		1
```

### Stage breakdown (results-stage-breakdown.tsv)
```
Case	DeltaLabel	DeltaSample	Mode	Turn	Total_s	SEM	PRN	FC	WMC
P1	inc1	1	full	delete	0.033642	0.029615	0.000201	0.000837	0.000041
P1	inc1	1	full	insert	0.036387	0.032983	0.000950	0.000808	0.000031
P1	inc1	1	inc-naive	delete	0.005108	0.004238	0.000303	0.000374	0.000055
P1	inc1	1	inc-naive	insert	0.004039	0.003345	0.000237	0.000293	0.000044
P1	inc1	1	inc-regional	delete	0.005179	0.004386	0.000302	0.000036	0.000333
P1	inc1	1	inc-regional	insert	0.004060	0.003243	0.000218	0.000029	0.000389
P1	inc3	1	full	delete	0.034014	0.030019	0.000216	0.000939	0.000034
P1	inc3	1	full	insert	0.030997	0.029222	0.000160	0.000779	0.000028
P1	inc3	1	inc-naive	delete	0.005261	0.004445	0.000313	0.000334	0.000045
P1	inc3	1	inc-naive	insert	0.003959	0.003248	0.000245	0.000287	0.000054
P1	inc3	1	inc-regional	delete	0.005464	0.004493	0.000370	0.000040	0.000410
P1	inc3	1	inc-regional	insert	0.003578	0.002853	0.000230	0.000029	0.000334
P1	inc5	1	full	delete	0.032650	0.028808	0.000185	0.001068	0.000031
P1	inc5	1	full	insert	0.031295	0.029318	0.000201	0.000983	0.000035
P1	inc5	1	inc-naive	delete	0.005296	0.004442	0.000353	0.000332	0.000045
P1	inc5	1	inc-naive	insert	0.003818	0.003113	0.000221	0.000319	0.000044
P1	inc5	1	inc-regional	delete	0.005385	0.004434	0.000357	0.000039	0.000418
P1	inc5	1	inc-regional	insert	0.003735	0.003018	0.000248	0.000030	0.000300
P3	inc1	1	full	delete	0.029918	0.026282	0.000168	0.000883	0.000029
P3	inc1	1	full	insert	0.029905	0.028101	0.000166	0.000877	0.000028
P3	inc1	1	inc-naive	delete	0.002877	0.002090	0.000234	0.000383	0.000048
P3	inc1	1	inc-naive	insert	0.004074	0.003230	0.000262	0.000383	0.000055
P3	inc1	1	inc-regional	delete	0.002923	0.001974	0.000263	0.000040	0.000493
P3	inc1	1	inc-regional	insert	0.003532	0.002861	0.000194	0.000029	0.000324
P3	inc3	1	full	delete	0.031607	0.028112	0.000171	0.000861	0.000029
P3	inc3	1	full	insert	0.028282	0.026361	0.000165	0.000858	0.000029
P3	inc3	1	inc-naive	delete	0.003009	0.002166	0.000282	0.000390	0.000047
P3	inc3	1	inc-naive	insert	0.003671	0.002992	0.000199	0.000310	0.000045
P3	inc3	1	inc-regional	delete	0.005444	0.004392	0.000348	0.000041	0.000520
P3	inc3	1	inc-regional	insert	0.003711	0.002794	0.000308	0.000043	0.000403
P3	inc5	1	full	delete	0.030486	0.026341	0.000209	0.000915	0.000031
P3	inc5	1	full	insert	0.028392	0.026276	0.000186	0.000863	0.000030
P3	inc5	1	inc-naive	delete	0.003127	0.002162	0.000282	0.000473	0.000059
P3	inc5	1	inc-naive	insert	0.003477	0.002807	0.000199	0.000307	0.000044
P3	inc5	1	inc-regional	delete	0.002908	0.001960	0.000289	0.000040	0.000469
P3	inc5	1	inc-regional	insert	0.003541	0.002839	0.000200	0.000029	0.000339
P4	inc1	1	full	delete	0.002917	0.000442	0.000097	0.000097	0.000023
P4	inc1	1	full	insert	0.001341	0.000621	0.000120	0.000092	0.000022
P4	inc1	1	inc-naive	delete	0.016216	0.015787	0.000202	0.000047	0.000030
P4	inc1	1	inc-naive	insert	0.003809	0.003433	0.000158	0.000044	0.000029
P4	inc1	1	inc-regional	delete	0.003206	0.002787	0.000179	0.000044	0.000046
P4	inc1	1	inc-regional	insert	0.003995	0.003624	0.000158	0.000036	0.000037
P4	inc3	1	full	delete	0.002844	0.000422	0.000079	0.000090	0.000022
P4	inc3	1	full	insert	0.001095	0.000441	0.000080	0.000073	0.000022
P4	inc3	1	inc-naive	delete	0.003434	0.003043	0.000168	0.000044	0.000035
P4	inc3	1	inc-naive	insert	0.004012	0.003663	0.000136	0.000043	0.000028
P4	inc3	1	inc-regional	delete	0.001381	0.001074	0.000132	0.000029	0.000029
P4	inc3	1	inc-regional	insert	0.004447	0.004025	0.000162	0.000044	0.000060
P4	inc5	1	full	delete	0.003059	0.000455	0.000078	0.000090	0.000022
P4	inc5	1	full	insert	0.001165	0.000479	0.000089	0.000086	0.000022
P4	inc5	1	inc-naive	delete	0.003226	0.002861	0.000145	0.000044	0.000034
P4	inc5	1	inc-naive	insert	0.004019	0.003577	0.000186	0.000052	0.000034
P4	inc5	1	inc-regional	delete	0.003457	0.002862	0.000285	0.000044	0.000076
P4	inc5	1	inc-regional	insert	0.003244	0.002928	0.000131	0.000030	0.000029
P5	inc1	1	full	delete	0.003760	0.000562	0.000113	0.000308	0.000055
P5	inc1	1	full	insert	0.001565	0.000473	0.000112	0.000290	0.000030
P5	inc1	1	inc-naive	delete	0.005247	0.004785	0.000177	0.000131	0.000032
P5	inc1	1	inc-naive	insert	0.004067	0.003605	0.000178	0.000121	0.000032
P5	inc1	1	inc-regional	delete	0.004948	0.004427	0.000205	0.000037	0.000142
P5	inc1	1	inc-regional	insert	0.003757	0.003326	0.000163	0.000031	0.000112
P5	inc3	1	full	delete	0.002780	0.000558	0.000102	0.000298	0.000029
P5	inc3	1	full	insert	0.002159	0.000380	0.000084	0.000282	0.000029
P5	inc3	1	inc-naive	delete	0.005065	0.004615	0.000171	0.000129	0.000031
P5	inc3	1	inc-naive	insert	0.004081	0.003600	0.000168	0.000134	0.000035
P5	inc3	1	inc-regional	delete	0.018701	0.018232	0.000184	0.000037	0.000126
P5	inc3	1	inc-regional	insert	0.004597	0.004191	0.000140	0.000030	0.000110
P5	inc5	1	full	delete	0.003327	0.000434	0.000094	0.000285	0.000029
P5	inc5	1	full	insert	0.002097	0.000394	0.000089	0.000284	0.000030
P5	inc5	1	inc-naive	delete	0.004030	0.003575	0.000151	0.000144	0.000036
P5	inc5	1	inc-naive	insert	0.004690	0.004183	0.000191	0.000136	0.000035
P5	inc5	1	inc-regional	delete	0.004263	0.003827	0.000154	0.000037	0.000122
P5	inc5	1	inc-regional	insert	0.004683	0.004185	0.000176	0.000036	0.000136
P6	inc1	1	full	delete	0.005287	0.001188	0.000182	0.000857	0.000053
P6	inc1	1	full	insert	0.003100	0.001025	0.000213	0.000892	0.000080
P6	inc1	1	inc-naive	delete	0.011721	0.010090	0.000388	0.001022	0.000050
P6	inc1	1	inc-naive	insert	0.011577	0.010556	0.000239	0.000328	0.000311
P6	inc1	1	inc-regional	delete	0.011399	0.010048	0.000343	0.000773	0.000065
P6	inc1	1	inc-regional	insert	0.011637	0.010382	0.000302	0.000456	0.000358
P6	inc3	1	full	delete	0.004995	0.001061	0.000177	0.000822	0.000044
P6	inc3	1	full	insert	0.002644	0.000876	0.000164	0.000739	0.000049
P6	inc3	1	inc-naive	delete	0.011654	0.010177	0.000315	0.000972	0.000055
P6	inc3	1	inc-naive	insert	0.027860	0.026559	0.000378	0.000380	0.000390
P6	inc3	1	inc-regional	delete	0.010835	0.009736	0.000237	0.000688	0.000047
P6	inc3	1	inc-regional	insert	0.009943	0.008664	0.000271	0.000478	0.000377
P6	inc5	1	full	delete	0.004911	0.000993	0.000164	0.000824	0.000046
P6	inc5	1	full	insert	0.002958	0.001017	0.000194	0.000754	0.000049
P6	inc5	1	inc-naive	delete	0.011606	0.010267	0.000262	0.000908	0.000047
P6	inc5	1	inc-naive	insert	0.009479	0.008399	0.000274	0.000274	0.000369
P6	inc5	1	inc-regional	delete	0.011001	0.009328	0.000283	0.001110	0.000114
P6	inc5	1	inc-regional	insert	0.012371	0.010909	0.000345	0.000544	0.000419
P7	inc1	1	full	delete	0.007175	0.002651	0.000282	0.001161	0.000095
P7	inc1	1	full	insert	0.005936	0.003223	0.000316	0.001173	0.000081
P7	inc1	1	inc-naive	delete	0.010996	0.009743	0.000406	0.000642	0.000062
P7	inc1	1	inc-naive	insert	0.011634	0.010679	0.000345	0.000423	0.000054
P7	inc1	1	inc-regional	delete	0.010550	0.009537	0.000304	0.000033	0.000542
P7	inc1	1	inc-regional	insert	0.012037	0.010943	0.000378	0.000039	0.000528
P7	inc3	1	full	delete	0.007667	0.002592	0.000281	0.001120	0.000075
P7	inc3	1	full	insert	0.006275	0.002774	0.000258	0.001139	0.000090
P7	inc3	1	inc-naive	delete	0.010528	0.009458	0.000370	0.000506	0.000063
P7	inc3	1	inc-naive	insert	0.011621	0.010559	0.000369	0.000486	0.000067
P7	inc3	1	inc-regional	delete	0.010388	0.009314	0.000382	0.000038	0.000524
P7	inc3	1	inc-regional	insert	0.011477	0.010528	0.000360	0.000032	0.000431
P7	inc5	1	full	delete	0.007709	0.003101	0.000294	0.001198	0.000073
P7	inc5	1	full	insert	0.005811	0.002835	0.000601	0.001220	0.000089
P7	inc5	1	inc-naive	delete	0.010079	0.008970	0.000333	0.000558	0.000075
P7	inc5	1	inc-naive	insert	0.011132	0.010079	0.000346	0.000478	0.000055
P7	inc5	1	inc-regional	delete	0.010303	0.009138	0.000381	0.000038	0.000618
P7	inc5	1	inc-regional	insert	0.011244	0.010128	0.000421	0.000039	0.000514
P8	inc1	1	full	delete	0.009915	0.005034	0.000318	0.001555	0.000104
P8	inc1	1	full	insert	0.008380	0.005276	0.000275	0.001532	0.000104
P8	inc1	1	inc-naive	delete	0.010901	0.009515	0.000432	0.000743	0.000075
P8	inc1	1	inc-naive	insert	0.011660	0.010251	0.000396	0.000728	0.000105
P8	inc1	1	inc-regional	delete	0.011254	0.009797	0.000502	0.000046	0.000770
P8	inc1	1	inc-regional	insert	0.012279	0.010394	0.000863	0.000040	0.000830
P8	inc3	1	full	delete	0.054451	0.004891	0.000327	0.046054	0.000084
P8	inc3	1	full	insert	0.008792	0.004626	0.000303	0.002340	0.000133
P8	inc3	1	inc-naive	delete	0.005614	0.003952	0.000610	0.000732	0.000183
P8	inc3	1	inc-naive	insert	0.003705	0.002484	0.000444	0.000576	0.000075
P8	inc3	1	inc-regional	delete	0.005539	0.004023	0.000529	0.000031	0.000826
P8	inc3	1	inc-regional	insert	0.003523	0.002329	0.000415	0.000031	0.000623
P8	inc5	1	full	delete	0.010227	0.005249	0.000285	0.001617	0.000093
P8	inc5	1	full	insert	0.007650	0.004530	0.000261	0.001513	0.000099
P8	inc5	1	inc-naive	delete	0.005402	0.002242	0.000433	0.002518	0.000084
P8	inc5	1	inc-naive	insert	0.004227	0.002032	0.000513	0.000844	0.000701
P8	inc5	1	inc-regional	delete	0.004554	0.002061	0.000409	0.001869	0.000089
P8	inc5	1	inc-regional	insert	0.004205	0.001911	0.000480	0.000994	0.000689
P9	inc1	1	full	delete	0.102217	0.003815	0.000398	0.093873	0.000129
P9	inc1	1	full	insert	0.070607	0.003402	0.000335	0.064956	0.000111
P9	inc1	1	inc-naive	delete	0.003615	0.001641	0.000612	0.001124	0.000112
P9	inc1	1	inc-naive	insert	0.004016	0.001576	0.001180	0.000987	0.000126
P9	inc1	1	inc-regional	delete	0.003815	0.001631	0.000701	0.000032	0.001330
P9	inc1	1	inc-regional	insert	0.003015	0.001514	0.000431	0.000033	0.000907
P9	inc3	1	full	delete	0.081985	0.004309	0.000485	0.072885	0.000113
P9	inc3	1	full	insert	0.068464	0.003938	0.000464	0.061665	0.000233
P9	inc3	1	inc-naive	delete	0.004438	0.001670	0.000765	0.001796	0.000068
P9	inc3	1	inc-naive	insert	0.003340	0.001605	0.000442	0.000270	0.000896
P9	inc3	1	inc-regional	delete	0.004265	0.001749	0.000708	0.001598	0.000067
P9	inc3	1	inc-regional	insert	0.004434	0.001628	0.000670	0.000913	0.001067
P9	inc5	1	full	delete	0.090526	0.004231	0.000484	0.081899	0.000117
P9	inc5	1	full	insert	0.067483	0.003192	0.000359	0.061979	0.000105
P9	inc5	1	inc-naive	delete	0.003986	0.001886	0.000546	0.001383	0.000046
P9	inc5	1	inc-naive	insert	0.003198	0.001596	0.000412	0.000225	0.000841
P9	inc5	1	inc-regional	delete	0.003946	0.001739	0.000587	0.001439	0.000051
P9	inc5	1	inc-regional	insert	0.003993	0.001658	0.000493	0.000839	0.000867
P10	inc1	1	full	delete	0.042660	0.036637	0.001537	0.000348	0.000033
P10	inc1	1	full	insert	0.041079	0.037063	0.001401	0.000403	0.000034
P10	inc1	1	inc-naive	delete	0.010078	0.007786	0.001973	0.000150	0.000037
P10	inc1	1	inc-naive	insert	0.007751	0.005433	0.001944	0.000166	0.000045
P10	inc1	1	inc-regional	delete	0.010850	0.008087	0.002380	0.000046	0.000190
P10	inc1	1	inc-regional	insert	0.007598	0.005376	0.001905	0.000031	0.000156
P10	inc3	1	full	delete	0.042932	0.036694	0.001661	0.000353	0.000033
P10	inc3	1	full	insert	0.040865	0.036860	0.001579	0.000330	0.000036
P10	inc3	1	inc-naive	delete	0.008531	0.006199	0.001999	0.000150	0.000038
P10	inc3	1	inc-naive	insert	0.006965	0.005081	0.001571	0.000148	0.000037
P10	inc3	1	inc-regional	delete	0.008766	0.006529	0.001916	0.000039	0.000157
P10	inc3	1	inc-regional	insert	0.007840	0.005769	0.001762	0.000031	0.000152
P10	inc5	1	full	delete	0.045991	0.038390	0.001805	0.000453	0.000046
P10	inc5	1	full	insert	0.043895	0.039341	0.001429	0.000363	0.000037
P10	inc5	1	inc-naive	delete	0.016029	0.011194	0.004460	0.000189	0.000048
P10	inc5	1	inc-naive	insert	0.010411	0.006578	0.003500	0.000164	0.000040
P10	inc5	1	inc-regional	delete	0.016881	0.011838	0.004685	0.000040	0.000184
P10	inc5	1	inc-regional	insert	0.010692	0.006278	0.004063	0.000035	0.000175
P11	inc1	1	full	delete	0.052565	0.044358	0.001996	0.000190	0.000029
P11	inc1	1	full	insert	0.045918	0.041159	0.002049	0.000212	0.000029
P11	inc1	1	inc-naive	delete	0.010768	0.008043	0.002481	0.000084	0.000031
P11	inc1	1	inc-naive	insert	0.007332	0.005187	0.001905	0.000081	0.000030
P11	inc1	1	inc-regional	delete	0.008025	0.006220	0.001567	0.000037	0.000079
P11	inc1	1	inc-regional	insert	0.006051	0.004465	0.001354	0.000030	0.000079
P11	inc3	1	full	delete	0.039558	0.034186	0.001399	0.000146	0.000049
P11	inc3	1	full	insert	0.040151	0.036678	0.001190	0.000194	0.000028
P11	inc3	1	inc-naive	delete	0.010075	0.007784	0.002051	0.000086	0.000032
P11	inc3	1	inc-naive	insert	0.006735	0.004959	0.001545	0.000079	0.000029
P11	inc3	1	inc-regional	delete	0.009644	0.007709	0.001691	0.000037	0.000083
P11	inc3	1	inc-regional	insert	0.006645	0.004906	0.001486	0.000031	0.000077
P11	inc5	1	full	delete	0.044714	0.039214	0.001491	0.000180	0.000046
P11	inc5	1	full	insert	0.040302	0.036719	0.001430	0.000178	0.000028
P11	inc5	1	inc-naive	delete	0.012822	0.009658	0.002916	0.000092	0.000031
P11	inc5	1	inc-naive	insert	0.007764	0.005464	0.002069	0.000080	0.000029
P11	inc5	1	inc-regional	delete	0.012530	0.009976	0.002253	0.000047	0.000105
P11	inc5	1	inc-regional	insert	0.007013	0.005086	0.001674	0.000031	0.000082
P12	inc1	1	full	delete	0.485402	0.050623	0.002129	0.407281	0.000882
P12	inc1	1	full	insert	0.489614	0.051700	0.003020	0.409581	0.000825
P12	inc1	1	inc-naive	delete	0.024834	0.007425	0.004837	0.011982	0.000444
P12	inc1	1	inc-naive	insert	0.019254	0.007343	0.003147	0.001575	0.007049
P12	inc1	1	inc-regional	delete	0.026990	0.007264	0.005241	0.013780	0.000558
P12	inc1	1	inc-regional	insert	0.025878	0.009045	0.003566	0.005790	0.007328
P12	inc3	1	full	delete	0.490620	0.057214	0.002902	0.404073	0.000930
P12	inc3	1	full	insert	0.522074	0.058568	0.003497	0.428319	0.000857
P12	inc3	1	inc-naive	delete	0.040566	0.014772	0.006467	0.018567	0.000606
P12	inc3	1	inc-naive	insert	0.032222	0.012501	0.005688	0.002858	0.011009
P12	inc3	1	inc-regional	delete	0.038489	0.014488	0.007633	0.015589	0.000619
P12	inc3	1	inc-regional	insert	0.038313	0.014908	0.005939	0.007779	0.009535
P12	inc5	1	full	delete	0.553511	0.056614	0.003177	0.441951	0.000997
P12	inc5	1	full	insert	0.520720	0.060607	0.003206	0.427346	0.000856
P12	inc5	1	inc-naive	delete	0.043919	0.016901	0.007329	0.018885	0.000660
P12	inc5	1	inc-naive	insert	0.035021	0.014940	0.006214	0.003624	0.010089
P12	inc5	1	inc-regional	delete	0.043123	0.018114	0.006804	0.016988	0.000946
P12	inc5	1	inc-regional	insert	0.041414	0.014341	0.008815	0.009175	0.008923
P13	inc1	1	full	delete	1.399172	0.122820	0.011037	1.224890	0.003877
P13	inc1	1	full	insert	1.370449	0.093490	0.005809	1.215951	0.003534
P13	inc1	1	inc-naive	delete	0.064272	0.020145	0.012299	0.028239	0.003428
P13	inc1	1	inc-naive	insert	0.049471	0.018922	0.008309	0.004019	0.018076
P13	inc1	1	inc-regional	delete	0.077196	0.026786	0.012869	0.033628	0.003728
P13	inc1	1	inc-regional	insert	0.069233	0.019404	0.010500	0.017347	0.021818
P13	inc3	1	full	delete	1.461566	0.101076	0.005993	1.297994	0.004170
P13	inc3	1	full	insert	1.371306	0.105501	0.005860	1.213428	0.003729
P13	inc3	1	inc-naive	delete	0.091506	0.024040	0.012702	0.050688	0.003914
P13	inc3	1	inc-naive	insert	0.061903	0.020445	0.011311	0.008948	0.021036
P13	inc3	1	inc-regional	delete	0.075256	0.022029	0.009918	0.038974	0.004166
P13	inc3	1	inc-regional	insert	0.085834	0.020530	0.015553	0.025555	0.024010
P13	inc5	1	full	delete	1.365152	0.097850	0.005845	1.225439	0.003552
P13	inc5	1	full	insert	1.416219	0.106148	0.005876	1.244546	0.003898
P13	inc5	1	inc-naive	delete	0.134402	0.033583	0.019122	0.076263	0.005238
P13	inc5	1	inc-naive	insert	0.100950	0.024741	0.018254	0.018382	0.039360
P13	inc5	1	inc-regional	delete	0.122867	0.039844	0.016015	0.061696	0.005118
P13	inc5	1	inc-regional	insert	0.104149	0.022678	0.015531	0.028526	0.037185
P14	inc1	1	full	delete	3.459172	0.157450	0.011553	3.057360	0.038878
P14	inc1	1	full	insert	3.237104	0.146436	0.008256	2.831090	0.046778
P14	inc1	1	inc-naive	delete	0.110456	0.025322	0.015417	0.046453	0.023059
P14	inc1	1	inc-naive	insert	0.096858	0.025441	0.015174	0.014660	0.041416
P14	inc1	1	inc-regional	delete	0.132816	0.026244	0.022241	0.059353	0.024793
P14	inc1	1	inc-regional	insert	0.109156	0.026044	0.019845	0.027630	0.035454
P14	inc3	1	full	delete	2.721935	0.147707	0.011098	2.342956	0.032307
P14	inc3	1	full	insert	3.681741	0.155358	0.009297	3.052671	0.045012
P14	inc3	1	inc-naive	delete	0.150757	0.027950	0.020660	0.073511	0.028457
P14	inc3	1	inc-naive	insert	0.122270	0.026970	0.019807	0.031796	0.043511
P14	inc3	1	inc-regional	delete	0.133120	0.027708	0.016763	0.059428	0.029037
P14	inc3	1	inc-regional	insert	0.136885	0.028906	0.020782	0.045813	0.041187
P14	inc5	1	full	delete	2.728413	0.142329	0.008961	2.341056	0.030420
P14	inc5	1	full	insert	3.230007	0.148307	0.008419	2.869059	0.037419
P14	inc5	1	inc-naive	delete	0.193066	0.037505	0.023548	0.102660	0.029175
P14	inc5	1	inc-naive	insert	0.166550	0.029274	0.018761	0.059261	0.059010
P14	inc5	1	inc-regional	delete	0.183602	0.037932	0.021886	0.089702	0.033909
P14	inc5	1	inc-regional	insert	0.147546	0.038329	0.018623	0.047257	0.043115
P15	inc1	1	full	delete	7.169072	0.319239	0.020100	6.202518	0.248775
P15	inc1	1	full	insert	7.141109	0.338278	0.026256	6.164552	0.255540
P15	inc1	1	inc-naive	delete	0.469645	0.063065	0.043512	0.202508	0.160332
P15	inc1	1	inc-naive	insert	0.317961	0.049234	0.042539	0.109718	0.116184
P15	inc1	1	inc-regional	delete	0.425790	0.069935	0.041562	0.168690	0.145385
P15	inc1	1	inc-regional	insert	0.353861	0.054715	0.037754	0.101325	0.159756
P15	inc3	1	full	delete	7.103019	0.338335	0.018916	6.108921	0.225036
P15	inc3	1	full	insert	7.315355	0.343240	0.023111	6.330043	0.284334
P15	inc3	1	inc-naive	delete	0.533096	0.076676	0.036140	0.282937	0.137127
P15	inc3	1	inc-naive	insert	0.395999	0.064076	0.035763	0.172087	0.123739
P15	inc3	1	inc-regional	delete	0.519923	0.089287	0.040885	0.242218	0.147311
P15	inc3	1	inc-regional	insert	0.498278	0.058296	0.037142	0.176579	0.225821
P15	inc5	1	full	delete	7.050255	0.305526	0.015365	6.159967	0.189352
P15	inc5	1	full	insert	9.118941	0.328508	0.023690	8.132329	0.316214
P15	inc5	1	inc-naive	delete	0.631932	0.090790	0.045995	0.351313	0.143581
P15	inc5	1	inc-naive	insert	0.559431	0.066188	0.041471	0.296920	0.154443
P15	inc5	1	inc-regional	delete	0.616131	0.088672	0.041601	0.327796	0.157843
P15	inc5	1	inc-regional	insert	0.740144	0.067340	0.040676	0.339955	0.291608
P16	inc1	1	full	delete	12.737703	0.447817	0.030901	11.356026	0.420133
P16	inc1	1	full	insert	13.043981	0.481192	0.032510	11.702593	0.425188
P16	inc1	1	inc-naive	delete	0.689441	0.091536	0.061743	0.270854	0.265013
P16	inc1	1	inc-naive	insert	0.397330	0.091684	0.046800	0.104680	0.153830
P16	inc1	1	inc-regional	delete	0.628963	0.093036	0.059962	0.218085	0.257576
P16	inc1	1	inc-regional	insert	0.572253	0.105915	0.054630	0.228011	0.183316
P16	inc3	1	full	delete	9.787202	0.472520	0.024902	8.460696	0.324993
P16	inc3	1	full	insert	13.049601	0.498569	0.032058	11.730237	0.433294
P16	inc3	1	inc-naive	delete	0.964189	0.118546	0.062508	0.507184	0.275653
P16	inc3	1	inc-naive	insert	0.732584	0.098405	0.052141	0.372281	0.209283
P16	inc3	1	inc-regional	delete	0.888696	0.116051	0.046848	0.406635	0.318868
P16	inc3	1	inc-regional	insert	0.981138	0.094068	0.055548	0.498587	0.332200
P16	inc5	1	full	delete	9.183869	0.467892	0.042940	7.923055	0.258306
P16	inc5	1	full	insert	13.000995	0.465742	0.035884	11.720960	0.406547
P16	inc5	1	inc-naive	delete	1.125671	0.152210	0.063040	0.646932	0.263198
P16	inc5	1	inc-naive	insert	1.236366	0.104576	0.060959	0.799452	0.270662
P16	inc5	1	inc-regional	delete	1.061068	0.151662	0.061334	0.581255	0.266516
P16	inc5	1	inc-regional	insert	1.411495	0.103109	0.053298	0.820077	0.434261
P17	inc1	1	full	delete	8.156579	0.632183	0.038412	7.046046	0.112205
P17	inc1	1	full	insert	8.167208	0.619505	0.034741	7.048845	0.151157
P17	inc1	1	inc-naive	delete	0.549473	0.137051	0.067572	0.286338	0.058153
P17	inc1	1	inc-naive	insert	0.477102	0.121409	0.070116	0.100325	0.184820
P17	inc1	1	inc-regional	delete	0.492410	0.140476	0.056184	0.237924	0.057453
P17	inc1	1	inc-regional	insert	0.598233	0.120815	0.056932	0.237304	0.182545
P17	inc3	1	full	delete	8.440426	0.675645	0.050381	7.274904	0.097161
P17	inc3	1	full	insert	8.374847	0.632130	0.046161	7.265962	0.131772
P17	inc3	1	inc-naive	delete	0.673930	0.178465	0.072253	0.356440	0.066404
P17	inc3	1	inc-naive	insert	0.602809	0.139647	0.059962	0.193756	0.208801
P17	inc3	1	inc-regional	delete	0.639994	0.180097	0.072962	0.324436	0.062119
P17	inc3	1	inc-regional	insert	0.761018	0.134117	0.061501	0.354846	0.209792
P17	inc5	1	full	delete	8.075688	0.661450	0.039399	6.972117	0.086161
P17	inc5	1	full	insert	8.215463	0.606338	0.048778	7.126631	0.132766
P17	inc5	1	inc-naive	delete	0.768975	0.205748	0.066864	0.424042	0.071951
P17	inc5	1	inc-naive	insert	0.642648	0.142378	0.065900	0.229595	0.204128
P17	inc5	1	inc-regional	delete	0.646477	0.196998	0.063170	0.319464	0.066480
P17	inc5	1	inc-regional	insert	0.815113	0.134570	0.075855	0.410683	0.193265
P18	inc1	1	full	delete	17.536394	0.830715	0.058335	15.593200	0.371983
P18	inc1	1	full	insert	17.734263	0.822323	0.056752	15.849892	0.366466
P18	inc1	1	inc-naive	delete	0.931745	0.187812	0.080171	0.460300	0.202770
P18	inc1	1	inc-naive	insert	0.680917	0.154694	0.095152	0.167310	0.263017
P18	inc1	1	inc-regional	delete	0.872817	0.197223	0.095980	0.396769	0.182078
P18	inc1	1	inc-regional	insert	0.845878	0.154032	0.091802	0.333655	0.265587
P18	inc3	1	full	delete	17.330965	0.814808	0.058721	15.520740	0.295408
P18	inc3	1	full	insert	17.570354	0.856366	0.074418	15.711959	0.354373
P18	inc3	1	inc-naive	delete	1.291460	0.266326	0.095744	0.699388	0.229432
P18	inc3	1	inc-naive	insert	0.910219	0.156264	0.077321	0.379201	0.296489
P18	inc3	1	inc-regional	delete	1.097873	0.252863	0.087154	0.568729	0.188589
P18	inc3	1	inc-regional	insert	1.267325	0.157471	0.088188	0.686326	0.334067
P18	inc5	1	full	delete	13.074474	0.847169	0.079700	11.218660	0.264697
P18	inc5	1	full	insert	17.616912	0.884459	0.068658	15.764872	0.355143
P18	inc5	1	inc-naive	delete	1.371156	0.295751	0.085019	0.793221	0.196710
P18	inc5	1	inc-naive	insert	1.081222	0.159380	0.091239	0.559712	0.269807
P18	inc5	1	inc-regional	delete	1.265376	0.295356	0.085944	0.662179	0.221392
P18	inc5	1	inc-regional	insert	1.436735	0.158724	0.080887	0.804079	0.391693
P19	inc1	1	full	delete	26.942615	1.121456	0.093532	24.234795	0.578404
P19	inc1	1	full	insert	28.186758	1.142754	0.089643	25.479969	0.805987
P19	inc1	1	inc-naive	delete	1.651324	0.352235	0.116040	0.830834	0.351653
P19	inc1	1	inc-naive	insert	1.345499	0.206452	0.094558	0.651813	0.391072
P19	inc1	1	inc-regional	delete	1.650478	0.342895	0.122102	0.837239	0.347570
P19	inc1	1	inc-regional	insert	1.857964	0.211277	0.100295	1.115493	0.429860
P19	inc3	1	full	delete	26.865512	1.130835	0.092810	24.154782	0.474173
P19	inc3	1	full	insert	28.312409	1.201688	0.092234	25.482301	0.803427
P19	inc3	1	inc-naive	delete	2.165645	0.473176	0.117672	1.203698	0.370536
P19	inc3	1	inc-naive	insert	2.022775	0.200894	0.106251	1.223960	0.490206
P19	inc3	1	inc-regional	delete	2.218036	0.472519	0.122314	1.211285	0.411122
P19	inc3	1	inc-regional	insert	2.791812	0.206032	0.119485	1.861776	0.601723
P19	inc5	1	full	delete	26.789002	1.149953	0.089967	24.145886	0.457554
P19	inc5	1	full	insert	27.799282	1.122851	0.094011	25.097082	0.859970
P19	inc5	1	inc-naive	delete	2.508933	0.544623	0.135287	1.417253	0.411212
P19	inc5	1	inc-naive	insert	2.136770	0.209582	0.119742	1.319062	0.486300
P19	inc5	1	inc-regional	delete	2.219626	0.525545	0.118339	1.197120	0.378161
P19	inc5	1	inc-regional	insert	2.807880	0.197135	0.099344	1.697368	0.811194
P20	inc1	1	full	delete	5.186845	1.182836	0.117415	3.473524	0.018564
P20	inc1	1	full	insert	7.802048	1.068036	0.099332	6.184789	0.025576
P20	inc1	1	inc-naive	delete	0.863327	0.250414	0.124318	0.472394	0.015174
P20	inc1	1	inc-naive	insert	0.799096	0.248738	0.122662	0.058110	0.368682
P20	inc1	1	inc-regional	delete	0.969091	0.256234	0.130662	0.560344	0.020913
P20	inc1	1	inc-regional	insert	1.075608	0.256124	0.124074	0.296078	0.398121
P20	inc3	1	full	delete	6.260554	1.461170	0.148350	4.041480	0.027965
P20	inc3	1	full	insert	5.406037	1.140528	0.115020	3.647710	0.023698
P20	inc3	1	inc-naive	delete	0.842898	0.265720	0.123814	0.435989	0.016297
P20	inc3	1	inc-naive	insert	0.887794	0.273597	0.130923	0.065606	0.416871
P20	inc3	1	inc-regional	delete	0.960495	0.249595	0.130853	0.557581	0.021319
P20	inc3	1	inc-regional	insert	1.602537	0.283960	0.163417	0.552075	0.601159
P20	inc5	1	full	delete	6.704749	1.381597	0.127937	4.668170	0.036152
P20	inc5	1	full	insert	8.838387	1.282053	0.147300	6.857395	0.034843
P20	inc5	1	inc-naive	delete	1.240967	0.305433	0.190358	0.709093	0.034050
P20	inc5	1	inc-naive	insert	0.994805	0.274739	0.158600	0.074834	0.485826
P20	inc5	1	inc-regional	delete	1.082132	0.274989	0.147839	0.633436	0.024595
P20	inc5	1	inc-regional	insert	1.399132	0.307464	0.167960	0.380552	0.541096
```

## Notes
- `P2` has no deltas and is skipped by the runner.
- Always run with `--det-opt` for consistency with prior measurements.
- Always use the repo build binary via `PATH=.../build/src:$PATH`.

## Related commits
- (no git history yet; uncommitted/new file)


## Experiment: 2026-01-23 (P20, --profile-wmc, in-place probResult updates)

### WMC breakdown (insert turn; last WMC entry per delta)

```
Delta=inc1
  inc-naive:   total_ms=7.4447  output_loop_ms=6.0064  classify=0.1980  lookup=1.6492  write=1.9640  node_wmc=0.1019  weight_apply_ms=0.0000 (calls=0)
  inc-regional:total_ms=13.2870 output_loop_ms=12.0250 classify=0.6120  lookup=1.7680  write=1.5070  node_wmc=0.2210  weight_apply_ms=0.0030 (calls=11)
  full:        total_ms=25.3612 node_wmc=9.1776 nodes=104104

Delta=inc3
  inc-naive:   total_ms=7.4397  output_loop_ms=6.2704  classify=0.2025  lookup=1.4527  write=2.3133  node_wmc=0.1817  weight_apply_ms=0.0000 (calls=0)
  inc-regional:total_ms=13.7050 output_loop_ms=12.4020 classify=0.7970  lookup=1.6160  write=1.5120  node_wmc=0.3940  weight_apply_ms=0.0040 (calls=16)
  full:        total_ms=21.2527 node_wmc=7.6325 nodes=104104

Delta=inc5
  inc-naive:   total_ms=8.3664  output_loop_ms=6.8355  classify=0.2367  lookup=1.8455  write=2.3023  node_wmc=0.1961  weight_apply_ms=0.0000 (calls=0)
  inc-regional:total_ms=14.8320 output_loop_ms=13.5050 classify=0.9780  lookup=1.8010  write=1.5980  node_wmc=0.7720  weight_apply_ms=0.0100 (calls=26)
  full:        total_ms=26.0190 node_wmc=8.8294 nodes=104104
```

### Notes
- These deltas had no evidence; `dep_graph_ms`, `evidence_build_ms`, and `node_make_and_ms` are all zero.
- `output_loop_ms` is inclusive of all per-output work; the sub-timers (classify/lookup/write/node_wmc/weight_apply) are
  point measurements and **do not sum to output_loop_ms**.
- **Why inc-regional slower than inc-naive (WMC stage):**
  - extra per-output bookkeeping to decide region/delta scope (higher `classify`),
  - repeated calibrated/original weight toggles (`weight_apply_calls` > 0),
  - larger output_loop overall despite similar recompute counts.
- **Why full slower than both incrementals (WMC stage):**
  - full iterates over ~104k outputs vs ~11k in incremental view;
  - `node_wmc_compute_ms` is only part of the total; the rest is overhead not currently broken out
    (e.g., per-output bookkeeping, map IO, and other loop-level work).
- Unknown: the remaining full WMC overhead beyond `node_wmc_compute_ms` is not yet attributed to a single substage.
