# Final Incremental Evaluation Log (2026-01-19)

This file records the **latest full P1–P20 incremental runs**, including
generation seeds, strengthen defaults, delta defaults, and run outcomes.
Use this as the canonical snapshot; older eval notes remain in
`README.eval.inc.md` for history.

## Dataset A: `side_channel_inc_strengthen_p1_20`

**Location**
`/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc_strengthen_p1_20`

**Commands used**
```bash
# generate
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_20 \
  generate --cases 1-20 --rule-set full --cleanup

# strengthen (defaults at the time below)
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_20 \
  strengthen --cases 1-20

# delta (default change spec = 0.1% / 0.3% / 0.5%)
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_20 \
  delta --cases 1-20 --sets 1

# compile + run
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_20 \
  compile --cases 1-20 --timeout 600 --jobs 4
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_20 \
  run --cases 1-20 --delta-labels inc1,inc3,inc5 --delta-samples 1 \
  --timeout 1200 --run-arg=--det-opt
```

**Generate seeds (from `operation_inc.log`)**
```
P1=1768803988  P3=1768803990  P4=1768803991  P5=1768803992
P6=1768803993  P7=1768803994  P8=1768803995  P9=1768803996
P10=1768803997 P11=1768803999 P12=1768804000 P13=1768804001
P14=1768804002 P15=1768804004 P16=1768804006 P17=1768804009
P18=1768804012 P19=1768804016 P20=1768804021
```
Notes: P2 source directory missing; no seed recorded.

**Strengthen defaults used**
- P1–P14: `augment_ratio=1.0`
- P15: `augment_ratio=0.5`
- P16–P20: `augment_ratio=0.2`
- `augment_seed=0` (default)

**Delta defaults used**
- `inc1=0.001`, `inc3=0.003`, `inc5=0.005` (0.1% / 0.3% / 0.5%)
- `sets=1`
- `--seed` not provided (time-based RNG inside script, not reproducible unless rerun with a fixed seed)

**Run outcomes**
- P1–P16: all 3 deltas OK (inc vs full match).
- P17–P20: superseded by Dataset B (see below); Dataset A results omitted here.

### Run Results: side_channel_inc_strengthen_p1_20

| Case | Delta | Inc (s) | Full (s) | Speedup | Status | Mismatches | Max |Δ| |
| --- | --- | ---: | ---: | ---: | --- | ---: | ---: |
| P1 | inc1 | 0.213 | 0.101 | 0.48x | OK | - | - |
| P1 | inc3 | 0.197 | 0.101 | 0.51x | OK | - | - |
| P1 | inc5 | 0.196 | 0.099 | 0.50x | OK | - | - |
| P3 | inc1 | 0.187 | 0.091 | 0.49x | OK | - | - |
| P3 | inc3 | 0.197 | 0.090 | 0.46x | OK | - | - |
| P3 | inc5 | 0.192 | 0.098 | 0.51x | OK | - | - |
| P4 | inc1 | 0.019 | 0.014 | 0.72x | OK | - | - |
| P4 | inc3 | 0.016 | 0.014 | 0.89x | OK | - | - |
| P4 | inc5 | 0.016 | 0.016 | 0.99x | OK | - | - |
| P5 | inc1 | 0.022 | 0.015 | 0.68x | OK | - | - |
| P5 | inc3 | 0.024 | 0.018 | 0.73x | OK | - | - |
| P5 | inc5 | 0.023 | 0.017 | 0.77x | OK | - | - |
| P6 | inc1 | 0.058 | 0.027 | 0.47x | OK | - | - |
| P6 | inc3 | 0.041 | 0.027 | 0.66x | OK | - | - |
| P6 | inc5 | 0.039 | 0.030 | 0.79x | OK | - | - |
| P7 | inc1 | 0.045 | 0.033 | 0.74x | OK | - | - |
| P7 | inc3 | 0.043 | 0.044 | 1.01x | OK | - | - |
| P7 | inc5 | 0.049 | 0.035 | 0.71x | OK | - | - |
| P8 | inc1 | 0.050 | 0.046 | 0.93x | OK | - | - |
| P8 | inc3 | 0.056 | 0.047 | 0.83x | OK | - | - |
| P8 | inc5 | 0.058 | 0.060 | 1.03x | OK | - | - |
| P9 | inc1 | 0.150 | 0.245 | 1.63x | OK | - | - |
| P9 | inc3 | 0.160 | 0.270 | 1.69x | OK | - | - |
| P9 | inc5 | 0.166 | 0.259 | 1.56x | OK | - | - |
| P10 | inc1 | 0.101 | 0.149 | 1.48x | OK | - | - |
| P10 | inc3 | 0.115 | 0.159 | 1.39x | OK | - | - |
| P10 | inc5 | 0.135 | 0.170 | 1.26x | OK | - | - |
| P11 | inc1 | 0.079 | 0.146 | 1.84x | OK | - | - |
| P11 | inc3 | 0.111 | 0.159 | 1.43x | OK | - | - |
| P11 | inc5 | 0.133 | 0.168 | 1.27x | OK | - | - |
| P12 | inc1 | 0.590 | 1.452 | 2.46x | OK | - | - |
| P12 | inc3 | 0.636 | 1.474 | 2.32x | OK | - | - |
| P12 | inc5 | 0.621 | 1.553 | 2.50x | OK | - | - |
| P13 | inc1 | 1.676 | 4.191 | 2.50x | OK | - | - |
| P13 | inc3 | 1.746 | 4.481 | 2.57x | OK | - | - |
| P13 | inc5 | 1.952 | 4.307 | 2.21x | OK | - | - |
| P14 | inc1 | 3.782 | 10.165 | 2.69x | OK | - | - |
| P14 | inc3 | 4.165 | 9.573 | 2.30x | OK | - | - |
| P14 | inc5 | 4.250 | 9.318 | 2.19x | OK | - | - |
| P15 | inc1 | 8.879 | 21.066 | 2.37x | OK | - | - |
| P15 | inc3 | 9.725 | 20.638 | 2.12x | OK | - | - |
| P15 | inc5 | 10.856 | 20.750 | 1.91x | OK | - | - |
| P16 | inc1 | 15.415 | 33.363 | 2.16x | OK | - | - |
| P16 | inc3 | 17.110 | 33.638 | 1.97x | OK | - | - |
| P16 | inc5 | 20.361 | 31.388 | 1.54x | OK | - | - |

### Supplement: inc-analyze regional vs delta-reachable (P1-P10, 2026-01-20)

- Re-ran P1-P10 as requested (actual cases: P1, P3-P10; P2 missing `compute.souffle.dl`).
- Added parsing for `inc-analyze` output to extract `regionNodes` and `drNodes`
  (regional work size vs delta-reachable).

**Summary**
- P1-P10: full / inc-naive / inc-regional all correct.
- inc-regional shows clear shrinkage in some cases (notably P8); in others it
  matches delta-reachable (ratio=1.0).

**regionNodes / drNodes (delta-reachable)**
| Case | Delta | regionNodes | drNodes | ratio |
| --- | --- | ---: | ---: | ---: |
| P1 | inc1 | 85 | 85 | 1.000 |
| P1 | inc3 | 85 | 85 | 1.000 |
| P1 | inc5 | 85 | 85 | 1.000 |
| P5 | inc1 | 3 | 8 | 0.375 |
| P5 | inc3 | 3 | 8 | 0.375 |
| P5 | inc5 | 3 | 8 | 0.375 |
| P6 | inc1 | 2 | 2 | 1.000 |
| P6 | inc3 | 2 | 2 | 1.000 |
| P6 | inc5 | 2 | 2 | 1.000 |
| P7 | inc1 | 6 | 6 | 1.000 |
| P7 | inc3 | 15 | 27 | 0.556 |
| P7 | inc5 | 15 | 27 | 0.556 |
| P8 | inc1 | 4 | 36 | 0.111 |
| P8 | inc3 | 11 | 65 | 0.169 |
| P8 | inc5 | 60 | 130 | 0.462 |
| P9 | inc1 | 14 | 14 | 1.000 |
| P9 | inc3 | 32 | 32 | 1.000 |
| P9 | inc5 | 32 | 32 | 1.000 |

