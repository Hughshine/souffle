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

## Notes
- All runs used `--det-opt` in the CLI run phase.
- If deterministic delta seeds are required, rerun `delta` with an explicit `--seed`.
- Logs and per-delta JSON summaries are under each case’s `output/` directory.
