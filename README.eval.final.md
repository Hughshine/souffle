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
- Compile: 2026-01-20 10:58:37 → 2026-01-20 11:02:55
- Run: 2026-01-20 11:03:01 → 2026-01-20 11:04:53
- Collect: 2026-01-20 11:04:56
- inc-analyze refresh (inc-regional only): 2026-01-20 11:29:40 → 11:30:04

### Run Results (end-to-end, elapsed_s)
turn1 turn2 turn3

| Case | Delta | inc-naive (s) | inc-regional (s) | full (s) | Status | Mismatches | Max |Δ| |
| --- | --- | ---: | ---: | ---: | --- | ---: | ---: |
| P1 | inc3 | 0.049 | 0.048 | 0.102 | OK | - | 0.00e+00 |
| P1 | inc6 | 0.049 | 0.047 | 0.110 | OK | - | 0.00e+00 |
| P1 | inc10 | 0.207 | 0.203 | 0.104 | OK | - | 0.00e+00 |
| P3 | inc3 | 0.182 | 0.182 | 0.084 | OK | - | 0.00e+00 |
| P3 | inc6 | 0.182 | 0.189 | 0.086 | OK | - | 0.00e+00 |
| P3 | inc10 | 0.185 | 0.186 | 0.085 | OK | - | 0.00e+00 |
| P4 | inc3 | 0.017 | 0.015 | 0.014 | OK | - | 0.00e+00 |
| P4 | inc6 | 0.025 | 0.015 | 0.015 | OK | - | 0.00e+00 |
| P4 | inc10 | 0.017 | 0.020 | 0.014 | OK | - | 0.00e+00 |
| P5 | inc3 | 0.025 | 0.025 | 0.019 | OK | - | 0.00e+00 |
| P5 | inc6 | 0.021 | 0.023 | 0.018 | OK | - | 0.00e+00 |
| P5 | inc10 | 0.022 | 0.022 | 0.018 | OK | - | 0.00e+00 |
| P6 | inc3 | 0.042 | 0.043 | 0.028 | OK | - | 0.00e+00 |
| P6 | inc6 | 0.040 | 0.042 | 0.027 | OK | - | 0.00e+00 |
| P6 | inc10 | 0.043 | 0.041 | 0.028 | OK | - | 0.00e+00 |
| P7 | inc3 | 0.046 | 0.046 | 0.034 | OK | - | 0.00e+00 |
| P7 | inc6 | 0.046 | 0.044 | 0.036 | OK | - | 0.00e+00 |
| P7 | inc10 | 0.066 | 0.050 | 0.041 | OK | - | 0.00e+00 |
| P8 | inc3 | 0.058 | 0.055 | 0.048 | OK | - | 0.00e+00 |
| P8 | inc6 | 0.061 | 0.075 | 0.052 | OK | - | 0.00e+00 |
| P8 | inc10 | 0.065 | 0.063 | 0.053 | OK | - | 0.00e+00 |
| P9 | inc3 | 0.162 | 0.163 | 0.326 | OK | - | 0.00e+00 |
| P9 | inc6 | 0.165 | 0.161 | 0.434 | OK | - | 0.00e+00 |
| P9 | inc10 | 0.167 | 0.168 | 0.313 | OK | - | 0.00e+00 |
| P10 | inc3 | 0.122 | 0.128 | 0.170 | OK | - | 0.00e+00 |
| P10 | inc6 | 0.143 | 0.143 | 0.187 | OK | - | 0.00e+00 |
| P10 | inc10 | 0.176 | 0.171 | 0.210 | OK | - | 0.00e+00 |
| P11 | inc3 | 0.110 | 0.110 | 0.160 | OK | - | 0.00e+00 |
| P11 | inc6 | 0.141 | 0.136 | 0.184 | OK | - | 0.00e+00 |
| P11 | inc10 | 0.175 | 0.177 | 0.215 | OK | - | 0.00e+00 |
| P12 | inc3 | 0.618 | 0.641 | 1.468 | OK | - | 0.00e+00 |
| P12 | inc6 | 0.687 | 0.688 | 1.497 | OK | - | 0.00e+00 |
| P12 | inc10 | 0.701 | 0.703 | 1.529 | OK | - | 0.00e+00 |
| P13 | inc3 | 1.817 | 1.744 | 4.253 | OK | - | 0.00e+00 |
| P13 | inc6 | 1.914 | 1.903 | 4.321 | OK | - | 0.00e+00 |
| P13 | inc10 | 2.168 | 2.128 | 4.118 | OK | - | 0.00e+00 |
| P14 | inc3 | 4.380 | 4.705 | 9.780 | OK | - | 0.00e+00 |
| P14 | inc6 | 4.433 | 4.369 | 9.682 | OK | - | 0.00e+00 |
| P14 | inc10 | 4.851 | 5.035 | 9.772 | OK | - | 0.00e+00 |
  