**Notes**
- P3 / P4 / P10: `inc_analyze` produced no output because the insert turn had
  no insertion delta (`deltaInsertedEdges/Nodes` empty). `inc-regional` skips
  insertion analysis, so no region/dr data is available.
- For cases with output, `inc-regional` reduces the update region in P5/P7/P8;
  others are approximately delta-reachable (ratio=1).

### Stage Speedups + Delta Ratios (P1–P16, side_channel_inc_strengthen_p1_20)

| Case | Delta | Phase | SEM speedup | FC speedup | ΔEdges | |E| | ΔLiveNodes | LiveNodes |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| P1 | inc1 | del | 0.17x | 2.57x | 0 | 220 | 0 | 3 |
| P1 | inc1 | ins | 7.65x | 2.80x | 0 | 220 | 0 | 3 |
| P1 | inc3 | del | 0.18x | 1.89x | 0 | 220 | 0 | 3 |
| P1 | inc3 | ins | 6.95x | 2.75x | 0 | 220 | 0 | 3 |
| P1 | inc5 | del | 0.18x | 2.42x | 0 | 220 | 0 | 3 |
| P1 | inc5 | ins | 7.33x | 2.73x | 0 | 220 | 0 | 3 |
| P3 | inc1 | del | 0.17x | 2.60x | 0 | 240 | 0 | 3 |
| P3 | inc1 | ins | 3.12x | 2.76x | 0 | 240 | 0 | 3 |
| P3 | inc3 | del | 0.16x | 2.61x | 0 | 240 | 0 | 3 |
| P3 | inc3 | ins | 2.95x | 2.76x | 0 | 240 | 0 | 3 |
| P3 | inc5 | del | 0.18x | 2.98x | 0 | 240 | 0 | 3 |
| P3 | inc5 | ins | 2.27x | 2.30x | 0 | 240 | 0 | 3 |
| P4 | inc1 | del | 0.09x | 2.08x | 0 | 50 | 0 | 2 |
| P4 | inc1 | ins | 0.11x | 2.14x | 0 | 50 | 0 | 2 |
| P4 | inc3 | del | 0.34x | 2.13x | 0 | 50 | 0 | 2 |
| P4 | inc3 | ins | 0.14x | 2.00x | 0 | 50 | 0 | 2 |
| P4 | inc5 | del | 0.39x | 2.63x | 0 | 50 | 0 | 2 |
| P4 | inc5 | ins | 0.11x | 2.47x | 0 | 50 | 0 | 2 |
| P5 | inc1 | del | 0.09x | 1.21x | 1 | 71 | 0 | 339 |
| P5 | inc1 | ins | 0.12x | 2.48x | 1 | 72 | 0 | 339 |
| P5 | inc3 | del | 0.10x | 2.13x | 1 | 71 | 0 | 339 |
| P5 | inc3 | ins | 0.12x | 2.29x | 1 | 72 | 0 | 339 |
| P5 | inc5 | del | 0.09x | 1.27x | 1 | 71 | 0 | 339 |
| P5 | inc5 | ins | 0.15x | 2.78x | 1 | 72 | 0 | 339 |
| P6 | inc1 | del | 0.04x | 0.84x | 1 | 137 | -67 | 1090 |
| P6 | inc1 | ins | 0.10x | 1.41x | 1 | 138 | 67 | 1157 |
| P6 | inc3 | del | 0.11x | 1.01x | 1 | 137 | -67 | 1090 |
| P6 | inc3 | ins | 0.09x | 1.44x | 1 | 138 | 67 | 1157 |
| P6 | inc5 | del | 0.15x | 1.28x | 1 | 137 | -67 | 1090 |
| P6 | inc5 | ins | 0.11x | 1.05x | 1 | 138 | 67 | 1157 |
| P7 | inc1 | del | 0.27x | 1.85x | 1 | 372 | 0 | 1809 |
| P7 | inc1 | ins | 0.24x | 2.11x | 1 | 373 | 0 | 1809 |
| P7 | inc3 | del | 0.30x | 0.55x | 2 | 371 | -50 | 1759 |
| P7 | inc3 | ins | 0.28x | 1.14x | 2 | 373 | 50 | 1809 |
| P7 | inc5 | del | 0.24x | 0.54x | 3 | 370 | -50 | 1759 |
| P7 | inc5 | ins | 0.24x | 1.03x | 3 | 373 | 50 | 1809 |
| P8 | inc1 | del | 0.58x | 2.10x | 1 | 625 | 0 | 2811 |
| P8 | inc1 | ins | 0.52x | 1.90x | 1 | 626 | 0 | 2811 |
| P8 | inc3 | del | 0.46x | 0.41x | 3 | 623 | -33 | 2778 |
| P8 | inc3 | ins | 0.49x | 1.19x | 3 | 626 | 33 | 2811 |
| P8 | inc5 | del | 0.42x | 0.38x | 5 | 621 | -33 | 2778 |
| P8 | inc5 | ins | 0.47x | 1.27x | 5 | 626 | 33 | 2811 |
| P9 | inc1 | del | 0.39x | 31.96x | 1 | 469 | 0 | 782 |
| P9 | inc1 | ins | 0.36x | 40.40x | 1 | 470 | 0 | 782 |
| P9 | inc3 | del | 0.39x | 36.03x | 4 | 466 | -73 | 709 |
| P9 | inc3 | ins | 0.33x | 35.35x | 4 | 470 | 73 | 782 |
| P9 | inc5 | del | 0.34x | 23.01x | 6 | 464 | -75 | 707 |
| P9 | inc5 | ins | 0.31x | 27.34x | 6 | 470 | 75 | 782 |
| P10 | inc1 | del | 2.11x | 1.78x | 12 | 4732 | 0 | 49 |
| P10 | inc1 | ins | 4.07x | 2.05x | 12 | 4744 | 0 | 49 |
| P10 | inc3 | del | 1.81x | 1.16x | 32 | 4712 | -3 | 46 |
| P10 | inc3 | ins | 3.68x | 1.20x | 32 | 4744 | 3 | 49 |
| P10 | inc5 | del | 1.67x | 1.01x | 47 | 4697 | -3 | 46 |
| P10 | inc5 | ins | 3.64x | 1.19x | 47 | 4744 | 3 | 49 |
| P11 | inc1 | del | 4.64x | 2.06x | 7 | 4706 | 0 | 17 |
| P11 | inc1 | ins | 12.31x | 2.40x | 7 | 4713 | 0 | 17 |
| P11 | inc3 | del | 2.03x | 2.26x | 26 | 4687 | 0 | 17 |
| P11 | inc3 | ins | 6.55x | 2.25x | 26 | 4713 | 0 | 17 |
| P11 | inc5 | del | 1.71x | 2.11x | 50 | 4663 | 0 | 17 |
| P11 | inc5 | ins | 5.87x | 2.10x | 50 | 4713 | 0 | 17 |
| P12 | inc1 | del | 2.02x | 22.26x | 10 | 4868 | -225 | 9025 |
| P12 | inc1 | ins | 3.99x | 39.46x | 10 | 4878 | 225 | 9250 |
| P12 | inc3 | del | 2.30x | 13.47x | 27 | 4851 | -315 | 8935 |
| P12 | inc3 | ins | 4.04x | 27.71x | 27 | 4878 | 315 | 9250 |
| P12 | inc5 | del | 1.78x | 11.50x | 49 | 4829 | -614 | 8636 |
| P12 | inc5 | ins | 3.74x | 26.84x | 49 | 4878 | 614 | 9250 |
| P13 | inc1 | del | 2.48x | 21.80x | 30 | 9473 | -3263 | 70704 |
| P13 | inc1 | ins | 4.24x | 35.89x | 30 | 9503 | 3263 | 73967 |
| P13 | inc3 | del | 2.38x | 17.40x | 68 | 9435 | -5448 | 68519 |
| P13 | inc3 | ins | 4.28x | 30.98x | 68 | 9503 | 5448 | 73967 |
| P13 | inc5 | del | 1.99x | 6.80x | 111 | 9392 | -13268 | 60699 |
| P13 | inc5 | ins | 4.10x | 22.69x | 111 | 9503 | 13268 | 73967 |
| P14 | inc1 | del | 5.01x | 19.03x | 27 | 14102 | -3980 | 638473 |
| P14 | inc1 | ins | 6.20x | 30.60x | 27 | 14129 | 3980 | 642453 |
| P14 | inc3 | del | 3.28x | 7.04x | 80 | 14049 | -56146 | 586307 |
| P14 | inc3 | ins | 4.26x | 19.79x | 80 | 14129 | 56146 | 642453 |
| P14 | inc5 | del | 2.61x | 5.64x | 147 | 13982 | -121730 | 520723 |
| P14 | inc5 | ins | 3.90x | 14.86x | 147 | 14129 | 121730 | 642453 |
| P15 | inc1 | del | 4.09x | 7.29x | 75 | 23687 | -410259 | 3385993 |
| P15 | inc1 | ins | 5.12x | 16.67x | 75 | 23762 | 410259 | 3796252 |
| P15 | inc3 | del | 2.60x | 4.51x | 245 | 23517 | -1292254 | 2503998 |
| P15 | inc3 | ins | 4.82x | 9.67x | 245 | 23762 | 1292254 | 3796252 |
| P15 | inc5 | del | 2.27x | 2.98x | 394 | 23368 | -1705713 | 2090539 |
| P15 | inc5 | ins | 3.48x | 6.74x | 394 | 23762 | 1705713 | 3796252 |
| P16 | inc1 | del | 3.04x | 8.02x | 205 | 29861 | -1524012 | 3685758 |
| P16 | inc1 | ins | 4.64x | 9.11x | 205 | 30066 | 1524012 | 5209770 |
| P16 | inc3 | del | 1.82x | 3.92x | 565 | 29501 | -2507632 | 2702138 |
| P16 | inc3 | ins | 5.08x | 6.13x | 565 | 30066 | 2507632 | 5209770 |
| P16 | inc5 | del | 1.37x | 1.85x | 917 | 29149 | -3061672 | 2148098 |
| P16 | inc5 | ins | 4.30x | 3.39x | 915 | 30064 | 3061672 | 5209770 |

## Dataset B: `side_channel_inc_strengthen_p17_20` (P17–P20 only)

**Location**
`/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc_strengthen_p17_20`

**Commands used**
```bash
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p17_20 \
  generate --cases 17-20 --rule-set full --cleanup
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p17_20 \
  strengthen --cases 17-20
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p17_20 \
  delta --cases 17-20 --sets 1
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p17_20 \
  compile --cases 17-20 --timeout 600 --jobs 4
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p17_20 \
  run --cases 17-20 --delta-labels inc1,inc3,inc5 --delta-samples 1 \
  --timeout 1200 --run-arg=--det-opt
```

**Generate seeds (from `operation_inc.log`)**
```
P17=1768806224  P18=1768806228  P19=1768806232  P20=1768806237
```

**Strengthen defaults used**
- P17–P20: `augment_ratio=0.05`
- `augment_seed=0`

**Delta defaults used**
- `inc1=0.001`, `inc3=0.003`, `inc5=0.005`
- `sets=1`
- `--seed` not provided (time-based RNG)

**Run outcomes**
- P17: all OK (inc1/inc3/inc5).
- P18: all OK (inc1/inc3/inc5).
- P19: all OK (inc1/inc3/inc5).
- P20: mismatches for all deltas:
  - inc1: 4 mismatches, max |Δ| = 2.75e-04
  - inc3: 33 mismatches, max |Δ| = 4.52e-01
  - inc5: 35 mismatches, max |Δ| = 7.13e-01

### Run Results: side_channel_inc_strengthen_p17_20 (P17–P20, ratio=0.05)

| Case | Delta | Inc (s) | Full (s) | Speedup | Status | Mismatches | Max |Δ| |
| --- | --- | ---: | ---: | ---: | --- | ---: | ---: |
| P17 | inc1 | 10.272 | 22.937 | 2.23x | OK | - | - |
| P17 | inc3 | 12.979 | 22.238 | 1.71x | OK | - | - |
| P17 | inc5 | 15.296 | 20.946 | 1.37x | OK | - | - |
| P18 | inc1 | 21.034 | 48.409 | 2.30x | OK | - | - |
| P18 | inc3 | 25.515 | 45.397 | 1.78x | OK | - | - |
| P18 | inc5 | 29.214 | 42.144 | 1.44x | OK | - | - |
| P19 | inc1 | 37.241 | 88.680 | 2.38x | OK | - | - |
| P19 | inc3 | 43.710 | 80.532 | 1.84x | OK | - | - |
| P19 | inc5 | 49.480 | 75.417 | 1.52x | OK | - | - |
| P20 | inc1 | 16.846 | 22.560 | 1.34x | mismatch | 4 | 2.75e-04 |
| P20 | inc3 | 17.717 | 24.171 | 1.36x | mismatch | 33 | 4.52e-01 |
| P20 | inc5 | 18.627 | 24.173 | 1.30x | mismatch | 35 | 7.13e-01 |

### Stage Speedups + Delta Ratios (P17–P20, ratio=0.05)

| Case | Delta | Phase | SEM speedup | FC speedup | ΔEdges | |E| | ΔLiveNodes | LiveNodes |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| P17 | inc1 | del | 2.44x | 8.70x | 821 | 34466 | -526262 | 1359826 |
| P17 | inc1 | ins | 5.39x | 13.83x | 819 | 35285 | 526262 | 1886088 |
| P17 | inc3 | del | 1.11x | 2.25x | 2817 | 32470 | -1061858 | 824230 |
| P17 | inc3 | ins | 4.11x | 7.60x | 2815 | 35285 | 1061858 | 1886088 |
| P17 | inc5 | del | 1.08x | 0.80x | 4045 | 31242 | -1240759 | 645329 |
| P17 | inc5 | ins | 3.39x | 7.03x | 4043 | 35285 | 1240759 | 1886088 |
| P18 | inc1 | del | 2.30x | 9.94x | 1264 | 43338 | -1755142 | 3441450 |
| P18 | inc1 | ins | 5.56x | 12.43x | 1264 | 44602 | 1755142 | 5196592 |
| P18 | inc3 | del | 1.27x | 2.38x | 2856 | 41746 | -3398207 | 1798385 |
| P18 | inc3 | ins | 5.55x | 6.32x | 2855 | 44601 | 3398207 | 5196592 |
| P18 | inc5 | del | 1.02x | 1.04x | 4446 | 40156 | -4127543 | 1069049 |
| P18 | inc5 | ins | 3.89x | 5.40x | 4445 | 44601 | 4127543 | 5196592 |
| P19 | inc1 | del | 2.23x | 12.19x | 1086 | 55269 | -2433197 | 8301854 |
| P19 | inc1 | ins | 5.83x | 11.06x | 1086 | 56355 | 2433197 | 10735051 |
| P19 | inc3 | del | 1.36x | 3.53x | 3913 | 52442 | -6878281 | 3856770 |
| P19 | inc3 | ins | 5.41x | 6.01x | 3913 | 56355 | 6878281 | 10735051 |
| P19 | inc5 | del | 0.95x | 1.49x | 5673 | 50682 | -8525623 | 2209428 |
| P19 | inc5 | ins | 4.53x | 4.62x | 5673 | 56355 | 8525623 | 10735051 |
| P20 | inc1 | del | 3.89x | 0.65x | 638 | 50962 | -3990 | 143792 |
| P20 | inc1 | ins | 3.40x | 8.60x | 638 | 51600 | 3526 | 147318 |
| P20 | inc3 | del | 2.53x | 0.59x | 2185 | 49415 | -18721 | 129061 |
| P20 | inc3 | ins | 3.28x | 9.34x | 2184 | 51599 | 18267 | 147328 |
| P20 | inc5 | del | 2.63x | 0.56x | 3312 | 48288 | -25045 | 122737 |
| P20 | inc5 | ins | 2.95x | 6.97x | 3311 | 51599 | 24604 | 147341 |