### Per-turn Per-stage (seconds; turn2=del, turn3=ins)

| Case | Delta | Mode | Phase | Total (s) | SEM | PRN | FC | WMC |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| P1 | inc3 | full | del | 0.031885 | 0.028713 | 0.000173 | 0.000792 | 0.000028 |
| P1 | inc3 | full | ins | 0.031035 | 0.029367 | 0.000168 | 0.000836 | 0.000028 |
| P1 | inc3 | inc-naive | del | 0.003587 | 0.002866 | 0.000201 | 0.000351 | 0.000044 |
| P1 | inc3 | inc-naive | ins | 0.004791 | 0.004165 | 0.000170 | 0.000286 | 0.000043 |
| P1 | inc3 | inc-regional | del | 0.003688 | 0.002782 | 0.000295 | 0.000038 | 0.000390 |
| P1 | inc3 | inc-regional | ins | 0.004570 | 0.003865 | 0.000237 | 0.000031 | 0.000304 |
| P1 | inc6 | full | del | 0.033003 | 0.029176 | 0.000212 | 0.000925 | 0.000031 |
| P1 | inc6 | full | ins | 0.036745 | 0.034832 | 0.000173 | 0.000847 | 0.000031 |
| P1 | inc6 | inc-naive | del | 0.003481 | 0.002721 | 0.000227 | 0.000360 | 0.000046 |
| P1 | inc6 | inc-naive | ins | 0.004900 | 0.004210 | 0.000225 | 0.000290 | 0.000044 |
| P1 | inc6 | inc-regional | del | 0.003887 | 0.003078 | 0.000271 | 0.000030 | 0.000354 |
| P1 | inc6 | inc-regional | ins | 0.004482 | 0.003835 | 0.000189 | 0.000029 | 0.000302 |
| P1 | inc10 | full | del | 0.032517 | 0.029116 | 0.000162 | 0.000850 | 0.000029 |
| P1 | inc10 | full | ins | 0.031092 | 0.029436 | 0.000166 | 0.000827 | 0.000028 |
| P1 | inc10 | inc-naive | del | 0.158655 | 0.156704 | 0.001449 | 0.000334 | 0.000042 |
| P1 | inc10 | inc-naive | ins | 0.006100 | 0.005291 | 0.000339 | 0.000292 | 0.000050 |
| P1 | inc10 | inc-regional | del | 0.156024 | 0.153578 | 0.001947 | 0.000038 | 0.000315 |
| P1 | inc10 | inc-regional | ins | 0.005536 | 0.004741 | 0.000337 | 0.000030 | 0.000299 |
| P3 | inc3 | full | del | 0.018543 | 0.015327 | 0.000166 | 0.000891 | 0.000029 |
| P3 | inc3 | full | ins | 0.028912 | 0.027081 | 0.000176 | 0.000916 | 0.000028 |
| P3 | inc3 | inc-naive | del | 0.116411 | 0.112381 | 0.003507 | 0.000357 | 0.000042 |
| P3 | inc3 | inc-naive | ins | 0.028070 | 0.025469 | 0.002093 | 0.000339 | 0.000041 |
| P3 | inc3 | inc-regional | del | 0.116744 | 0.112249 | 0.003991 | 0.000039 | 0.000335 |
| P3 | inc3 | inc-regional | ins | 0.027835 | 0.025289 | 0.002051 | 0.000030 | 0.000337 |
| P3 | inc6 | full | del | 0.019896 | 0.015003 | 0.000174 | 0.000913 | 0.000030 |
| P3 | inc6 | full | ins | 0.029423 | 0.026981 | 0.000138 | 0.000899 | 0.000028 |
| P3 | inc6 | inc-naive | del | 0.116902 | 0.112770 | 0.003623 | 0.000343 | 0.000041 |
| P3 | inc6 | inc-naive | ins | 0.027962 | 0.025418 | 0.002054 | 0.000319 | 0.000045 |
| P3 | inc6 | inc-regional | del | 0.117637 | 0.113441 | 0.003672 | 0.000031 | 0.000351 |
| P3 | inc6 | inc-regional | ins | 0.033302 | 0.030525 | 0.002279 | 0.000031 | 0.000336 |
| P3 | inc10 | full | del | 0.018740 | 0.015129 | 0.000174 | 0.000893 | 0.000028 |
| P3 | inc10 | full | ins | 0.028248 | 0.026432 | 0.000174 | 0.000911 | 0.000029 |
| P3 | inc10 | inc-naive | del | 0.116454 | 0.112127 | 0.003840 | 0.000320 | 0.000040 |
| P3 | inc10 | inc-naive | ins | 0.031171 | 0.028588 | 0.002097 | 0.000319 | 0.000041 |
| P3 | inc10 | inc-regional | del | 0.117804 | 0.113321 | 0.003969 | 0.000038 | 0.000333 |
| P3 | inc10 | inc-regional | ins | 0.028148 | 0.025095 | 0.002540 | 0.000030 | 0.000353 |
| P4 | inc3 | full | del | 0.002601 | 0.000421 | 0.000078 | 0.000094 | 0.000023 |
| P4 | inc3 | full | ins | 0.001052 | 0.000411 | 0.000079 | 0.000090 | 0.000023 |
| P4 | inc3 | inc-naive | del | 0.002248 | 0.001855 | 0.000172 | 0.000045 | 0.000029 |
| P4 | inc3 | inc-naive | ins | 0.004055 | 0.003763 | 0.000109 | 0.000037 | 0.000024 |
| P4 | inc3 | inc-regional | del | 0.001152 | 0.000830 | 0.000136 | 0.000030 | 0.000029 |
| P4 | inc3 | inc-regional | ins | 0.005003 | 0.004359 | 0.000428 | 0.000030 | 0.000052 |
| P4 | inc6 | full | del | 0.003135 | 0.000442 | 0.000081 | 0.000092 | 0.000023 |
| P4 | inc6 | full | ins | 0.001209 | 0.000438 | 0.000082 | 0.000076 | 0.000023 |
| P4 | inc6 | inc-naive | del | 0.001684 | 0.001355 | 0.000140 | 0.000041 | 0.000024 |
| P4 | inc6 | inc-naive | ins | 0.004690 | 0.004292 | 0.000179 | 0.000045 | 0.000029 |
| P4 | inc6 | inc-regional | del | 0.001954 | 0.001523 | 0.000193 | 0.000042 | 0.000037 |
| P4 | inc6 | inc-regional | ins | 0.003913 | 0.003591 | 0.000130 | 0.000031 | 0.000031 |
| P4 | inc10 | full | del | 0.002726 | 0.000472 | 0.000082 | 0.000096 | 0.000023 |
| P4 | inc10 | full | ins | 0.001074 | 0.000420 | 0.000084 | 0.000092 | 0.000023 |
| P4 | inc10 | inc-naive | del | 0.001501 | 0.001193 | 0.000122 | 0.000036 | 0.000024 |
| P4 | inc10 | inc-naive | ins | 0.004644 | 0.004243 | 0.000135 | 0.000042 | 0.000077 |
| P4 | inc10 | inc-regional | del | 0.005075 | 0.003093 | 0.000229 | 0.001409 | 0.000048 |
| P4 | inc10 | inc-regional | ins | 0.002846 | 0.002435 | 0.000166 | 0.000052 | 0.000038 |
| P5 | inc3 | full | del | 0.003343 | 0.000453 | 0.000093 | 0.000294 | 0.000030 |
| P5 | inc3 | full | ins | 0.001590 | 0.000425 | 0.000097 | 0.000365 | 0.000030 |
| P5 | inc3 | inc-naive | del | 0.006791 | 0.005444 | 0.000211 | 0.000945 | 0.000043 |
| P5 | inc3 | inc-naive | ins | 0.003984 | 0.003226 | 0.000195 | 0.000376 | 0.000039 |
| P5 | inc3 | inc-regional | del | 0.006625 | 0.005648 | 0.000177 | 0.000635 | 0.000034 |
| P5 | inc3 | inc-regional | ins | 0.004110 | 0.003468 | 0.000145 | 0.000336 | 0.000033 |
| P5 | inc6 | full | del | 0.003347 | 0.000584 | 0.000097 | 0.000292 | 0.000030 |
| P5 | inc6 | full | ins | 0.001364 | 0.000432 | 0.000097 | 0.000284 | 0.000030 |
| P5 | inc6 | inc-naive | del | 0.004979 | 0.003647 | 0.000243 | 0.000907 | 0.000033 |
| P5 | inc6 | inc-naive | ins | 0.003656 | 0.003078 | 0.000140 | 0.000284 | 0.000032 |
| P5 | inc6 | inc-regional | del | 0.004480 | 0.003539 | 0.000147 | 0.000635 | 0.000033 |
| P5 | inc6 | inc-regional | ins | 0.004433 | 0.003599 | 0.000256 | 0.000391 | 0.000032 |
| P5 | inc10 | full | del | 0.003036 | 0.000450 | 0.000097 | 0.000291 | 0.000029 |
| P5 | inc10 | full | ins | 0.001386 | 0.000448 | 0.000094 | 0.000302 | 0.000030 |
| P5 | inc10 | inc-naive | del | 0.004843 | 0.003702 | 0.000225 | 0.000740 | 0.000034 |
| P5 | inc10 | inc-naive | ins | 0.003926 | 0.003336 | 0.000120 | 0.000306 | 0.000033 |
| P5 | inc10 | inc-regional | del | 0.005058 | 0.003820 | 0.000226 | 0.000830 | 0.000036 |
| P5 | inc10 | inc-regional | ins | 0.003823 | 0.003206 | 0.000168 | 0.000288 | 0.000033 |
| P6 | inc3 | full | del | 0.004096 | 0.000986 | 0.000157 | 0.000693 | 0.000047 |
| P6 | inc3 | full | ins | 0.002957 | 0.000923 | 0.000155 | 0.000904 | 0.000051 |
| P6 | inc3 | inc-naive | del | 0.010898 | 0.009884 | 0.000277 | 0.000552 | 0.000035 |
| P6 | inc3 | inc-naive | ins | 0.009825 | 0.008239 | 0.000331 | 0.000856 | 0.000038 |
| P6 | inc3 | inc-regional | del | 0.011048 | 0.010049 | 0.000278 | 0.000538 | 0.000035 |
| P6 | inc3 | inc-regional | ins | 0.009388 | 0.008068 | 0.000277 | 0.000851 | 0.000037 |
| P6 | inc6 | full | del | 0.004126 | 0.000990 | 0.000183 | 0.000680 | 0.000046 |
| P6 | inc6 | full | ins | 0.002561 | 0.000923 | 0.000168 | 0.000749 | 0.000050 |
| P6 | inc6 | inc-naive | del | 0.011057 | 0.010052 | 0.000293 | 0.000550 | 0.000029 |
| P6 | inc6 | inc-naive | ins | 0.009202 | 0.008247 | 0.000200 | 0.000598 | 0.000032 |
| P6 | inc6 | inc-regional | del | 0.010767 | 0.009931 | 0.000219 | 0.000454 | 0.000029 |
| P6 | inc6 | inc-regional | ins | 0.009533 | 0.008470 | 0.000215 | 0.000687 | 0.000031 |
| P6 | inc10 | full | del | 0.004071 | 0.000959 | 0.000167 | 0.000694 | 0.000047 |
| P6 | inc10 | full | ins | 0.002517 | 0.000929 | 0.000164 | 0.000721 | 0.000049 |
| P6 | inc10 | inc-naive | del | 0.011089 | 0.009985 | 0.000244 | 0.000677 | 0.000049 |
| P6 | inc10 | inc-naive | ins | 0.009020 | 0.007933 | 0.000261 | 0.000656 | 0.000044 |
| P6 | inc10 | inc-regional | del | 0.010624 | 0.009605 | 0.000241 | 0.000603 | 0.000048 |
| P6 | inc10 | inc-regional | ins | 0.009229 | 0.008056 | 0.000262 | 0.000727 | 0.000049 |
| P7 | inc3 | full | del | 0.006435 | 0.002596 | 0.000219 | 0.001043 | 0.000075 |
| P7 | inc3 | full | ins | 0.004729 | 0.002359 | 0.000231 | 0.001053 | 0.000071 |
| P7 | inc3 | inc-naive | del | 0.011374 | 0.009512 | 0.000366 | 0.001309 | 0.000060 |
| P7 | inc3 | inc-naive | ins | 0.010524 | 0.009314 | 0.000322 | 0.000709 | 0.000043 |
| P7 | inc3 | inc-regional | del | 0.010782 | 0.009246 | 0.000323 | 0.001027 | 0.000056 |
| P7 | inc3 | inc-regional | ins | 0.010620 | 0.009269 | 0.000329 | 0.000849 | 0.000042 |
| P7 | inc6 | full | del | 0.006586 | 0.002624 | 0.000264 | 0.001066 | 0.000065 |
| P7 | inc6 | full | ins | 0.005038 | 0.002646 | 0.000370 | 0.001045 | 0.000071 |
| P7 | inc6 | inc-naive | del | 0.010698 | 0.008168 | 0.000582 | 0.001742 | 0.000061 |
| P7 | inc6 | inc-naive | ins | 0.011319 | 0.010040 | 0.000332 | 0.000775 | 0.000046 |
| P7 | inc6 | inc-regional | del | 0.009906 | 0.008188 | 0.000341 | 0.001172 | 0.000060 |
| P7 | inc6 | inc-regional | ins | 0.010071 | 0.008320 | 0.000529 | 0.000995 | 0.000070 |
| P7 | inc10 | full | del | 0.007518 | 0.002689 | 0.000265 | 0.001700 | 0.000080 |
| P7 | inc10 | full | ins | 0.005881 | 0.003076 | 0.000306 | 0.001268 | 0.000087 |
| P7 | inc10 | inc-naive | del | 0.012346 | 0.009805 | 0.000440 | 0.001907 | 0.000062 |
| P7 | inc10 | inc-naive | ins | 0.026713 | 0.025358 | 0.000364 | 0.000817 | 0.000046 |
| P7 | inc10 | inc-regional | del | 0.011746 | 0.009572 | 0.000406 | 0.001571 | 0.000065 |
| P7 | inc10 | inc-regional | ins | 0.011605 | 0.009962 | 0.000369 | 0.000991 | 0.000140 |
| P8 | inc3 | full | del | 0.010453 | 0.005275 | 0.000366 | 0.001721 | 0.000100 |
| P8 | inc3 | full | ins | 0.008082 | 0.004602 | 0.000338 | 0.001741 | 0.000143 |
| P8 | inc3 | inc-naive | del | 0.015959 | 0.010524 | 0.000587 | 0.004616 | 0.000095 |
| P8 | inc3 | inc-naive | ins | 0.012220 | 0.009878 | 0.000646 | 0.001475 | 0.000064 |
| P8 | inc3 | inc-regional | del | 0.012450 | 0.008411 | 0.000421 | 0.003394 | 0.000093 |
| P8 | inc3 | inc-regional | ins | 0.011990 | 0.009843 | 0.000426 | 0.001518 | 0.000074 |
| P8 | inc6 | full | del | 0.010880 | 0.005653 | 0.000338 | 0.001737 | 0.000101 |
| P8 | inc6 | full | ins | 0.008985 | 0.005255 | 0.000318 | 0.001932 | 0.000116 |
| P8 | inc6 | inc-naive | del | 0.016550 | 0.009800 | 0.000569 | 0.005898 | 0.000129 |
| P8 | inc6 | inc-naive | ins | 0.012377 | 0.009887 | 0.000510 | 0.001754 | 0.000092 |
| P8 | inc6 | inc-regional | del | 0.015225 | 0.009917 | 0.000588 | 0.004445 | 0.000139 |
| P8 | inc6 | inc-regional | ins | 0.027826 | 0.025397 | 0.000636 | 0.001561 | 0.000095 |
| P8 | inc10 | full | del | 0.010284 | 0.005190 | 0.000314 | 0.001504 | 0.000098 |
| P8 | inc10 | full | ins | 0.008259 | 0.005127 | 0.000321 | 0.001562 | 0.000102 |
| P8 | inc10 | inc-naive | del | 0.017640 | 0.010779 | 0.000931 | 0.005696 | 0.000094 |
| P8 | inc10 | inc-naive | ins | 0.013088 | 0.010225 | 0.000603 | 0.002033 | 0.000082 |
| P8 | inc10 | inc-regional | del | 0.015954 | 0.010949 | 0.000664 | 0.004094 | 0.000101 |
| P8 | inc10 | inc-regional | ins | 0.012668 | 0.010067 | 0.000592 | 0.001776 | 0.000085 |
| P9 | inc3 | full | del | 0.112849 | 0.003553 | 0.000367 | 0.105036 | 0.000146 |
| P9 | inc3 | full | ins | 0.077560 | 0.003314 | 0.000340 | 0.071949 | 0.000147 |
| P9 | inc3 | inc-naive | del | 0.011642 | 0.008670 | 0.000688 | 0.002061 | 0.000087 |
| P9 | inc3 | inc-naive | ins | 0.012265 | 0.009977 | 0.000534 | 0.001563 | 0.000049 |
| P9 | inc3 | inc-regional | del | 0.011791 | 0.008876 | 0.000740 | 0.001960 | 0.000068 |
| P9 | inc3 | inc-regional | ins | 0.013160 | 0.010136 | 0.000633 | 0.002167 | 0.000060 |
| P9 | inc6 | full | del | 0.152990 | 0.003434 | 0.000368 | 0.145464 | 0.000116 |
| P9 | inc6 | full | ins | 0.128718 | 0.003289 | 0.000328 | 0.122320 | 0.000122 |
| P9 | inc6 | inc-naive | del | 0.014183 | 0.010450 | 0.000699 | 0.002842 | 0.000063 |
| P9 | inc6 | inc-naive | ins | 0.013030 | 0.010100 | 0.000813 | 0.001914 | 0.000062 |
| P9 | inc6 | inc-regional | del | 0.014609 | 0.010577 | 0.000747 | 0.003051 | 0.000076 |
| P9 | inc6 | inc-regional | ins | 0.013437 | 0.010155 | 0.000728 | 0.002345 | 0.000059 |
| P9 | inc10 | full | del | 0.061357 | 0.003368 | 0.000344 | 0.054157 | 0.000080 |
| P9 | inc10 | full | ins | 0.113909 | 0.003271 | 0.000296 | 0.108474 | 0.000142 |
| P9 | inc10 | inc-naive | del | 0.015729 | 0.011682 | 0.000651 | 0.003189 | 0.000078 |
| P9 | inc10 | inc-naive | ins | 0.012984 | 0.010095 | 0.000586 | 0.002090 | 0.000075 |
| P9 | inc10 | inc-regional | del | 0.015996 | 0.011663 | 0.000989 | 0.003113 | 0.000082 |
| P9 | inc10 | inc-regional | ins | 0.013082 | 0.009932 | 0.000607 | 0.002342 | 0.000063 |
| P10 | inc3 | full | del | 0.042161 | 0.036907 | 0.001261 | 0.000363 | 0.000034 |
| P10 | inc3 | full | ins | 0.038785 | 0.035261 | 0.001165 | 0.000335 | 0.000034 |
| P10 | inc3 | inc-naive | del | 0.020587 | 0.018223 | 0.001906 | 0.000301 | 0.000032 |
| P10 | inc3 | inc-naive | ins | 0.011214 | 0.009131 | 0.001657 | 0.000266 | 0.000033 |
| P10 | inc3 | inc-regional | del | 0.021926 | 0.018720 | 0.002742 | 0.000299 | 0.000035 |
| P10 | inc3 | inc-regional | ins | 0.011684 | 0.009112 | 0.002092 | 0.000314 | 0.000033 |
| P10 | inc6 | full | del | 0.042685 | 0.036400 | 0.001363 | 0.000478 | 0.000050 |
| P10 | inc6 | full | ins | 0.037347 | 0.033941 | 0.001160 | 0.000324 | 0.000032 |
| P10 | inc6 | inc-naive | del | 0.023533 | 0.020792 | 0.002261 | 0.000319 | 0.000033 |
| P10 | inc6 | inc-naive | ins | 0.011880 | 0.009495 | 0.001964 | 0.000260 | 0.000032 |
| P10 | inc6 | inc-regional | del | 0.023406 | 0.020710 | 0.002208 | 0.000320 | 0.000032 |
| P10 | inc6 | inc-regional | ins | 0.011781 | 0.009470 | 0.001828 | 0.000315 | 0.000033 |
| P10 | inc10 | full | del | 0.037980 | 0.032785 | 0.001120 | 0.000279 | 0.000031 |
| P10 | inc10 | full | ins | 0.038114 | 0.034607 | 0.001264 | 0.000342 | 0.000036 |
| P10 | inc10 | inc-naive | del | 0.026649 | 0.022946 | 0.003110 | 0.000424 | 0.000035 |
| P10 | inc10 | inc-naive | ins | 0.013057 | 0.010046 | 0.002528 | 0.000317 | 0.000037 |
| P10 | inc10 | inc-regional | del | 0.026306 | 0.023051 | 0.002692 | 0.000392 | 0.000036 |
| P10 | inc10 | inc-regional | ins | 0.012787 | 0.010029 | 0.002175 | 0.000414 | 0.000033 |
| P11 | inc3 | full | del | 0.038916 | 0.034130 | 0.001148 | 0.000174 | 0.000028 |
| P11 | inc3 | full | ins | 0.036455 | 0.033407 | 0.001216 | 0.000173 | 0.000028 |
| P11 | inc3 | inc-naive | del | 0.018136 | 0.015990 | 0.001893 | 0.000093 | 0.000032 |
| P11 | inc3 | inc-naive | ins | 0.007213 | 0.005511 | 0.001463 | 0.000083 | 0.000030 |
| P11 | inc3 | inc-regional | del | 0.018098 | 0.015992 | 0.001848 | 0.000031 | 0.000100 |
| P11 | inc3 | inc-regional | ins | 0.007881 | 0.006048 | 0.001565 | 0.000056 | 0.000082 |
| P11 | inc6 | full | del | 0.039736 | 0.034782 | 0.001154 | 0.000176 | 0.000028 |
| P11 | inc6 | full | ins | 0.038490 | 0.035296 | 0.001138 | 0.000180 | 0.000028 |
| P11 | inc6 | inc-naive | del | 0.022922 | 0.019925 | 0.002645 | 0.000125 | 0.000042 |
| P11 | inc6 | inc-naive | ins | 0.010434 | 0.007624 | 0.002500 | 0.000107 | 0.000038 |
| P11 | inc6 | inc-regional | del | 0.022539 | 0.019304 | 0.002971 | 0.000038 | 0.000091 |
| P11 | inc6 | inc-regional | ins | 0.009245 | 0.006021 | 0.002968 | 0.000033 | 0.000093 |
| P11 | inc10 | full | del | 0.040586 | 0.035082 | 0.001427 | 0.000184 | 0.000029 |
| P11 | inc10 | full | ins | 0.039684 | 0.035939 | 0.001455 | 0.000182 | 0.000028 |
| P11 | inc10 | inc-naive | del | 0.027388 | 0.022037 | 0.005093 | 0.000099 | 0.000033 |
| P11 | inc10 | inc-naive | ins | 0.010544 | 0.006606 | 0.003691 | 0.000088 | 0.000030 |
| P11 | inc10 | inc-regional | del | 0.025998 | 0.021498 | 0.004220 | 0.000039 | 0.000095 |
| P11 | inc10 | inc-regional | ins | 0.011770 | 0.008020 | 0.003498 | 0.000033 | 0.000090 |
| P12 | inc3 | full | del | 0.470035 | 0.055350 | 0.002376 | 0.387387 | 0.000796 |
| P12 | inc3 | full | ins | 0.452567 | 0.052805 | 0.002658 | 0.371542 | 0.001032 |
| P12 | inc3 | inc-naive | del | 0.047784 | 0.018147 | 0.005805 | 0.023029 | 0.000656 |
| P12 | inc3 | inc-naive | ins | 0.034537 | 0.013803 | 0.004952 | 0.015246 | 0.000381 |
| P12 | inc3 | inc-regional | del | 0.043572 | 0.017718 | 0.005509 | 0.019612 | 0.000587 |
| P12 | inc3 | inc-regional | ins | 0.034163 | 0.013433 | 0.003922 | 0.016286 | 0.000369 |
| P12 | inc6 | full | del | 0.464781 | 0.056393 | 0.002645 | 0.380327 | 0.000756 |
| P12 | inc6 | full | ins | 0.472495 | 0.051064 | 0.002586 | 0.394284 | 0.000913 |
| P12 | inc6 | inc-naive | del | 0.073397 | 0.026455 | 0.006953 | 0.039047 | 0.000796 |
| P12 | inc6 | inc-naive | ins | 0.037093 | 0.015658 | 0.004825 | 0.015988 | 0.000461 |
| P12 | inc6 | inc-regional | del | 0.078803 | 0.034100 | 0.008193 | 0.035207 | 0.001129 |
| P12 | inc6 | inc-regional | ins | 0.045078 | 0.016204 | 0.006660 | 0.021475 | 0.000566 |
| P12 | inc10 | full | del | 0.478162 | 0.055116 | 0.003010 | 0.394428 | 0.000786 |
| P12 | inc10 | full | ins | 0.461748 | 0.052310 | 0.002498 | 0.382332 | 0.000761 |
| P12 | inc10 | inc-naive | del | 0.086376 | 0.027207 | 0.006710 | 0.051684 | 0.000635 |
| P12 | inc10 | inc-naive | ins | 0.038317 | 0.015410 | 0.005173 | 0.017119 | 0.000452 |
| P12 | inc10 | inc-regional | del | 0.078568 | 0.028190 | 0.006860 | 0.042768 | 0.000604 |
| P12 | inc10 | inc-regional | ins | 0.043674 | 0.018588 | 0.005101 | 0.019200 | 0.000604 |
| P13 | inc3 | full | del | 1.412406 | 0.098610 | 0.005396 | 1.247723 | 0.003583 |
| P13 | inc3 | full | ins | 1.348840 | 0.095147 | 0.004875 | 1.209083 | 0.004764 |
| P13 | inc3 | inc-naive | del | 0.152406 | 0.044011 | 0.013902 | 0.089451 | 0.004866 |
| P13 | inc3 | inc-naive | ins | 0.082070 | 0.022594 | 0.010612 | 0.046141 | 0.002543 |
| P13 | inc3 | inc-regional | del | 0.123941 | 0.036116 | 0.010702 | 0.072938 | 0.004026 |
| P13 | inc3 | inc-regional | ins | 0.079955 | 0.022629 | 0.009009 | 0.046131 | 0.002001 |
| P13 | inc6 | full | del | 1.403585 | 0.098008 | 0.005618 | 1.245162 | 0.003405 |
| P13 | inc6 | full | ins | 1.349781 | 0.098613 | 0.005222 | 1.207495 | 0.004092 |
| P13 | inc6 | inc-naive | del | 0.232052 | 0.056343 | 0.015694 | 0.154375 | 0.005379 |
| P13 | inc6 | inc-naive | ins | 0.108934 | 0.037973 | 0.011248 | 0.056157 | 0.003309 |
| P13 | inc6 | inc-regional | del | 0.197385 | 0.049025 | 0.012799 | 0.130730 | 0.004659 |
| P13 | inc6 | inc-regional | ins | 0.095749 | 0.023972 | 0.010301 | 0.058491 | 0.002769 |
| P13 | inc10 | full | del | 1.075082 | 0.103882 | 0.005832 | 0.904849 | 0.003454 |
| P13 | inc10 | full | ins | 1.390058 | 0.102429 | 0.005319 | 1.244928 | 0.003686 |
| P13 | inc10 | inc-naive | del | 0.411597 | 0.065688 | 0.015051 | 0.324686 | 0.006011 |
| P13 | inc10 | inc-naive | ins | 0.125319 | 0.026608 | 0.015015 | 0.079024 | 0.004379 |
| P13 | inc10 | inc-regional | del | 0.380752 | 0.068433 | 0.016923 | 0.288924 | 0.006310 |
| P13 | inc10 | inc-regional | ins | 0.127924 | 0.034497 | 0.013484 | 0.075602 | 0.003984 |
| P14 | inc3 | full | del | 2.728114 | 0.146742 | 0.007129 | 2.337107 | 0.027899 |
| P14 | inc3 | full | ins | 3.509590 | 0.174226 | 0.009164 | 3.084548 | 0.042574 |
| P14 | inc3 | inc-naive | del | 0.376568 | 0.055396 | 0.017689 | 0.269294 | 0.034016 |
| P14 | inc3 | inc-naive | ins | 0.217337 | 0.034768 | 0.020941 | 0.141748 | 0.019579 |
| P14 | inc3 | inc-regional | del | 0.371347 | 0.060029 | 0.019435 | 0.250909 | 0.040793 |
| P14 | inc3 | inc-regional | ins | 0.261299 | 0.037070 | 0.016850 | 0.185462 | 0.021592 |
| P14 | inc6 | full | del | 2.655964 | 0.143987 | 0.007172 | 2.295306 | 0.020317 |
| P14 | inc6 | full | ins | 3.257108 | 0.143258 | 0.005974 | 2.873654 | 0.041486 |
| P14 | inc6 | inc-naive | del | 0.558662 | 0.072192 | 0.020635 | 0.425739 | 0.039898 |
| P14 | inc6 | inc-naive | ins | 0.249851 | 0.033527 | 0.017666 | 0.172943 | 0.025394 |
| P14 | inc6 | inc-regional | del | 0.513212 | 0.070383 | 0.019534 | 0.386431 | 0.036689 |
| P14 | inc6 | inc-regional | ins | 0.299806 | 0.040675 | 0.019214 | 0.212419 | 0.027138 |
| P14 | inc10 | full | del | 2.595619 | 0.142121 | 0.006999 | 2.235396 | 0.015468 |
| P14 | inc10 | full | ins | 3.530348 | 0.156188 | 0.008577 | 2.913660 | 0.037062 |
| P14 | inc10 | inc-naive | del | 0.905535 | 0.096669 | 0.023876 | 0.748496 | 0.036324 |
| P14 | inc10 | inc-naive | ins | 0.315028 | 0.049190 | 0.020590 | 0.210830 | 0.033936 |
| P14 | inc10 | inc-regional | del | 0.919165 | 0.101012 | 0.025397 | 0.750516 | 0.042050 |
| P14 | inc10 | inc-regional | ins | 0.420034 | 0.053101 | 0.026327 | 0.307450 | 0.032633 |