## Dataset C: `side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0` (P1–P14 subset, 2026-01-20)

**Location**
`/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0`

**Notes**
- P2 source directory missing in SMT dataset; actual cases: P1, P3–P14 (count=13).
- `run --compare-all` compares full vs inc-naive vs inc-regional for each iter (iter2 = insert).

**Commands used**
```bash
# generate
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0 \
  generate --cases 1,3-14 --rule-set full --cleanup --seed 0

# strengthen
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0 \
  strengthen --cases 1,3-14

# delta
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0 \
  delta --cases 1,3-14 --change-spec inc3=0.003,inc6=0.006,inc10=0.01 --sets 1 --seed 0 --cleanup

# compile
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0 \
  compile --cases 1,3-14 --timeout 600 --jobs 4

# run
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0 \
  run --cases 1,3-14 --delta-labels inc3,inc6,inc10 --delta-samples 1 \
  --timeout 1200 --run-arg=--det-opt --compare-all

# collect
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0 collect
```

**Timestamps (from `operation_inc.log`)**
- Compile: 2026-01-20 02:52:37 → 02:56:54
- Run: 2026-01-20 02:57:08 → 02:58:59
- Collect: 2026-01-20 02:59:17
- Rerun (subset after WMC-cache fix; `--det-opt`):
  - P9 inc10: 2026-01-20 04:28:15
  - P13 inc6+inc10: 2026-01-20 04:28:22 → 04:28:42

### Run Results (end-to-end, elapsed_s)

| Case | Delta | inc-naive (s) | inc-regional (s) | full (s) | Status | Mismatches | Max |Δ| |
| --- | --- | ---: | ---: | ---: | --- | ---: | ---: |
| P1 | inc3 | 0.048 | 0.048 | 0.105 | OK | - | 0.00e+00 |
| P1 | inc6 | 0.048 | 0.045 | 0.102 | OK | - | 0.00e+00 |
| P1 | inc10 | 0.195 | 0.202 | 0.101 | OK | - | 0.00e+00 |
| P3 | inc3 | 0.194 | 0.193 | 0.088 | OK | - | 0.00e+00 |
| P3 | inc6 | 0.181 | 0.182 | 0.084 | OK | - | 0.00e+00 |
| P3 | inc10 | 0.185 | 0.187 | 0.094 | OK | - | 0.00e+00 |
| P4 | inc3 | 0.019 | 0.019 | 0.014 | OK | - | 0.00e+00 |
| P4 | inc6 | 0.018 | 0.016 | 0.015 | OK | - | 0.00e+00 |
| P4 | inc10 | 0.030 | 0.016 | 0.014 | OK | - | 0.00e+00 |
| P5 | inc3 | 0.023 | 0.023 | 0.017 | OK | - | 0.00e+00 |
| P5 | inc6 | 0.035 | 0.021 | 0.017 | OK | - | 0.00e+00 |
| P5 | inc10 | 0.026 | 0.026 | 0.018 | OK | - | 0.00e+00 |
| P6 | inc3 | 0.038 | 0.040 | 0.026 | OK | - | 0.00e+00 |
| P6 | inc6 | 0.039 | 0.038 | 0.030 | OK | - | 0.00e+00 |
| P6 | inc10 | 0.060 | 0.043 | 0.028 | OK | - | 0.00e+00 |
| P7 | inc3 | 0.047 | 0.048 | 0.034 | OK | - | 0.00e+00 |
| P7 | inc6 | 0.062 | 0.044 | 0.039 | OK | - | 0.00e+00 |
| P7 | inc10 | 0.052 | 0.050 | 0.038 | OK | - | 0.00e+00 |
| P8 | inc3 | 0.056 | 0.052 | 0.045 | OK | - | 0.00e+00 |
| P8 | inc6 | 0.056 | 0.058 | 0.046 | OK | - | 0.00e+00 |
| P8 | inc10 | 0.061 | 0.060 | 0.049 | OK | - | 0.00e+00 |
| P9 | inc3 | 0.151 | 0.151 | 0.307 | OK | - | 0.00e+00 |
| P9 | inc6 | 0.164 | 0.165 | 0.380 | OK | - | 0.00e+00 |
| P9 | inc10 | 0.154 | 0.160 | 0.297 | OK | - | 0.00e+00 |
| P10 | inc3 | 0.124 | 0.125 | 0.176 | OK | - | 0.00e+00 |
| P10 | inc6 | 0.141 | 0.154 | 0.180 | OK | - | 0.00e+00 |
| P10 | inc10 | 0.169 | 0.165 | 0.208 | OK | - | 0.00e+00 |
| P11 | inc3 | 0.122 | 0.122 | 0.152 | OK | - | 0.00e+00 |
| P11 | inc6 | 0.131 | 0.132 | 0.168 | OK | - | 0.00e+00 |
| P11 | inc10 | 0.156 | 0.154 | 0.194 | OK | - | 0.00e+00 |
| P12 | inc3 | 0.617 | 0.616 | 1.414 | OK | - | 0.00e+00 |
| P12 | inc6 | 0.648 | 0.647 | 1.435 | OK | - | 0.00e+00 |
| P12 | inc10 | 0.680 | 0.668 | 1.509 | OK | - | 0.00e+00 |
| P13 | inc3 | 1.686 | 1.640 | 4.194 | OK | - | 0.00e+00 |
| P13 | inc6 | 1.760 | 1.792 | 4.153 | OK | - | 0.00e+00 |
| P13 | inc10 | 2.088 | 2.108 | 3.850 | OK | - | 0.00e+00 |
| P14 | inc3 | 4.147 | 3.932 | 9.479 | OK | - | 0.00e+00 |
| P14 | inc6 | 4.328 | 4.245 | 9.103 | OK | - | 0.00e+00 |
| P14 | inc10 | 4.746 | 4.649 | 8.781 | OK | - | 0.00e+00 |

### Per-turn Per-stage (seconds; turn2=del, turn3=ins)

| Case | Delta | Mode | Phase | Total (s) | SEM | PRN | FC | WMC |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| P1 | inc3 | full | del | 0.032552 | 0.029104 | 0.000183 | 0.000818 | 0.000028 |
| P1 | inc3 | full | ins | 0.031847 | 0.029970 | 0.000195 | 0.000866 | 0.000032 |
| P1 | inc3 | inc-naive | del | 0.002850 | 0.002091 | 0.000230 | 0.000364 | 0.000043 |
| P1 | inc3 | inc-naive | ins | 0.003961 | 0.003323 | 0.000185 | 0.000290 | 0.000043 |
| P1 | inc3 | inc-regional | del | 0.002926 | 0.002152 | 0.000202 | 0.000035 | 0.000403 |
| P1 | inc3 | inc-regional | ins | 0.003820 | 0.003059 | 0.000219 | 0.000036 | 0.000360 |
| P1 | inc6 | full | del | 0.032002 | 0.028359 | 0.000160 | 0.000809 | 0.000029 |
| P1 | inc6 | full | ins | 0.029624 | 0.027881 | 0.000176 | 0.000788 | 0.000028 |
| P1 | inc6 | inc-naive | del | 0.002961 | 0.002065 | 0.000197 | 0.000535 | 0.000042 |
| P1 | inc6 | inc-naive | ins | 0.003959 | 0.003201 | 0.000221 | 0.000338 | 0.000051 |
| P1 | inc6 | inc-regional | del | 0.002825 | 0.002153 | 0.000190 | 0.000028 | 0.000333 |
| P1 | inc6 | inc-regional | ins | 0.004208 | 0.003476 | 0.000176 | 0.000028 | 0.000394 |
| P1 | inc10 | full | del | 0.031001 | 0.027743 | 0.000161 | 0.000803 | 0.000028 |
| P1 | inc10 | full | ins | 0.031194 | 0.028784 | 0.000217 | 0.000872 | 0.000034 |
| P1 | inc10 | inc-naive | del | 0.149171 | 0.147267 | 0.001395 | 0.000342 | 0.000044 |
| P1 | inc10 | inc-naive | ins | 0.004972 | 0.004195 | 0.000325 | 0.000283 | 0.000042 |
| P1 | inc10 | inc-regional | del | 0.155976 | 0.153802 | 0.001680 | 0.000041 | 0.000321 |
| P1 | inc10 | inc-regional | ins | 0.004647 | 0.003813 | 0.000375 | 0.000030 | 0.000300 |
| P3 | inc3 | full | del | 0.020581 | 0.016496 | 0.000181 | 0.000946 | 0.000030 |
| P3 | inc3 | full | ins | 0.029286 | 0.027282 | 0.000182 | 0.000905 | 0.000029 |
| P3 | inc3 | inc-naive | del | 0.123689 | 0.118623 | 0.004435 | 0.000449 | 0.000047 |
| P3 | inc3 | inc-naive | ins | 0.029299 | 0.026207 | 0.002478 | 0.000412 | 0.000052 |
| P3 | inc3 | inc-regional | del | 0.116161 | 0.111780 | 0.003869 | 0.000039 | 0.000345 |
| P3 | inc3 | inc-regional | ins | 0.030426 | 0.027584 | 0.002319 | 0.000034 | 0.000360 |
| P3 | inc6 | full | del | 0.017755 | 0.014324 | 0.000165 | 0.000856 | 0.000028 |
| P3 | inc6 | full | ins | 0.028781 | 0.026737 | 0.000200 | 0.001043 | 0.000037 |
| P3 | inc6 | inc-naive | del | 0.114725 | 0.110147 | 0.004050 | 0.000353 | 0.000043 |
| P3 | inc6 | inc-naive | ins | 0.027141 | 0.024507 | 0.002090 | 0.000364 | 0.000046 |
| P3 | inc6 | inc-regional | del | 0.115828 | 0.110955 | 0.004311 | 0.000037 | 0.000371 |
| P3 | inc6 | inc-regional | ins | 0.031457 | 0.028266 | 0.002671 | 0.000037 | 0.000353 |
| P3 | inc10 | full | del | 0.024620 | 0.020234 | 0.000181 | 0.000975 | 0.000030 |
| P3 | inc10 | full | ins | 0.030483 | 0.028424 | 0.000190 | 0.000953 | 0.000031 |
| P3 | inc10 | inc-naive | del | 0.127252 | 0.122587 | 0.004018 | 0.000560 | 0.000047 |
| P3 | inc10 | inc-naive | ins | 0.029220 | 0.026574 | 0.002094 | 0.000505 | 0.000045 |
| P3 | inc10 | inc-regional | del | 0.120486 | 0.116193 | 0.003753 | 0.000038 | 0.000354 |
| P3 | inc10 | inc-regional | ins | 0.030586 | 0.027604 | 0.002393 | 0.000036 | 0.000358 |
| P4 | inc3 | full | del | 0.001978 | 0.001323 | 0.000134 | 0.000247 | 0.000021 |
| P4 | inc3 | full | ins | 0.001995 | 0.001421 | 0.000122 | 0.000207 | 0.000021 |
| P4 | inc3 | inc-naive | del | 0.006953 | 0.006639 | 0.000174 | 0.000084 | 0.000032 |
| P4 | inc3 | inc-naive | ins | 0.007108 | 0.006768 | 0.000205 | 0.000101 | 0.000030 |
| P4 | inc3 | inc-regional | del | 0.007388 | 0.007077 | 0.000208 | 0.000044 | 0.000037 |
| P4 | inc3 | inc-regional | ins | 0.007646 | 0.007300 | 0.000234 | 0.000047 | 0.000034 |
| P4 | inc6 | full | del | 0.001844 | 0.001257 | 0.000124 | 0.000236 | 0.000021 |
| P4 | inc6 | full | ins | 0.001923 | 0.001352 | 0.000132 | 0.000204 | 0.000022 |
| P4 | inc6 | inc-naive | del | 0.006756 | 0.006449 | 0.000174 | 0.000083 | 0.000030 |
| P4 | inc6 | inc-naive | ins | 0.006881 | 0.006556 | 0.000194 | 0.000094 | 0.000030 |
| P4 | inc6 | inc-regional | del | 0.007200 | 0.006899 | 0.000200 | 0.000042 | 0.000037 |
| P4 | inc6 | inc-regional | ins | 0.007423 | 0.007092 | 0.000224 | 0.000045 | 0.000035 |
| P4 | inc10 | full | del | 0.001926 | 0.001350 | 0.000136 | 0.000213 | 0.000022 |
| P4 | inc10 | full | ins | 0.001919 | 0.001300 | 0.000138 | 0.000249 | 0.000022 |
| P4 | inc10 | inc-naive | del | 0.019595 | 0.019355 | 0.000112 | 0.000084 | 0.000031 |
| P4 | inc10 | inc-naive | ins | 0.007268 | 0.006938 | 0.000204 | 0.000095 | 0.000030 |
| P4 | inc10 | inc-regional | del | 0.008051 | 0.007738 | 0.000195 | 0.000047 | 0.000037 |
| P4 | inc10 | inc-regional | ins | 0.007738 | 0.007395 | 0.000227 | 0.000048 | 0.000035 |
| P5 | inc3 | full | del | 0.002180 | 0.001539 | 0.000136 | 0.000219 | 0.000020 |
| P5 | inc3 | full | ins | 0.002162 | 0.001470 | 0.000144 | 0.000245 | 0.000021 |
| P5 | inc3 | inc-naive | del | 0.007556 | 0.007163 | 0.000246 | 0.000104 | 0.000042 |
| P5 | inc3 | inc-naive | ins | 0.007427 | 0.007060 | 0.000226 | 0.000102 | 0.000039 |
| P5 | inc3 | inc-regional | del | 0.007715 | 0.007378 | 0.000272 | 0.000045 | 0.000020 |
| P5 | inc3 | inc-regional | ins | 0.007567 | 0.007214 | 0.000268 | 0.000046 | 0.000022 |
| P5 | inc6 | full | del | 0.002137 | 0.001466 | 0.000130 | 0.000244 | 0.000022 |
| P5 | inc6 | full | ins | 0.001991 | 0.001316 | 0.000135 | 0.000246 | 0.000022 |
| P5 | inc6 | inc-naive | del | 0.011275 | 0.010954 | 0.000171 | 0.000105 | 0.000042 |
| P5 | inc6 | inc-naive | ins | 0.007627 | 0.007274 | 0.000229 | 0.000089 | 0.000034 |
| P5 | inc6 | inc-regional | del | 0.008447 | 0.008048 | 0.000319 | 0.000046 | 0.000022 |
| P5 | inc6 | inc-regional | ins | 0.007603 | 0.007191 | 0.000305 | 0.000047 | 0.000022 |
| P5 | inc10 | full | del | 0.002069 | 0.001437 | 0.000138 | 0.000228 | 0.000021 |
| P5 | inc10 | full | ins | 0.002146 | 0.001493 | 0.000140 | 0.000229 | 0.000021 |
| P5 | inc10 | inc-naive | del | 0.007720 | 0.007356 | 0.000244 | 0.000088 | 0.000032 |
| P5 | inc10 | inc-naive | ins | 0.007560 | 0.007165 | 0.000258 | 0.000100 | 0.000039 |
| P5 | inc10 | inc-regional | del | 0.007788 | 0.007421 | 0.000268 | 0.000045 | 0.000020 |
| P5 | inc10 | inc-regional | ins | 0.007664 | 0.007281 | 0.000293 | 0.000046 | 0.000022 |
| P6 | inc3 | full | del | 0.005656 | 0.004958 | 0.000162 | 0.000448 | 0.000025 |
| P6 | inc3 | full | ins | 0.005560 | 0.004878 | 0.000153 | 0.000449 | 0.000026 |
| P6 | inc3 | inc-naive | del | 0.010235 | 0.009740 | 0.000292 | 0.000145 | 0.000043 |
| P6 | inc3 | inc-naive | ins | 0.010436 | 0.009961 | 0.000285 | 0.000141 | 0.000043 |
| P6 | inc3 | inc-regional | del | 0.010500 | 0.010006 | 0.000333 | 0.000046 | 0.000020 |
| P6 | inc3 | inc-regional | ins | 0.011133 | 0.010634 | 0.000419 | 0.000047 | 0.000022 |
| P6 | inc6 | full | del | 0.005623 | 0.004935 | 0.000172 | 0.000442 | 0.000026 |
| P6 | inc6 | full | ins | 0.005811 | 0.005106 | 0.000164 | 0.000480 | 0.000026 |
| P6 | inc6 | inc-naive | del | 0.010080 | 0.009630 | 0.000256 | 0.000149 | 0.000043 |
| P6 | inc6 | inc-naive | ins | 0.010521 | 0.010053 | 0.000275 | 0.000150 | 0.000043 |
| P6 | inc6 | inc-regional | del | 0.010192 | 0.009755 | 0.000372 | 0.000045 | 0.000020 |
| P6 | inc6 | inc-regional | ins | 0.010672 | 0.010255 | 0.000397 | 0.000047 | 0.000022 |
| P6 | inc10 | full | del | 0.005572 | 0.004870 | 0.000162 | 0.000451 | 0.000026 |
| P6 | inc10 | full | ins | 0.005866 | 0.005155 | 0.000157 | 0.000494 | 0.000026 |
| P6 | inc10 | inc-naive | del | 0.010317 | 0.009855 | 0.000273 | 0.000145 | 0.000043 |
| P6 | inc10 | inc-naive | ins | 0.048681 | 0.048222 | 0.000271 | 0.000145 | 0.000043 |
| P6 | inc10 | inc-regional | del | 0.010214 | 0.009754 | 0.000375 | 0.000045 | 0.000020 |
| P6 | inc10 | inc-regional | ins | 0.010993 | 0.010548 | 0.000385 | 0.000047 | 0.000022 |
| P7 | inc3 | full | del | 0.019094 | 0.017001 | 0.000142 | 0.000698 | 0.000021 |
| P7 | inc3 | full | ins | 0.020769 | 0.018967 | 0.000152 | 0.000708 | 0.000022 |
| P7 | inc3 | inc-naive | del | 0.010369 | 0.009667 | 0.000529 | 0.000126 | 0.000042 |
| P7 | inc3 | inc-naive | ins | 0.019624 | 0.019006 | 0.000377 | 0.000119 | 0.000043 |
| P7 | inc3 | inc-regional | del | 0.010332 | 0.009638 | 0.000512 | 0.000045 | 0.000020 |
| P7 | inc3 | inc-regional | ins | 0.010523 | 0.009876 | 0.000572 | 0.000047 | 0.000022 |
| P7 | inc6 | full | del | 0.025306 | 0.023138 | 0.000147 | 0.000720 | 0.000021 |
| P7 | inc6 | full | ins | 0.026741 | 0.024639 | 0.000148 | 0.000727 | 0.000022 |
| P7 | inc6 | inc-naive | del | 0.024386 | 0.023980 | 0.000257 | 0.000109 | 0.000041 |
| P7 | inc6 | inc-naive | ins | 0.010625 | 0.009886 | 0.000566 | 0.000128 | 0.000042 |
| P7 | inc6 | inc-regional | del | 0.010556 | 0.009833 | 0.000632 | 0.000045 | 0.000020 |
| P7 | inc6 | inc-regional | ins | 0.010687 | 0.010086 | 0.000525 | 0.000047 | 0.000022 |
| P7 | inc10 | full | del | 0.024910 | 0.022776 | 0.000139 | 0.000718 | 0.000020 |
| P7 | inc10 | full | ins | 0.025686 | 0.023547 | 0.000150 | 0.000710 | 0.000020 |
| P7 | inc10 | inc-naive | del | 0.010701 | 0.010009 | 0.000500 | 0.000150 | 0.000041 |
| P7 | inc10 | inc-naive | ins | 0.010925 | 0.010227 | 0.000506 | 0.000150 | 0.000042 |
| P7 | inc10 | inc-regional | del | 0.010655 | 0.009977 | 0.000551 | 0.000045 | 0.000020 |
| P7 | inc10 | inc-regional | ins | 0.010754 | 0.010104 | 0.000584 | 0.000047 | 0.000022 |
| P8 | inc3 | full | del | 0.038923 | 0.037461 | 0.000144 | 0.000698 | 0.000020 |
| P8 | inc3 | full | ins | 0.040573 | 0.039076 | 0.000146 | 0.000708 | 0.000020 |
| P8 | inc3 | inc-naive | del | 0.012052 | 0.011385 | 0.000426 | 0.000201 | 0.000039 |
| P8 | inc3 | inc-naive | ins | 0.014161 | 0.013501 | 0.000420 | 0.000201 | 0.000039 |
| P8 | inc3 | inc-regional | del | 0.012246 | 0.011555 | 0.000479 | 0.000051 | 0.000020 |
| P8 | inc3 | inc-regional | ins | 0.012495 | 0.011825 | 0.000514 | 0.000047 | 0.000022 |
| P8 | inc6 | full | del | 0.038500 | 0.036800 | 0.000136 | 0.000699 | 0.000020 |
| P8 | inc6 | full | ins | 0.038542 | 0.037057 | 0.000139 | 0.000698 | 0.000020 |
| P8 | inc6 | inc-naive | del | 0.012173 | 0.011520 | 0.000418 | 0.000196 | 0.000039 |
| P8 | inc6 | inc-naive | ins | 0.012083 | 0.011406 | 0.000445 | 0.000193 | 0.000039 |
| P8 | inc6 | inc-regional | del | 0.012108 | 0.011461 | 0.000531 | 0.000045 | 0.000020 |
| P8 | inc6 | inc-regional | ins | 0.012372 | 0.011742 | 0.000530 | 0.000047 | 0.000022 |
| P8 | inc10 | full | del | 0.038841 | 0.037087 | 0.000134 | 0.000701 | 0.000020 |
| P8 | inc10 | full | ins | 0.038814 | 0.037046 | 0.000133 | 0.000700 | 0.000020 |
| P8 | inc10 | inc-naive | del | 0.012043 | 0.011366 | 0.000431 | 0.000196 | 0.000039 |
| P8 | inc10 | inc-naive | ins | 0.012356 | 0.011689 | 0.000437 | 0.000192 | 0.000038 |
| P8 | inc10 | inc-regional | del | 0.012384 | 0.011701 | 0.000534 | 0.000045 | 0.000020 |
| P8 | inc10 | inc-regional | ins | 0.012624 | 0.011974 | 0.000542 | 0.000047 | 0.000022 |
| P9 | inc3 | full | del | 0.063679 | 0.003607 | 0.000364 | 0.058952 | 0.000108 |
| P9 | inc3 | full | ins | 0.076583 | 0.003452 | 0.000387 | 0.072625 | 0.000111 |
| P9 | inc3 | inc-naive | del | 0.011584 | 0.007918 | 0.000660 | 0.002966 | 0.000040 |
| P9 | inc3 | inc-naive | ins | 0.013507 | 0.009956 | 0.000984 | 0.002463 | 0.000059 |
| P9 | inc3 | inc-regional | del | 0.013011 | 0.008118 | 0.000661 | 0.002976 | 0.000040 |
| P9 | inc3 | inc-regional | ins | 0.013388 | 0.009814 | 0.000968 | 0.002498 | 0.000060 |
| P9 | inc6 | full | del | 0.067566 | 0.003607 | 0.000362 | 0.063488 | 0.000109 |
| P9 | inc6 | full | ins | 0.071451 | 0.003414 | 0.000395 | 0.067530 | 0.000112 |
| P9 | inc6 | inc-naive | del | 0.011828 | 0.007796 | 0.000801 | 0.003190 | 0.000041 |
| P9 | inc6 | inc-naive | ins | 0.013134 | 0.009603 | 0.000893 | 0.002572 | 0.000058 |
| P9 | inc6 | inc-regional | del | 0.013170 | 0.007813 | 0.000811 | 0.003216 | 0.000042 |
| P9 | inc6 | inc-regional | ins | 0.013112 | 0.009589 | 0.000875 | 0.002589 | 0.000059 |
| P9 | inc10 | full | del | 0.072090 | 0.003607 | 0.000363 | 0.067838 | 0.000100 |
| P9 | inc10 | full | ins | 0.070729 | 0.003405 | 0.000397 | 0.066785 | 0.000142 |
| P9 | inc10 | inc-naive | del | 0.015547 | 0.011253 | 0.000795 | 0.003297 | 0.000070 |
| P9 | inc10 | inc-naive | ins | 0.027428 | 0.024417 | 0.000591 | 0.002197 | 0.000081 |
| P9 | inc10 | inc-regional | del | 0.015008 | 0.011067 | 0.000681 | 0.003045 | 0.000086 |
| P9 | inc10 | inc-regional | ins | 0.013392 | 0.009728 | 0.000981 | 0.002467 | 0.000064 |
| P10 | inc3 | full | del | 0.069830 | 0.002809 | 0.000367 | 0.066615 | 0.000034 |
| P10 | inc3 | full | ins | 0.069768 | 0.002866 | 0.000406 | 0.066460 | 0.000034 |
| P10 | inc3 | inc-naive | del | 0.036319 | 0.032943 | 0.000802 | 0.002529 | 0.000040 |
| P10 | inc3 | inc-naive | ins | 0.036326 | 0.032646 | 0.001057 | 0.002578 | 0.000040 |
| P10 | inc3 | inc-regional | del | 0.037164 | 0.032980 | 0.000803 | 0.003327 | 0.000040 |
| P10 | inc3 | inc-regional | ins | 0.037000 | 0.032718 | 0.001073 | 0.003169 | 0.000040 |
| P10 | inc6 | full | del | 0.069187 | 0.002836 | 0.000374 | 0.065976 | 0.000034 |
| P10 | inc6 | full | ins | 0.068845 | 0.002798 | 0.000378 | 0.065634 | 0.000034 |
| P10 | inc6 | inc-naive | del | 0.053006 | 0.050074 | 0.000480 | 0.002412 | 0.000040 |
| P10 | inc6 | inc-naive | ins | 0.037718 | 0.033588 | 0.001508 | 0.002580 | 0.000040 |
| P10 | inc6 | inc-regional | del | 0.037747 | 0.033626 | 0.000788 | 0.003292 | 0.000040 |
| P10 | inc6 | inc-regional | ins | 0.038028 | 0.034695 | 0.000697 | 0.002595 | 0.000040 |
| P10 | inc10 | full | del | 0.071477 | 0.002824 | 0.000383 | 0.068236 | 0.000034 |
| P10 | inc10 | full | ins | 0.070068 | 0.002818 | 0.000381 | 0.066835 | 0.000034 |
| P10 | inc10 | inc-naive | del | 0.036986 | 0.033497 | 0.001106 | 0.002338 | 0.000040 |
| P10 | inc10 | inc-naive | ins | 0.051134 | 0.047685 | 0.000815 | 0.002601 | 0.000040 |
| P10 | inc10 | inc-regional | del | 0.038408 | 0.033702 | 0.001054 | 0.003612 | 0.000040 |
| P10 | inc10 | inc-regional | ins | 0.038159 | 0.034625 | 0.000904 | 0.002592 | 0.000040 |
| P11 | inc3 | full | del | 0.077970 | 0.003503 | 0.000360 | 0.073952 | 0.000061 |
| P11 | inc3 | full | ins | 0.077949 | 0.003363 | 0.000362 | 0.074162 | 0.000062 |
| P11 | inc3 | inc-naive | del | 0.044557 | 0.041492 | 0.000656 | 0.002363 | 0.000039 |
| P11 | inc3 | inc-naive | ins | 0.043521 | 0.040323 | 0.000787 | 0.002366 | 0.000039 |
| P11 | inc3 | inc-regional | del | 0.044838 | 0.041563 | 0.000684 | 0.002413 | 0.000039 |
| P11 | inc3 | inc-regional | ins | 0.043968 | 0.040894 | 0.000667 | 0.002368 | 0.000039 |
| P11 | inc6 | full | del | 0.075415 | 0.003488 | 0.000361 | 0.071498 | 0.000061 |
| P11 | inc6 | full | ins | 0.075414 | 0.003356 | 0.000365 | 0.071632 | 0.000061 |
| P11 | inc6 | inc-naive | del | 0.045034 | 0.041915 | 0.000690 | 0.002390 | 0.000039 |
| P11 | inc6 | inc-naive | ins | 0.045052 | 0.041814 | 0.000810 | 0.002390 | 0.000039 |
| P11 | inc6 | inc-regional | del | 0.045224 | 0.041849 | 0.000758 | 0.002579 | 0.000039 |
| P11 | inc6 | inc-regional | ins | 0.045404 | 0.042267 | 0.000707 | 0.002391 | 0.000039 |
| P11 | inc10 | full | del | 0.074445 | 0.003499 | 0.000360 | 0.070656 | 0.000061 |
| P11 | inc10 | full | ins | 0.074378 | 0.003352 | 0.000363 | 0.070597 | 0.000061 |
| P11 | inc10 | inc-naive | del | 0.046819 | 0.043326 | 0.000746 | 0.002425 | 0.000039 |
| P11 | inc10 | inc-naive | ins | 0.044227 | 0.040932 | 0.000866 | 0.002391 | 0.000039 |
| P11 | inc10 | inc-regional | del | 0.044718 | 0.041081 | 0.000832 | 0.002644 | 0.000039 |
| P11 | inc10 | inc-regional | ins | 0.044521 | 0.041301 | 0.000790 | 0.002392 | 0.000039 |
| P12 | inc3 | full | del | 0.711898 | 0.002652 | 0.000368 | 0.708745 | 0.000101 |
| P12 | inc3 | full | ins | 0.693479 | 0.002497 | 0.000369 | 0.690226 | 0.000100 |
| P12 | inc3 | inc-naive | del | 0.068815 | 0.045805 | 0.000855 | 0.022114 | 0.000040 |
| P12 | inc3 | inc-naive | ins | 0.071568 | 0.048990 | 0.001138 | 0.021401 | 0.000039 |
| P12 | inc3 | inc-regional | del | 0.068699 | 0.045768 | 0.000867 | 0.022024 | 0.000040 |
| P12 | inc3 | inc-regional | ins | 0.071570 | 0.048905 | 0.001245 | 0.021342 | 0.000039 |
| P12 | inc6 | full | del | 0.709290 | 0.002644 | 0.000366 | 0.706165 | 0.000101 |
| P12 | inc6 | full | ins | 0.693610 | 0.002489 | 0.000363 | 0.690466 | 0.000101 |
| P12 | inc6 | inc-naive | del | 0.069700 | 0.046336 | 0.000862 | 0.022458 | 0.000040 |
| P12 | inc6 | inc-naive | ins | 0.072267 | 0.049790 | 0.001136 | 0.021302 | 0.000039 |
| P12 | inc6 | inc-regional | del | 0.069587 | 0.046329 | 0.000856 | 0.022361 | 0.000040 |
| P12 | inc6 | inc-regional | ins | 0.072096 | 0.049759 | 0.001105 | 0.021195 | 0.000039 |
| P12 | inc10 | full | del | 0.692699 | 0.002647 | 0.000364 | 0.689586 | 0.000101 |
| P12 | inc10 | full | ins | 0.719293 | 0.002542 | 0.000366 | 0.716139 | 0.000101 |
| P12 | inc10 | inc-naive | del | 0.070847 | 0.047638 | 0.000859 | 0.022311 | 0.000040 |
| P12 | inc10 | inc-naive | ins | 0.072731 | 0.050177 | 0.001117 | 0.021397 | 0.000039 |
| P12 | inc10 | inc-regional | del | 0.070794 | 0.047594 | 0.000871 | 0.022291 | 0.000040 |
| P12 | inc10 | inc-regional | ins | 0.072703 | 0.050130 | 0.001142 | 0.021392 | 0.000039 |
| P13 | inc3 | full | del | 2.168997 | 0.003008 | 0.000368 | 2.165542 | 0.000080 |
| P13 | inc3 | full | ins | 2.311115 | 0.002492 | 0.000367 | 2.307937 | 0.000081 |
| P13 | inc3 | inc-naive | del | 0.236534 | 0.062771 | 0.001046 | 0.172678 | 0.000040 |
| P13 | inc3 | inc-naive | ins | 0.241332 | 0.065538 | 0.001061 | 0.174693 | 0.000040 |
| P13 | inc3 | inc-regional | del | 0.229404 | 0.062654 | 0.001039 | 0.165671 | 0.000040 |
| P13 | inc3 | inc-regional | ins | 0.232217 | 0.064723 | 0.001055 | 0.166400 | 0.000040 |
| P13 | inc6 | full | del | 2.344818 | 0.003002 | 0.000367 | 2.341342 | 0.000080 |
| P13 | inc6 | full | ins | 2.313783 | 0.002504 | 0.000368 | 2.310597 | 0.000081 |
| P13 | inc6 | inc-naive | del | 0.236075 | 0.058967 | 0.001020 | 0.176048 | 0.000040 |
| P13 | inc6 | inc-naive | ins | 0.245759 | 0.068520 | 0.001166 | 0.176033 | 0.000040 |
| P13 | inc6 | inc-regional | del | 0.230578 | 0.058831 | 0.001013 | 0.170694 | 0.000040 |
| P13 | inc6 | inc-regional | ins | 0.232913 | 0.068597 | 0.001193 | 0.163083 | 0.000040 |
| P13 | inc10 | full | del | 2.279762 | 0.003002 | 0.000366 | 2.276287 | 0.000080 |
| P13 | inc10 | full | ins | 2.169094 | 0.002500 | 0.000368 | 2.165820 | 0.000080 |
| P13 | inc10 | inc-naive | del | 0.300612 | 0.131979 | 0.000823 | 0.167770 | 0.000040 |
| P13 | inc10 | inc-naive | ins | 0.254887 | 0.068354 | 0.001292 | 0.185201 | 0.000040 |
| P13 | inc10 | inc-regional | del | 0.229878 | 0.058924 | 0.000971 | 0.169942 | 0.000040 |
| P13 | inc10 | inc-regional | ins | 0.233537 | 0.066225 | 0.001422 | 0.165850 | 0.000040 |
| P14 | inc3 | full | del | 4.608295 | 0.004019 | 0.000373 | 4.603845 | 0.000058 |
| P14 | inc3 | full | ins | 5.126709 | 0.002849 | 0.000373 | 5.123253 | 0.000058 |
| P14 | inc3 | inc-naive | del | 0.627909 | 0.248655 | 0.001056 | 0.378162 | 0.000036 |
| P14 | inc3 | inc-naive | ins | 0.681083 | 0.303172 | 0.001257 | 0.376617 | 0.000036 |
| P14 | inc3 | inc-regional | del | 0.625341 | 0.248592 | 0.001076 | 0.375637 | 0.000036 |
| P14 | inc3 | inc-regional | ins | 0.669249 | 0.301554 | 0.001251 | 0.366408 | 0.000036 |
| P14 | inc6 | full | del | 4.421395 | 0.004012 | 0.000371 | 4.416946 | 0.000058 |
| P14 | inc6 | full | ins | 5.047992 | 0.002844 | 0.000373 | 5.044537 | 0.000058 |
| P14 | inc6 | inc-naive | del | 0.683345 | 0.290690 | 0.000977 | 0.391642 | 0.000036 |
| P14 | inc6 | inc-naive | ins | 0.679917 | 0.302276 | 0.001240 | 0.376365 | 0.000036 |
| P14 | inc6 | inc-regional | del | 0.664117 | 0.290581 | 0.000967 | 0.372534 | 0.000036 |
| P14 | inc6 | inc-regional | ins | 0.688415 | 0.304661 | 0.001300 | 0.382419 | 0.000036 |
| P14 | inc10 | full | del | 4.016786 | 0.004016 | 0.000371 | 4.012335 | 0.000058 |
| P14 | inc10 | full | ins | 4.729832 | 0.002841 | 0.000373 | 4.726377 | 0.000058 |
| P14 | inc10 | inc-naive | del | 0.682578 | 0.262458 | 0.001031 | 0.419053 | 0.000036 |
| P14 | inc10 | inc-naive | ins | 0.678304 | 0.301621 | 0.001314 | 0.375332 | 0.000036 |
| P14 | inc10 | inc-regional | del | 0.658616 | 0.262407 | 0.001070 | 0.395102 | 0.000036 |
| P14 | inc10 | inc-regional | ins | 0.678441 | 0.299662 | 0.001516 | 0.377227 | 0.000036 |