### inc-regional insert: regionNodes / drNodes / ratio (from `inc-analyze`)

| Case | Delta | regionNodes | drNodes | ratio |
| --- | --- | ---: | ---: | ---: |
| P1 | inc3 | n/a | n/a | n/a |
| P1 | inc6 | n/a | n/a | n/a |
| P1 | inc10 | n/a | n/a | n/a |
| P3 | inc3 | n/a | n/a | n/a |
| P3 | inc6 | n/a | n/a | n/a |
| P3 | inc10 | n/a | n/a | n/a |
| P4 | inc3 | n/a | n/a | n/a |
| P4 | inc6 | n/a | n/a | n/a |
| P4 | inc10 | n/a | n/a | n/a |
| P5 | inc3 | 3 | 12 | 0.250 |
| P5 | inc6 | 3 | 12 | 0.250 |
| P5 | inc10 | 3 | 12 | 0.250 |
| P6 | inc3 | 24 | 24 | 1.000 |
| P6 | inc6 | 24 | 24 | 1.000 |
| P6 | inc10 | 27 | 28 | 0.964 |
| P7 | inc3 | 3 | 11 | 0.273 |
| P7 | inc6 | 9 | 17 | 0.529 |
| P7 | inc10 | 16 | 24 | 0.667 |
| P8 | inc3 | 6 | 46 | 0.130 |
| P8 | inc6 | 12 | 63 | 0.190 |
| P8 | inc10 | 40 | 80 | 0.500 |
| P9 | inc3 | 3 | 8 | 0.375 |
| P9 | inc6 | 91 | 105 | 0.867 |
| P9 | inc10 | 94 | 111 | 0.847 |
| P10 | inc3 | 2 | 2 | 1.000 |
| P10 | inc6 | 2 | 2 | 1.000 |
| P10 | inc10 | 26 | 26 | 1.000 |
| P11 | inc3 | n/a | n/a | n/a |
| P11 | inc6 | n/a | n/a | n/a |
| P11 | inc10 | n/a | n/a | n/a |
| P12 | inc3 | 94 | 225 | 0.418 |
| P12 | inc6 | 193 | 439 | 0.440 |
| P12 | inc10 | 315 | 601 | 0.524 |
| P13 | inc3 | 265 | 681 | 0.389 |
| P13 | inc6 | 652 | 1417 | 0.460 |
| P13 | inc10 | 1168 | 2160 | 0.541 |
| P14 | inc3 | 691 | 1488 | 0.464 |
| P14 | inc6 | 1185 | 2234 | 0.530 |
| P14 | inc10 | 1946 | 3362 | 0.579 |

Note: n/a indicates no inc-analyze output (pruned insert delta empty).

## Notes
- All runs used `--det-opt` in the CLI run phase.
- If deterministic delta seeds are required, rerun `delta` with an explicit `--seed`.
- Logs and per-delta JSON summaries are under each case’s `output/` directory.