### inc-regional insert: regionNodes / drNodes / ratio (from `inc-analyze`)

| Case | Delta | regionNodes | drNodes | ratio |
| --- | --- | ---: | ---: | ---: |
| P1 | inc3 | 85 | 85 | 1.000 |
| P1 | inc6 | 85 | 85 | 1.000 |
| P1 | inc10 | 86 | 86 | 1.000 |
| P3 | inc3 | 85 | 91 | 0.934 |
| P3 | inc6 | 85 | 91 | 0.934 |
| P3 | inc10 | 85 | 91 | 0.934 |
| P4 | inc3 | 3 | 12 | 0.250 |
| P4 | inc6 | 3 | 12 | 0.250 |
| P4 | inc10 | 3 | 12 | 0.250 |
| P5 | inc3 | 5 | 12 | 0.417 |
| P5 | inc6 | 3 | 14 | 0.214 |
| P5 | inc10 | 6 | 17 | 0.353 |
| P6 | inc3 | 2 | 2 | 1.000 |
| P6 | inc6 | 2 | 2 | 1.000 |
| P6 | inc10 | 2 | 2 | 1.000 |
| P7 | inc3 | 6 | 6 | 1.000 |
| P7 | inc6 | 15 | 27 | 0.556 |
| P7 | inc10 | 15 | 27 | 0.556 |
| P8 | inc3 | 5 | 40 | 0.125 |
| P8 | inc6 | 11 | 67 | 0.164 |
| P8 | inc10 | 5 | 44 | 0.114 |
| P9 | inc3 | 32 | 32 | 1.000 |
| P9 | inc6 | 32 | 32 | 1.000 |
| P9 | inc10 | 94 | 111 | 0.847 |
| P10 | inc3 | 20 | 97 | 0.206 |
| P10 | inc6 | 20 | 99 | 0.202 |
| P10 | inc10 | 26 | 102 | 0.255 |
| P11 | inc3 | 24 | 81 | 0.296 |
| P11 | inc6 | 34 | 86 | 0.395 |
| P11 | inc10 | 24 | 80 | 0.300 |
| P12 | inc3 | 42 | 371 | 0.113 |
| P12 | inc6 | 42 | 382 | 0.110 |
| P12 | inc10 | 42 | 391 | 0.107 |
| P13 | inc3 | 89 | 1024 | 0.087 |
| P13 | inc6 | 120 | 1094 | 0.110 |
| P13 | inc10 | 83 | 1213 | 0.068 |
| P14 | inc3 | 120 | 829 | 0.145 |
| P14 | inc6 | 136 | 849 | 0.160 |
| P14 | inc10 | 139 | 873 | 0.159 |

## Notes
- All runs used `--det-opt` in the CLI run phase.
- If deterministic delta seeds are required, rerun `delta` with an explicit `--seed`.
- Logs and per-delta JSON summaries are under each case’s `output/` directory.
