# Final Incremental Evaluation Log (2026-01-21)

This file records the **latest full P1–P20 incremental runs**, including
generation seeds, strengthen defaults, delta defaults, and run outcomes.
Use this as the canonical snapshot; older eval notes remain in
`README.eval.inc.md` for history.

## Dataset D: `side_channel_inc_strengthen_fresh` (2026-01-21, det-opt)

**Location**
`/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc_strengthen_fresh`

**Commands used**
```bash
# compile (uses existing generated/strengthened/delta data in base-dir)
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc_strengthen_fresh \
  compile --cases 16-20 --jobs $(nproc || sysctl -n hw.ncpu || echo 2)

# run (compare-all, det-opt)
python3 /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc_strengthen_fresh \
  run --cases 16-20 --delta-labels inc1,inc3,inc5 --delta-samples 1 \
  --timeout 600 --compare-all --run-arg=--det-opt
```

**Run timestamps**
- Compile: 2026-01-21 16:57–16:58 local
- Run: 2026-01-21 16:59–17:28 local

**Outcome summary**
- P1–P15 (earlier re-run on this dataset): all compare-all OK.
- P16–P20: inc-regional mismatches on insert (iter2); P19/P20 also show
  inc_iter1 vs full mismatches on some deltas (see details below).

**Mismatch details (from `delta-*.json`)**
- P16: inc3/inc5 base_vs_inc_regional_iter2 mismatches=9/45 (max|Δ|=3.47e-03)
- P17: inc3/inc5 base_vs_inc_regional_iter2 mismatches=90/147 (max|Δ|=2.13e-02/1.30e-02)
- P18: inc1/inc3/inc5 base_vs_inc_regional_iter2 mismatches=93/171/237 (max|Δ|=6.97e-03/3.15e-02/3.15e-02)
- P19:
  - inc1: base_vs_inc_regional_iter2 mismatches=123 (max|Δ|=2.15e-02)
  - inc3: inc_iter1_vs_full_iter1 mismatches=102 (max|Δ|=6.93e-02) and base_vs_inc_regional_iter2 mismatches=306 (max|Δ|=2.15e-02)
  - inc5: inc_iter1_vs_full_iter1 mismatches=21 (max|Δ|=6.54e-02) and base_vs_inc_regional_iter2 mismatches=447 (max|Δ|=2.62e-02)
- P20:
  - inc1/inc3: base_vs_inc_regional_iter2 mismatches=27/75 (max|Δ|=7.72e-03/3.00e-02)
  - inc5: inc_iter1_vs_full_iter1 mismatches=30 (max|Δ|=6.85e-01) and base_vs_inc_regional_iter2 mismatches=90 (max|Δ|=3.00e-02)

### Dataset D rerun (2026-01-22, det-opt + profile-inc-regional, P1–P18)

**Commands used**
```bash
# compile (rebuild compute with updated inc-regional instrumentation)
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc_strengthen_fresh \
  compile --cases 1-18 --jobs $(nproc || sysctl -n hw.ncpu || echo 2)

# run (compare-all, det-opt + profile-inc-regional)
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH \
  python3 /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc.py \
  --base-dir /home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc_strengthen_fresh \
  run --cases 1-18 --delta-labels inc1,inc3,inc5 --delta-samples 1 \
  --timeout 1200 --compare-all --run-arg=--det-opt --run-arg=--profile-inc-regional
```

**Run timestamps**
- Compile: 2026-01-22 02:57–03:00 local
- Run: 2026-01-22 03:00–03:17 local

**Notes**
- P2 skipped (missing `compute.souffle.dl`, no delta files).
- All other cases (P1, P3–P18) compare-all OK.

### End-to-end time + correctness (P1–P18, inc1/inc3/inc5)

| Case | Delta | Full(s) | Inc-naive(s) | Inc-regional(s) | Speedup full/naive | Speedup full/regional | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | inc1 | 0.105 | 0.055 | 0.065 | 1.892 | 1.616 | OK |
| 1 | inc3 | 0.092 | 0.056 | 0.074 | 1.625 | 1.236 | OK |
| 1 | inc5 | 0.091 | 0.050 | 0.060 | 1.819 | 1.505 | OK |
| 3 | inc1 | 0.101 | 0.048 | 0.046 | 2.114 | 2.183 | OK |
| 3 | inc3 | 0.108 | 0.052 | 0.049 | 2.062 | 2.208 | OK |
| 3 | inc5 | 0.102 | 0.063 | 0.047 | 1.634 | 2.193 | OK |
| 4 | inc1 | 0.015 | 0.018 | 0.015 | 0.855 | 1.013 | OK |
| 4 | inc3 | 0.014 | 0.015 | 0.016 | 0.951 | 0.919 | OK |
| 4 | inc5 | 0.014 | 0.015 | 0.016 | 0.971 | 0.888 | OK |
| 5 | inc1 | 0.018 | 0.038 | 0.049 | 0.480 | 0.373 | OK |
| 5 | inc3 | 0.019 | 0.020 | 0.021 | 0.962 | 0.899 | OK |
| 5 | inc5 | 0.019 | 0.021 | 0.020 | 0.882 | 0.930 | OK |
| 6 | inc1 | 0.029 | 0.055 | 0.040 | 0.520 | 0.725 | OK |
| 6 | inc3 | 0.027 | 0.040 | 0.043 | 0.663 | 0.620 | OK |
| 6 | inc5 | 0.026 | 0.039 | 0.036 | 0.679 | 0.724 | OK |
| 7 | inc1 | 0.032 | 0.043 | 0.046 | 0.743 | 0.709 | OK |
| 7 | inc3 | 0.034 | 0.046 | 0.045 | 0.732 | 0.759 | OK |
| 7 | inc5 | 0.050 | 0.047 | 0.049 | 1.063 | 1.029 | OK |
| 8 | inc1 | 0.045 | 0.053 | 0.053 | 0.861 | 0.864 | OK |
| 8 | inc3 | 0.047 | 0.057 | 0.054 | 0.818 | 0.859 | OK |
| 8 | inc5 | 0.055 | 0.063 | 0.061 | 0.878 | 0.902 | OK |
| 9 | inc1 | 0.265 | 0.132 | 0.140 | 2.001 | 1.896 | OK |
| 9 | inc3 | 0.334 | 0.128 | 0.151 | 2.617 | 2.204 | OK |
| 9 | inc5 | 0.264 | 0.131 | 0.132 | 2.007 | 2.004 | OK |
| 10 | inc1 | 0.165 | 0.108 | 0.104 | 1.530 | 1.588 | OK |
| 10 | inc3 | 0.176 | 0.127 | 0.128 | 1.387 | 1.382 | OK |
| 10 | inc5 | 0.191 | 0.159 | 0.135 | 1.204 | 1.413 | OK |
| 11 | inc1 | 0.156 | 0.091 | 0.092 | 1.708 | 1.687 | OK |
| 11 | inc3 | 0.166 | 0.124 | 0.116 | 1.336 | 1.432 | OK |
| 11 | inc5 | 0.177 | 0.127 | 0.122 | 1.394 | 1.452 | OK |
| 12 | inc1 | 1.471 | 0.581 | 0.655 | 2.530 | 2.246 | OK |
| 12 | inc3 | 1.515 | 0.748 | 0.612 | 2.024 | 2.477 | OK |
| 12 | inc5 | 1.510 | 0.631 | 0.659 | 2.392 | 2.292 | OK |
| 13 | inc1 | 4.172 | 1.669 | 1.601 | 2.499 | 2.607 | OK |
| 13 | inc3 | 4.068 | 1.750 | 1.792 | 2.325 | 2.270 | OK |
| 13 | inc5 | 3.954 | 1.852 | 1.999 | 2.135 | 1.978 | OK |
| 14 | inc1 | 9.469 | 3.838 | 3.826 | 2.467 | 2.475 | OK |
| 14 | inc3 | 9.634 | 4.015 | 4.206 | 2.400 | 2.290 | OK |
| 14 | inc5 | 8.865 | 4.313 | 4.548 | 2.055 | 1.949 | OK |
| 15 | inc1 | 20.746 | 8.659 | 8.591 | 2.396 | 2.415 | OK |
| 15 | inc3 | 22.030 | 9.623 | 9.892 | 2.289 | 2.227 | OK |
| 15 | inc5 | 20.718 | 10.617 | 11.290 | 1.951 | 1.835 | OK |
| 16 | inc1 | 33.764 | 15.046 | 15.203 | 2.244 | 2.221 | OK |
| 16 | inc3 | 32.853 | 16.942 | 17.808 | 1.939 | 1.845 | OK |
| 16 | inc5 | 32.847 | 20.445 | 21.352 | 1.607 | 1.538 | OK |
| 17 | inc1 | 23.324 | 9.574 | 10.088 | 2.436 | 2.312 | OK |
| 17 | inc3 | 21.596 | 11.118 | 12.872 | 1.942 | 1.678 | OK |
| 17 | inc5 | 20.046 | 12.982 | 15.564 | 1.544 | 1.288 | OK |
| 18 | inc1 | 46.077 | 20.313 | 21.362 | 2.268 | 2.157 | OK |
| 18 | inc3 | 43.282 | 23.936 | 25.480 | 1.808 | 1.699 | OK |
| 18 | inc5 | 46.195 | 26.597 | 30.924 | 1.737 | 1.494 | OK |

### Delete turn (turn2) times + speedup vs full

| Case | Delta | Full del(s) | Inc-naive del(s) | Speedup full/naive | Inc-regional del(s) | Speedup full/regional |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | inc1 | 0.037 | 0.010 | 3.589 | 0.010 | 3.549 |
| 1 | inc3 | 0.029 | 0.010 | 2.804 | 0.025 | 1.153 |
| 1 | inc5 | 0.028 | 0.010 | 2.660 | 0.010 | 2.850 |
| 3 | inc1 | 0.032 | 0.003 | 9.874 | 0.003 | 10.888 |
| 3 | inc3 | 0.035 | 0.003 | 12.788 | 0.003 | 12.222 |
| 3 | inc5 | 0.032 | 0.018 | 1.768 | 0.003 | 11.495 |
| 4 | inc1 | 0.003 | 0.003 | 1.118 | 0.001 | 2.002 |
| 4 | inc3 | 0.003 | 0.002 | 1.768 | 0.001 | 2.627 |
| 4 | inc5 | 0.003 | 0.001 | 2.366 | 0.001 | 2.693 |
| 5 | inc1 | 0.003 | 0.007 | 0.488 | 0.012 | 0.287 |
| 5 | inc3 | 0.003 | 0.004 | 0.791 | 0.004 | 0.739 |
| 5 | inc5 | 0.003 | 0.004 | 0.742 | 0.004 | 0.683 |
| 6 | inc1 | 0.005 | 0.025 | 0.188 | 0.010 | 0.478 |
| 6 | inc3 | 0.004 | 0.010 | 0.441 | 0.011 | 0.419 |
| 6 | inc5 | 0.004 | 0.010 | 0.415 | 0.010 | 0.412 |
| 7 | inc1 | 0.006 | 0.010 | 0.638 | 0.010 | 0.622 |
| 7 | inc3 | 0.006 | 0.011 | 0.539 | 0.011 | 0.565 |
| 7 | inc5 | 0.007 | 0.012 | 0.578 | 0.012 | 0.564 |
| 8 | inc1 | 0.010 | 0.013 | 0.754 | 0.013 | 0.791 |
| 8 | inc3 | 0.010 | 0.017 | 0.575 | 0.014 | 0.720 |
| 8 | inc5 | 0.012 | 0.018 | 0.658 | 0.017 | 0.703 |
| 9 | inc1 | 0.075 | 0.013 | 5.884 | 0.013 | 5.865 |
| 9 | inc3 | 0.135 | 0.011 | 12.765 | 0.030 | 4.549 |
| 9 | inc5 | 0.062 | 0.012 | 5.065 | 0.010 | 6.020 |
| 10 | inc1 | 0.040 | 0.017 | 2.347 | 0.015 | 2.636 |
| 10 | inc3 | 0.046 | 0.022 | 2.052 | 0.027 | 1.721 |
| 10 | inc5 | 0.043 | 0.042 | 1.008 | 0.022 | 1.928 |
| 11 | inc1 | 0.042 | 0.014 | 3.092 | 0.013 | 3.373 |
| 11 | inc3 | 0.043 | 0.021 | 2.052 | 0.019 | 2.300 |
| 11 | inc5 | 0.043 | 0.023 | 1.921 | 0.021 | 2.061 |
| 12 | inc1 | 0.485 | 0.032 | 15.342 | 0.034 | 14.222 |
| 12 | inc3 | 0.474 | 0.057 | 8.307 | 0.049 | 9.647 |
| 12 | inc5 | 0.475 | 0.061 | 7.728 | 0.057 | 8.305 |
| 13 | inc1 | 1.364 | 0.095 | 14.311 | 0.081 | 16.928 |
| 13 | inc3 | 1.321 | 0.167 | 7.924 | 0.152 | 8.665 |
| 13 | inc5 | 1.045 | 0.221 | 4.718 | 0.191 | 5.476 |
| 14 | inc1 | 2.666 | 0.186 | 14.314 | 0.179 | 14.876 |
| 14 | inc3 | 2.753 | 0.350 | 7.853 | 0.348 | 7.904 |
| 14 | inc5 | 2.615 | 0.534 | 4.892 | 0.490 | 5.336 |
| 15 | inc1 | 6.654 | 0.971 | 6.850 | 1.003 | 6.634 |
| 15 | inc3 | 6.491 | 1.783 | 3.641 | 1.654 | 3.925 |
| 15 | inc5 | 6.597 | 2.395 | 2.755 | 2.294 | 2.876 |
| 16 | inc1 | 9.448 | 1.417 | 6.669 | 1.270 | 7.437 |
| 16 | inc3 | 8.395 | 2.727 | 3.079 | 2.708 | 3.100 |
| 16 | inc5 | 8.133 | 4.474 | 1.818 | 4.100 | 1.984 |
| 17 | inc1 | 7.588 | 0.819 | 9.261 | 0.755 | 10.051 |
| 17 | inc3 | 5.752 | 1.795 | 3.203 | 1.765 | 3.259 |
| 17 | inc5 | 4.542 | 3.283 | 1.384 | 3.134 | 1.449 |
| 18 | inc1 | 12.081 | 1.811 | 6.669 | 1.670 | 7.235 |
| 18 | inc3 | 9.155 | 3.398 | 2.695 | 3.041 | 3.011 |
| 18 | inc5 | 9.516 | 5.343 | 1.781 | 5.086 | 1.871 |

### Insert turn (turn3) times + speedup vs full

| Case | Delta | Full ins(s) | Inc-naive ins(s) | Speedup full/naive | Inc-regional ins(s) | Speedup full/regional |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | inc1 | 0.031 | 0.005 | 6.453 | 0.017 | 1.859 |
| 1 | inc3 | 0.027 | 0.006 | 4.258 | 0.013 | 2.027 |
| 1 | inc5 | 0.027 | 0.005 | 5.463 | 0.014 | 1.880 |
| 3 | inc1 | 0.031 | 0.004 | 8.419 | 0.004 | 7.803 |
| 3 | inc3 | 0.031 | 0.003 | 9.933 | 0.006 | 5.315 |
| 3 | inc5 | 0.031 | 0.003 | 9.337 | 0.006 | 4.875 |
| 4 | inc1 | 0.009 | 0.011 | 0.799 | 0.008 | 1.155 |
| 4 | inc3 | 0.008 | 0.010 | 0.791 | 0.008 | 1.035 |
| 4 | inc5 | 0.008 | 0.010 | 0.783 | 0.008 | 1.015 |
| 5 | inc1 | 0.011 | 0.023 | 0.457 | 0.026 | 0.415 |
| 5 | inc3 | 0.011 | 0.016 | 0.695 | 0.016 | 0.703 |
| 5 | inc5 | 0.011 | 0.016 | 0.696 | 0.014 | 0.792 |
| 6 | inc1 | 0.012 | 0.016 | 0.747 | 0.018 | 0.667 |
| 6 | inc3 | 0.012 | 0.014 | 0.878 | 0.017 | 0.695 |
| 6 | inc5 | 0.012 | 0.013 | 0.923 | 0.015 | 0.833 |
| 7 | inc1 | 0.012 | 0.016 | 0.782 | 0.017 | 0.701 |
| 7 | inc3 | 0.012 | 0.016 | 0.737 | 0.015 | 0.800 |
| 7 | inc5 | 0.018 | 0.018 | 1.000 | 0.017 | 1.037 |
| 8 | inc1 | 0.015 | 0.021 | 0.692 | 0.019 | 0.796 |
| 8 | inc3 | 0.016 | 0.021 | 0.748 | 0.019 | 0.829 |
| 8 | inc5 | 0.017 | 0.021 | 0.815 | 0.019 | 0.879 |
| 9 | inc1 | 0.129 | 0.040 | 3.230 | 0.034 | 3.835 |
| 9 | inc3 | 0.130 | 0.036 | 3.584 | 0.028 | 4.582 |
| 9 | inc5 | 0.130 | 0.043 | 3.047 | 0.031 | 4.170 |
| 10 | inc1 | 0.080 | 0.034 | 2.335 | 0.037 | 2.165 |
| 10 | inc3 | 0.082 | 0.040 | 2.033 | 0.042 | 1.951 |
| 10 | inc5 | 0.082 | 0.044 | 1.864 | 0.038 | 2.163 |
| 11 | inc1 | 0.079 | 0.035 | 2.246 | 0.041 | 1.930 |
| 11 | inc3 | 0.081 | 0.039 | 2.077 | 0.043 | 1.892 |
| 11 | inc5 | 0.086 | 0.040 | 2.174 | 0.047 | 1.842 |
| 12 | inc1 | 0.887 | 0.474 | 1.871 | 0.533 | 1.664 |
| 12 | inc3 | 0.890 | 0.575 | 1.547 | 0.481 | 1.850 |
| 12 | inc5 | 0.913 | 0.521 | 1.753 | 0.498 | 1.835 |
| 13 | inc1 | 2.436 | 1.044 | 2.333 | 1.214 | 2.006 |
| 13 | inc3 | 2.528 | 1.243 | 2.034 | 1.317 | 1.920 |
| 13 | inc5 | 2.580 | 1.370 | 1.883 | 1.493 | 1.728 |
| 14 | inc1 | 5.381 | 2.410 | 2.233 | 2.736 | 1.966 |
| 14 | inc3 | 5.439 | 2.591 | 2.100 | 2.890 | 1.882 |
| 14 | inc5 | 5.222 | 2.771 | 1.885 | 3.113 | 1.678 |
| 15 | inc1 | 11.126 | 5.542 | 2.007 | 5.734 | 1.941 |
| 15 | inc3 | 11.463 | 6.027 | 1.902 | 6.355 | 1.805 |
| 15 | inc5 | 11.027 | 6.534 | 1.688 | 6.984 | 1.579 |
| 16 | inc1 | 17.143 | 8.314 | 2.062 | 8.618 | 1.989 |
| 16 | inc3 | 17.970 | 9.694 | 1.854 | 10.189 | 1.764 |
| 16 | inc5 | 18.908 | 11.616 | 1.627 | 12.303 | 1.537 |
| 17 | inc1 | 10.737 | 5.807 | 1.849 | 6.109 | 1.758 |
| 17 | inc3 | 12.099 | 6.887 | 1.756 | 8.599 | 1.407 |
| 17 | inc5 | 12.527 | 7.934 | 1.579 | 10.321 | 1.213 |
| 18 | inc1 | 21.421 | 11.158 | 1.920 | 11.395 | 1.880 |
| 18 | inc3 | 22.728 | 12.068 | 1.883 | 13.320 | 1.706 |
| 18 | inc5 | 23.548 | 13.666 | 1.723 | 16.140 | 1.459 |

### Region vs delta-reachable (inc-regional analyze)

| Case | Delta | regionNodes | drNodes | ratio |
| --- | --- | --- | --- | --- |
| 1 | inc1 | 85 | 85 | 1.0000 |
| 1 | inc3 | 85 | 85 | 1.0000 |
| 1 | inc5 | 85 | 85 | 1.0000 |
| 2 | inc1 | NA | NA | NA |
| 2 | inc3 | NA | NA | NA |
| 2 | inc5 | NA | NA | NA |
| 3 | inc1 | NA | NA | NA |
| 3 | inc3 | NA | NA | NA |
| 3 | inc5 | NA | NA | NA |
| 4 | inc1 | NA | NA | NA |
| 4 | inc3 | NA | NA | NA |
| 4 | inc5 | NA | NA | NA |
| 5 | inc1 | 3 | 8 | 0.3750 |
| 5 | inc3 | 3 | 8 | 0.3750 |
| 5 | inc5 | 3 | 8 | 0.3750 |
| 6 | inc1 | 2 | 2 | 1.0000 |
| 6 | inc3 | 2 | 2 | 1.0000 |
| 6 | inc5 | 2 | 2 | 1.0000 |
| 7 | inc1 | 3 | 6 | 0.5000 |
| 7 | inc3 | 9 | 27 | 0.3333 |
| 7 | inc5 | 9 | 27 | 0.3333 |
| 8 | inc1 | 3 | 36 | 0.0833 |
| 8 | inc3 | 6 | 65 | 0.0923 |
| 8 | inc5 | 53 | 130 | 0.4077 |
| 9 | inc1 | 3 | 38 | 0.0789 |
| 9 | inc3 | 32 | 32 | 1.0000 |
| 9 | inc5 | 32 | 32 | 1.0000 |
| 10 | inc1 | NA | NA | NA |
| 10 | inc3 | NA | NA | NA |
| 10 | inc5 | NA | NA | NA |
| 11 | inc1 | NA | NA | NA |
| 11 | inc3 | NA | NA | NA |
| 11 | inc5 | NA | NA | NA |
| 12 | inc1 | 59 | 129 | 0.4574 |
| 12 | inc3 | 127 | 332 | 0.3825 |
| 12 | inc5 | 162 | 368 | 0.4402 |
| 13 | inc1 | 72 | 206 | 0.3495 |
| 13 | inc3 | 505 | 931 | 0.5424 |
| 13 | inc5 | 742 | 1365 | 0.5436 |
| 14 | inc1 | 134 | 334 | 0.4012 |
| 14 | inc3 | 462 | 1101 | 0.4196 |
| 14 | inc5 | 837 | 1709 | 0.4898 |
| 15 | inc1 | 344 | 1149 | 0.2994 |
| 15 | inc3 | 1400 | 3358 | 0.4169 |
| 15 | inc5 | 2049 | 4574 | 0.4480 |
| 16 | inc1 | 879 | 2750 | 0.3196 |
| 16 | inc3 | 2708 | 5652 | 0.4791 |
| 16 | inc5 | 4777 | 9150 | 0.5221 |
| 17 | inc1 | 1941 | 3812 | 0.5092 |
| 17 | inc3 | 8429 | 11296 | 0.7462 |
| 17 | inc5 | 12448 | 15913 | 0.7823 |
| 18 | inc1 | 4346 | 7267 | 0.5980 |
| 18 | inc3 | 12064 | 15477 | 0.7795 |
| 18 | inc5 | 17976 | 22107 | 0.8131 |

### Inc-regional insert breakdown (timing line)

| Case | Delta | analyze(ms) | plan(ms) | rebuild(ms) | calibrate(ms) | total(ms) | full_insert(ms) | speedup(full/total) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | inc1 | 4.780 | 0.424 | 11.832 | 0.447 | 17.577 | 31.0 | 1.764 |
| 1 | inc3 | 3.833 | 0.324 | 9.710 | 0.287 | 14.416 | 27.1 | 1.882 |
| 1 | inc5 | 3.689 | 0.328 | 10.162 | 0.297 | 14.808 | 27.3 | 1.844 |
| 3 | inc1 | 2.325 | 0.202 | 2.703 | 0.066 | 5.425 | 31.3 | 5.771 |
| 3 | inc3 | 2.408 | 0.204 | 5.234 | 0.079 | 7.925 | 31.4 | 3.957 |
| 3 | inc5 | 2.357 | 0.205 | 5.275 | 0.085 | 7.964 | 31.4 | 3.939 |
| 4 | inc1 | 0.615 | 0.094 | 1.012 | 0.053 | 1.830 | 8.8 | 4.823 |
| 4 | inc3 | 0.589 | 0.093 | 1.033 | 0.059 | 1.807 | 7.9 | 4.387 |
| 4 | inc5 | 0.599 | 0.099 | 1.028 | 0.059 | 1.823 | 7.7 | 4.247 |
| 5 | inc1 | 0.523 | 0.115 | 2.433 | 0.081 | 3.260 | 10.9 | 3.340 |
| 5 | inc3 | 0.513 | 0.112 | 1.895 | 0.073 | 2.656 | 11.1 | 4.182 |
| 5 | inc5 | 0.506 | 0.112 | 1.732 | 0.071 | 2.462 | 11.0 | 4.455 |
| 6 | inc1 | 0.706 | 0.121 | 4.109 | 0.089 | 5.138 | 11.5 | 2.236 |
| 6 | inc3 | 0.676 | 0.116 | 3.686 | 0.086 | 4.614 | 11.7 | 2.544 |
| 6 | inc5 | 0.668 | 0.116 | 3.408 | 0.084 | 4.318 | 11.6 | 2.684 |
| 7 | inc1 | 0.845 | 0.141 | 4.520 | 0.096 | 5.612 | 11.5 | 2.054 |
| 7 | inc3 | 0.814 | 0.139 | 4.173 | 0.094 | 5.256 | 11.7 | 2.230 |
| 7 | inc5 | 1.098 | 0.153 | 4.453 | 0.104 | 5.879 | 18.1 | 3.081 |
| 8 | inc1 | 1.054 | 0.161 | 5.666 | 0.105 | 6.986 | 14.9 | 2.134 |
| 8 | inc3 | 1.067 | 0.165 | 5.589 | 0.106 | 6.926 | 15.7 | 2.271 |
| 8 | inc5 | 1.083 | 0.164 | 5.461 | 0.106 | 6.890 | 16.9 | 2.453 |
| 9 | inc1 | 6.662 | 0.694 | 19.017 | 0.493 | 26.942 | 129.0 | 4.787 |
| 9 | inc3 | 6.498 | 0.701 | 15.296 | 0.455 | 23.520 | 129.7 | 5.515 |
| 9 | inc5 | 6.638 | 0.702 | 16.041 | 0.477 | 24.004 | 130.0 | 5.416 |
| 10 | inc1 | 4.770 | 0.444 | 20.075 | 0.405 | 25.778 | 79.6 | 3.088 |
| 10 | inc3 | 4.694 | 0.439 | 20.658 | 0.403 | 26.228 | 81.8 | 3.119 |
| 10 | inc5 | 4.809 | 0.442 | 20.215 | 0.405 | 25.870 | 82.2 | 3.177 |
| 11 | inc1 | 4.403 | 0.427 | 20.746 | 0.391 | 26.132 | 79.0 | 3.023 |
| 11 | inc3 | 4.445 | 0.429 | 21.014 | 0.392 | 26.482 | 81.0 | 3.058 |
| 11 | inc5 | 4.593 | 0.439 | 22.412 | 0.403 | 28.083 | 86.0 | 3.064 |
| 12 | inc1 | 20.156 | 1.662 | 92.119 | 0.809 | 114.814 | 886.7 | 7.723 |
| 12 | inc3 | 20.056 | 1.666 | 92.066 | 0.813 | 114.677 | 889.7 | 7.757 |
| 12 | inc5 | 20.141 | 1.666 | 92.188 | 0.806 | 114.835 | 912.5 | 7.945 |
| 13 | inc1 | 44.691 | 3.657 | 212.378 | 1.473 | 262.207 | 2435.8 | 9.289 |
| 13 | inc3 | 44.714 | 3.658 | 213.249 | 1.469 | 263.318 | 2528.3 | 9.602 |
| 13 | inc5 | 44.635 | 3.649 | 212.797 | 1.480 | 262.561 | 2580.3 | 9.831 |
| 14 | inc1 | 88.745 | 6.288 | 469.320 | 2.531 | 566.889 | 5380.7 | 9.492 |
| 14 | inc3 | 88.944 | 6.289 | 469.724 | 2.533 | 567.490 | 5439.0 | 9.585 |
| 14 | inc5 | 88.969 | 6.290 | 472.274 | 2.540 | 570.823 | 5221.8 | 9.152 |
| 15 | inc1 | 256.421 | 19.372 | 1344.714 | 7.619 | 1633.793 | 11126.1 | 6.811 |
| 15 | inc3 | 256.422 | 19.381 | 1345.917 | 7.611 | 1635.542 | 11462.5 | 7.011 |
| 15 | inc5 | 256.403 | 19.384 | 1344.321 | 7.616 | 1633.481 | 11026.7 | 6.751 |
| 16 | inc1 | 446.569 | 35.106 | 2611.354 | 13.229 | 3106.258 | 17142.8 | 5.518 |
| 16 | inc3 | 446.596 | 35.099 | 2611.657 | 13.235 | 3106.588 | 17969.8 | 5.784 |
| 16 | inc5 | 446.468 | 35.105 | 2610.725 | 13.236 | 3105.820 | 18908.2 | 6.089 |
| 17 | inc1 | 299.751 | 24.095 | 1792.091 | 9.246 | 2125.186 | 10737.4 | 5.052 |
| 17 | inc3 | 299.830 | 24.121 | 1792.768 | 9.251 | 2126.395 | 12099.4 | 5.690 |
| 17 | inc5 | 299.728 | 24.109 | 1792.613 | 9.243 | 2125.694 | 12527.1 | 5.891 |
| 18 | inc1 | 418.179 | 32.340 | 2435.260 | 11.285 | 2897.190 | 21421.3 | 7.395 |
| 18 | inc3 | 418.323 | 32.349 | 2436.620 | 11.295 | 2899.446 | 22727.7 | 7.839 |
| 18 | inc5 | 418.416 | 32.373 | 2436.516 | 11.296 | 2898.968 | 23548.2 | 8.124 |

### Inc-regional rebuild sub-breakdown

| Case | Delta | snapshot(ms) | initNodes(ms) | initEdges(ms) | depGraph(ms) | regionCycles(ms) | indegree(ms) | rebuildLoop(ms) | total(ms) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | inc1 | 0.002 | 0.127 | 0.095 | 0.000 | 0.000 | 0.364 | 11.228 | 11.832 |
| 1 | inc3 | 0.001 | 0.105 | 0.078 | 0.000 | 0.000 | 0.320 | 9.206 | 9.710 |
| 1 | inc5 | 0.001 | 0.106 | 0.077 | 0.000 | 0.000 | 0.327 | 9.651 | 10.162 |
| 3 | inc1 | 0.001 | 0.029 | 0.028 | 0.000 | 0.000 | 0.071 | 2.558 | 2.703 |
| 3 | inc3 | 0.001 | 0.029 | 0.029 | 0.000 | 0.000 | 0.076 | 5.098 | 5.234 |
| 3 | inc5 | 0.001 | 0.029 | 0.029 | 0.000 | 0.000 | 0.076 | 5.140 | 5.275 |
| 4 | inc1 | 0.001 | 0.011 | 0.011 | 0.000 | 0.000 | 0.036 | 0.948 | 1.012 |
| 4 | inc3 | 0.001 | 0.011 | 0.011 | 0.000 | 0.000 | 0.036 | 0.969 | 1.033 |
| 4 | inc5 | 0.001 | 0.011 | 0.011 | 0.000 | 0.000 | 0.036 | 0.964 | 1.028 |
| 5 | inc1 | 0.001 | 0.012 | 0.012 | 0.000 | 0.000 | 0.037 | 2.359 | 2.433 |
| 5 | inc3 | 0.001 | 0.012 | 0.012 | 0.000 | 0.000 | 0.036 | 1.809 | 1.895 |
| 5 | inc5 | 0.001 | 0.012 | 0.012 | 0.000 | 0.000 | 0.037 | 1.646 | 1.732 |
| 6 | inc1 | 0.001 | 0.016 | 0.015 | 0.000 | 0.000 | 0.044 | 4.027 | 4.109 |
| 6 | inc3 | 0.001 | 0.016 | 0.015 | 0.000 | 0.000 | 0.043 | 3.602 | 3.686 |
| 6 | inc5 | 0.001 | 0.016 | 0.015 | 0.000 | 0.000 | 0.043 | 3.323 | 3.408 |
| 7 | inc1 | 0.001 | 0.018 | 0.018 | 0.000 | 0.000 | 0.048 | 4.431 | 4.520 |
| 7 | inc3 | 0.001 | 0.018 | 0.017 | 0.000 | 0.000 | 0.047 | 4.083 | 4.173 |
| 7 | inc5 | 0.001 | 0.019 | 0.018 | 0.000 | 0.000 | 0.048 | 4.362 | 4.453 |
| 8 | inc1 | 0.001 | 0.020 | 0.019 | 0.000 | 0.000 | 0.054 | 5.560 | 5.666 |
| 8 | inc3 | 0.001 | 0.020 | 0.019 | 0.000 | 0.000 | 0.054 | 5.481 | 5.589 |
| 8 | inc5 | 0.001 | 0.020 | 0.019 | 0.000 | 0.000 | 0.054 | 5.353 | 5.461 |
| 9 | inc1 | 0.001 | 0.055 | 0.056 | 0.000 | 0.000 | 0.168 | 18.741 | 19.017 |
| 9 | inc3 | 0.001 | 0.055 | 0.054 | 0.000 | 0.000 | 0.168 | 15.020 | 15.296 |
| 9 | inc5 | 0.001 | 0.055 | 0.055 | 0.000 | 0.000 | 0.168 | 15.764 | 16.041 |
| 10 | inc1 | 0.002 | 0.095 | 0.093 | 0.000 | 0.000 | 0.318 | 19.509 | 20.075 |
| 10 | inc3 | 0.002 | 0.095 | 0.093 | 0.000 | 0.000 | 0.317 | 20.092 | 20.658 |
| 10 | inc5 | 0.002 | 0.095 | 0.093 | 0.000 | 0.000 | 0.317 | 19.649 | 20.215 |
| 11 | inc1 | 0.002 | 0.091 | 0.091 | 0.000 | 0.000 | 0.306 | 20.256 | 20.746 |
| 11 | inc3 | 0.002 | 0.091 | 0.091 | 0.000 | 0.000 | 0.307 | 20.524 | 21.014 |
| 11 | inc5 | 0.002 | 0.091 | 0.092 | 0.000 | 0.000 | 0.313 | 21.926 | 22.412 |
| 12 | inc1 | 0.004 | 0.312 | 0.306 | 0.000 | 0.000 | 1.158 | 90.210 | 92.119 |
| 12 | inc3 | 0.004 | 0.312 | 0.307 | 0.000 | 0.000 | 1.158 | 90.158 | 92.066 |
| 12 | inc5 | 0.004 | 0.312 | 0.306 | 0.000 | 0.000 | 1.158 | 90.281 | 92.188 |
| 13 | inc1 | 0.004 | 0.639 | 0.635 | 0.000 | 0.000 | 2.477 | 208.519 | 212.378 |
| 13 | inc3 | 0.004 | 0.638 | 0.635 | 0.000 | 0.000 | 2.477 | 209.388 | 213.249 |
| 13 | inc5 | 0.004 | 0.639 | 0.635 | 0.000 | 0.000 | 2.477 | 208.936 | 212.797 |
| 14 | inc1 | 0.005 | 1.372 | 1.355 | 0.000 | 0.000 | 5.494 | 461.707 | 469.320 |
| 14 | inc3 | 0.005 | 1.372 | 1.355 | 0.000 | 0.000 | 5.494 | 462.112 | 469.724 |
| 14 | inc5 | 0.005 | 1.372 | 1.356 | 0.000 | 0.000 | 5.494 | 464.662 | 472.274 |
| 15 | inc1 | 0.010 | 3.575 | 3.448 | 0.000 | 0.000 | 14.825 | 1322.600 | 1344.714 |
| 15 | inc3 | 0.010 | 3.576 | 3.448 | 0.000 | 0.000 | 14.827 | 1323.802 | 1345.917 |
| 15 | inc5 | 0.010 | 3.575 | 3.448 | 0.000 | 0.000 | 14.826 | 1322.205 | 1344.321 |
| 16 | inc1 | 0.015 | 6.991 | 6.832 | 0.000 | 0.000 | 28.612 | 2565.995 | 2611.354 |
| 16 | inc3 | 0.015 | 6.992 | 6.832 | 0.000 | 0.000 | 28.613 | 2566.297 | 2611.657 |
| 16 | inc5 | 0.015 | 6.989 | 6.832 | 0.000 | 0.000 | 28.613 | 2565.365 | 2610.725 |
| 17 | inc1 | 0.009 | 5.419 | 5.283 | 0.000 | 0.000 | 21.602 | 1760.505 | 1792.091 |
| 17 | inc3 | 0.009 | 5.420 | 5.284 | 0.000 | 0.000 | 21.605 | 1761.181 | 1792.768 |
| 17 | inc5 | 0.009 | 5.419 | 5.284 | 0.000 | 0.000 | 21.605 | 1761.027 | 1792.613 |
| 18 | inc1 | 0.011 | 7.359 | 7.226 | 0.000 | 0.000 | 29.858 | 2397.399 | 2435.260 |
| 18 | inc3 | 0.011 | 7.359 | 7.227 | 0.000 | 0.000 | 29.860 | 2398.752 | 2436.620 |
| 18 | inc5 | 0.011 | 7.360 | 7.227 | 0.000 | 0.000 | 29.863 | 2398.648 | 2436.516 |

### Insert turn (turn3) stage breakdown, P16–P20 (seconds)
Speedup = inc-naive / inc-regional (>1 means regional faster).

| Case | Delta | Total naive | Total regional | Total speedup | FC naive | FC regional | FC speedup | WMC naive | WMC regional | WMC speedup |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| P16 | inc1 | 0.943404 | 0.707219 | 1.334 | 0.693407 | 0.381427 | 1.818 | 0.116416 | 0.195570 | 0.595 |
| P16 | inc3 | 1.667415 | 1.479019 | 1.127 | 1.298172 | 1.023659 | 1.268 | 0.218130 | 0.277173 | 0.787 |
| P16 | inc5 | 3.572262 | 2.039442 | 1.752 | 3.055969 | 1.513283 | 2.019 | 0.333717 | 0.352561 | 0.947 |
| P17 | inc1 | 0.667522 | 0.766043 | 0.871 | 0.416051 | 0.368965 | 1.128 | 0.048016 | 0.187641 | 0.256 |
| P17 | inc3 | 1.056711 | 1.208750 | 0.874 | 0.713071 | 0.755556 | 0.944 | 0.087453 | 0.216232 | 0.404 |
| P17 | inc5 | 1.388711 | 1.522169 | 0.912 | 1.004269 | 0.994367 | 1.010 | 0.111182 | 0.253803 | 0.438 |
| P18 | inc1 | 1.512640 | 1.692314 | 0.894 | 1.115896 | 1.116295 | 1.000 | 0.138622 | 0.323213 | 0.429 |
| P18 | inc3 | 2.798251 | 2.830424 | 0.989 | 2.170517 | 2.093830 | 1.037 | 0.292433 | 0.425363 | 0.687 |
| P18 | inc5 | 3.543495 | 3.642495 | 0.973 | 2.743835 | 2.769902 | 0.991 | 0.358734 | 0.518348 | 0.692 |
| P19 | inc1 | 2.698399 | 2.733751 | 0.987 | 2.032176 | 1.902201 | 1.068 | 0.333274 | 0.518634 | 0.643 |
| P19 | inc3 | 5.255922 | 5.635740 | 0.933 | 4.183679 | 4.411039 | 0.948 | 0.672353 | 0.863938 | 0.778 |
| P19 | inc5 | 7.995956 | 9.552599 | 0.837 | 6.702363 | 8.063939 | 0.831 | 0.838894 | 1.011941 | 0.829 |
| P20 | inc1 | 1.087686 | 1.273946 | 0.854 | 0.597261 | 0.346679 | 1.723 | 0.014324 | 0.449436 | 0.032 |
| P20 | inc3 | 1.427479 | 1.596030 | 0.894 | 0.765501 | 0.556664 | 1.375 | 0.018854 | 0.440507 | 0.043 |
| P20 | inc5 | 1.572029 | 1.794462 | 0.876 | 0.878196 | 0.692996 | 1.267 | 0.021435 | 0.478138 | 0.045 |

### Inc-regional analyze breakdown (top components, ms)
These are the largest subcomponents inside the inc-regional insert analyze
phase. The dominant cost is typically `reexpand`, followed by `prepare` and
`expand` (P20 inc1 is the main exception where `prepare` dominates).

- P16: inc1 prepare=40.40, reexpand=36.16, expand=24.38; inc3 reexpand=57.35, prepare=54.81, expand=44.55; inc5 reexpand=71.56, expand=48.07, prepare=40.01
- P17: inc1 reexpand=85.48, prepare=46.91; inc3 reexpand=158.10, prepare=46.95, expand=36.57; inc5 reexpand=183.34, prepare=47.02, expand=42.20
- P18: inc1 reexpand=244.96, prepare=67.62; inc3 reexpand=340.34, prepare=66.97; inc5 reexpand=345.85, prepare=87.04, expand=64.48
- P19: inc1 reexpand=473.09, prepare=85.13; inc3 reexpand=615.63, prepare=94.02, expand=57.46; inc5 reexpand=752.39, expand=272.12, prepare=135.85
- P20: inc1 prepare=127.74, classify=19.47, reclassify=16.36; inc3 reexpand=168.07, prepare=116.66, expand=13.94; inc5 reexpand=178.57, prepare=113.78, expand=58.27

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
  --timeout 1200 --run-arg=--det-opt --run-arg=--profile-inc-regional --compare-all

# collect
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0 collect
```

**Timestamps (from `operation_inc.log`)**
- Compile: 2026-01-20 13:32:43 → 2026-01-20 13:35:31
- Run (P15–P16): 2026-01-20 13:35:37 → 2026-01-20 01:51:01
- Run (P17–P20): 2026-01-20 13:43:28 → 2026-01-20 14:03:06
- Collect: 2026-01-20 14:03:29
- inc-analyze refresh (inc-regional only): 2026-01-20 11:29:40 → 11:30:04

### Run Results (end-to-end, elapsed_s)
turn1 turn2 turn3

| Case | Delta | inc-naive (s) | inc-regional (s) | full (s) | Status | Mismatches | Max |Δ| |
| --- | --- | ---: | ---: | ---: | --- | ---: | ---: |
| P1 | inc3 | 0.047 | 0.043 | 0.102 | OK | - | 0.00e+00 |
| P1 | inc6 | 0.046 | 0.044 | 0.102 | OK | - | 0.00e+00 |
| P1 | inc10 | 0.201 | 0.201 | 0.099 | OK | - | 0.00e+00 |
| P3 | inc3 | 0.183 | 0.187 | 0.083 | OK | - | 0.00e+00 |
| P3 | inc6 | 0.182 | 0.182 | 0.081 | OK | - | 0.00e+00 |
| P3 | inc10 | 0.183 | 0.186 | 0.081 | OK | - | 0.00e+00 |
| P4 | inc3 | 0.017 | 0.014 | 0.011 | OK | - | 0.00e+00 |
| P4 | inc6 | 0.022 | 0.011 | 0.011 | OK | - | 0.00e+00 |
| P4 | inc10 | 0.013 | 0.014 | 0.010 | OK | - | 0.00e+00 |
| P5 | inc3 | 0.020 | 0.018 | 0.014 | OK | - | 0.00e+00 |
| P5 | inc6 | 0.018 | 0.021 | 0.013 | OK | - | 0.00e+00 |
| P5 | inc10 | 0.023 | 0.023 | 0.014 | OK | - | 0.00e+00 |
| P6 | inc3 | 0.035 | 0.036 | 0.023 | OK | - | 0.00e+00 |
| P6 | inc6 | 0.037 | 0.035 | 0.025 | OK | - | 0.00e+00 |
| P6 | inc10 | 0.038 | 0.038 | 0.023 | OK | - | 0.00e+00 |
| P7 | inc3 | 0.049 | 0.042 | 0.032 | OK | - | 0.00e+00 |
| P7 | inc6 | 0.041 | 0.055 | 0.031 | OK | - | 0.00e+00 |
| P7 | inc10 | 0.042 | 0.042 | 0.029 | OK | - | 0.00e+00 |
| P8 | inc3 | 0.052 | 0.059 | 0.042 | OK | - | 0.00e+00 |
| P8 | inc6 | 0.050 | 0.050 | 0.041 | OK | - | 0.00e+00 |
| P8 | inc10 | 0.052 | 0.052 | 0.041 | OK | - | 0.00e+00 |
| P9 | inc3 | 0.156 | 0.165 | 0.320 | OK | - | 0.00e+00 |
| P9 | inc6 | 0.163 | 0.167 | 0.389 | OK | - | 0.00e+00 |
| P9 | inc10 | 0.156 | 0.158 | 0.306 | OK | - | 0.00e+00 |
| P10 | inc3 | 0.091 | 0.098 | 0.142 | OK | - | 0.00e+00 |
| P10 | inc6 | 0.099 | 0.098 | 0.140 | OK | - | 0.00e+00 |
| P10 | inc10 | 0.101 | 0.103 | 0.147 | OK | - | 0.00e+00 |
| P11 | inc3 | 0.083 | 0.080 | 0.137 | OK | - | 0.00e+00 |
| P11 | inc6 | 0.082 | 0.086 | 0.132 | OK | - | 0.00e+00 |
| P11 | inc10 | 0.085 | 0.092 | 0.128 | OK | - | 0.00e+00 |
| P12 | inc3 | 0.564 | 0.542 | 1.469 | OK | - | 0.00e+00 |
| P12 | inc6 | 0.587 | 0.599 | 1.432 | OK | - | 0.00e+00 |
| P12 | inc10 | 0.624 | 0.660 | 1.567 | OK | - | 0.00e+00 |
| P13 | inc3 | 1.790 | 1.678 | 4.234 | OK | - | 0.00e+00 |
| P13 | inc6 | 1.780 | 1.730 | 4.304 | OK | - | 0.00e+00 |
| P13 | inc10 | 1.985 | 1.929 | 3.958 | OK | - | 0.00e+00 |
| P14 | inc3 | 3.881 | 4.066 | 9.483 | OK | - | 0.00e+00 |
| P14 | inc6 | 4.284 | 4.225 | 9.517 | OK | - | 0.00e+00 |
| P14 | inc10 | 4.605 | 4.633 | 9.435 | OK | - | 0.00e+00 |


**Speedup summary (inc-regional vs prior compare-all run)**
- Median inc-regional speedup vs previous table: 1.10x (min 0.80x, max 1.92x, n=39).
- Reference baseline: 2026-01-20 11:03 compare-all run (same dataset C).

**Profiling summary (turn3 insert, inc-regional)**
- Old profile logs (2026-01-20 12:14, n=25): median analyze=0.479 ms, scc_close=0.609 ms (48.9%), plan=0.049 ms, rebuild=0.075 ms, total=1.221 ms.
- New compare-all logs (2026-01-20 13:17, n=27): median analyze=0.556 ms, scc_close=0.016 ms (0.76%), plan=0.061 ms, rebuild=1.003 ms, total=1.509 ms.
- Detailed per-delta timings: `problog-benchmark/side_channel_inc_strengthen_p1_14_d003_d006_d010_seed0/inc_regional_fc_subcalc.tsv`.

### Per-turn Per-stage (seconds; turn2=del, turn3=ins)

| Case | Delta | Mode | Phase | Total (s) | SEM | PRN | FC | WMC |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| P1 | inc3 | full | del | 0.033327 | 0.029547 | 0.000175 | 0.000826 | 0.000029 |
| P1 | inc3 | full | ins | 0.032255 | 0.030334 | 0.000172 | 0.000840 | 0.000030 |
| P1 | inc3 | inc-naive | del | 0.003276 | 0.002504 | 0.000208 | 0.000364 | 0.000060 |
| P1 | inc3 | inc-naive | ins | 0.004547 | 0.003626 | 0.000277 | 0.000418 | 0.000055 |
| P1 | inc3 | inc-regional | del | 0.002966 | 0.002266 | 0.000184 | 0.000031 | 0.000358 |
| P1 | inc3 | inc-regional | ins | 0.004580 | 0.003777 | 0.000234 | 0.000037 | 0.000373 |
| P1 | inc6 | full | del | 0.034142 | 0.030408 | 0.000154 | 0.000876 | 0.000031 |
| P1 | inc6 | full | ins | 0.031774 | 0.029886 | 0.000166 | 0.000837 | 0.000029 |
| P1 | inc6 | inc-naive | del | 0.003509 | 0.002758 | 0.000223 | 0.000357 | 0.000045 |
| P1 | inc6 | inc-naive | ins | 0.004564 | 0.003739 | 0.000231 | 0.000384 | 0.000055 |
| P1 | inc6 | inc-regional | del | 0.002392 | 0.001648 | 0.000214 | 0.000030 | 0.000366 |
| P1 | inc6 | inc-regional | ins | 0.004685 | 0.003842 | 0.000249 | 0.000065 | 0.000373 |
| P1 | inc10 | full | del | 0.031935 | 0.028305 | 0.000153 | 0.000811 | 0.000029 |
| P1 | inc10 | full | ins | 0.031419 | 0.029631 | 0.000167 | 0.000855 | 0.000028 |
| P1 | inc10 | inc-naive | del | 0.156112 | 0.154191 | 0.001424 | 0.000329 | 0.000040 |
| P1 | inc10 | inc-naive | ins | 0.006463 | 0.004976 | 0.000702 | 0.000585 | 0.000049 |
| P1 | inc10 | inc-regional | del | 0.160838 | 0.157993 | 0.001920 | 0.000099 | 0.000662 |
| P1 | inc10 | inc-regional | ins | 0.004412 | 0.003573 | 0.000371 | 0.000031 | 0.000309 |
| P3 | inc3 | full | del | 0.018452 | 0.014918 | 0.000189 | 0.000896 | 0.000030 |
| P3 | inc3 | full | ins | 0.030073 | 0.028336 | 0.000173 | 0.000896 | 0.000028 |
| P3 | inc3 | inc-naive | del | 0.118428 | 0.113962 | 0.003951 | 0.000345 | 0.000043 |
| P3 | inc3 | inc-naive | ins | 0.029219 | 0.026435 | 0.002226 | 0.000387 | 0.000042 |
| P3 | inc3 | inc-regional | del | 0.119288 | 0.115030 | 0.003753 | 0.000032 | 0.000345 |
| P3 | inc3 | inc-regional | ins | 0.028831 | 0.026233 | 0.002098 | 0.000030 | 0.000337 |
| P3 | inc6 | full | del | 0.019121 | 0.015452 | 0.000173 | 0.000898 | 0.000030 |
| P3 | inc6 | full | ins | 0.028320 | 0.026535 | 0.000173 | 0.000894 | 0.000028 |
| P3 | inc6 | inc-naive | del | 0.117533 | 0.113282 | 0.003726 | 0.000355 | 0.000043 |
| P3 | inc6 | inc-naive | ins | 0.028062 | 0.025108 | 0.002460 | 0.000324 | 0.000041 |
| P3 | inc6 | inc-regional | del | 0.118644 | 0.114364 | 0.003745 | 0.000034 | 0.000353 |
| P3 | inc6 | inc-regional | ins | 0.029496 | 0.026856 | 0.002126 | 0.000031 | 0.000354 |
| P3 | inc10 | full | del | 0.018637 | 0.015170 | 0.000150 | 0.000904 | 0.000029 |
| P3 | inc10 | full | ins | 0.029227 | 0.027438 | 0.000174 | 0.000920 | 0.000029 |
| P3 | inc10 | inc-naive | del | 0.118112 | 0.113775 | 0.003812 | 0.000339 | 0.000043 |
| P3 | inc10 | inc-naive | ins | 0.028010 | 0.025372 | 0.002148 | 0.000320 | 0.000041 |
| P3 | inc10 | inc-regional | del | 0.119029 | 0.114392 | 0.004024 | 0.000044 | 0.000428 |
| P3 | inc10 | inc-regional | ins | 0.033114 | 0.029937 | 0.002669 | 0.000031 | 0.000341 |
| P4 | inc3 | full | del | 0.002685 | 0.000424 | 0.000079 | 0.000092 | 0.000023 |
| P4 | inc3 | full | ins | 0.001070 | 0.000406 | 0.000078 | 0.000088 | 0.000022 |
| P4 | inc3 | inc-naive | del | 0.003356 | 0.002769 | 0.000175 | 0.000038 | 0.000029 |
| P4 | inc3 | inc-naive | ins | 0.004480 | 0.004097 | 0.000161 | 0.000044 | 0.000030 |
| P4 | inc3 | inc-regional | del | 0.001726 | 0.001401 | 0.000137 | 0.000034 | 0.000030 |
| P4 | inc3 | inc-regional | ins | 0.005030 | 0.004407 | 0.000376 | 0.000048 | 0.000036 |
| P4 | inc6 | full | del | 0.002805 | 0.000427 | 0.000079 | 0.000084 | 0.000023 |
| P4 | inc6 | full | ins | 0.001123 | 0.000445 | 0.000080 | 0.000091 | 0.000023 |
| P4 | inc6 | inc-naive | del | 0.003275 | 0.002861 | 0.000187 | 0.000047 | 0.000030 |
| P4 | inc6 | inc-naive | ins | 0.010013 | 0.009697 | 0.000132 | 0.000037 | 0.000024 |
| P4 | inc6 | inc-regional | del | 0.001536 | 0.001220 | 0.000136 | 0.000029 | 0.000030 |
| P4 | inc6 | inc-regional | ins | 0.004419 | 0.004097 | 0.000133 | 0.000030 | 0.000030 |
| P4 | inc10 | full | del | 0.002741 | 0.000446 | 0.000082 | 0.000096 | 0.000023 |
| P4 | inc10 | full | ins | 0.001095 | 0.000437 | 0.000080 | 0.000091 | 0.000023 |
| P4 | inc10 | inc-naive | del | 0.001200 | 0.000904 | 0.000117 | 0.000035 | 0.000024 |
| P4 | inc10 | inc-naive | ins | 0.004708 | 0.004323 | 0.000177 | 0.000043 | 0.000023 |
| P4 | inc10 | inc-regional | del | 0.001856 | 0.001526 | 0.000148 | 0.000029 | 0.000030 |
| P4 | inc10 | inc-regional | ins | 0.004399 | 0.004083 | 0.000131 | 0.000030 | 0.000029 |
| P5 | inc3 | full | del | 0.002904 | 0.000457 | 0.000090 | 0.000266 | 0.000029 |
| P5 | inc3 | full | ins | 0.001382 | 0.000467 | 0.000097 | 0.000279 | 0.000030 |
| P5 | inc3 | inc-naive | del | 0.005844 | 0.004841 | 0.000150 | 0.000685 | 0.000034 |
| P5 | inc3 | inc-naive | ins | 0.004062 | 0.003430 | 0.000143 | 0.000330 | 0.000033 |
| P5 | inc3 | inc-regional | del | 0.004776 | 0.003701 | 0.000143 | 0.000772 | 0.000034 |
| P5 | inc3 | inc-regional | ins | 0.004357 | 0.003502 | 0.000198 | 0.000458 | 0.000041 |
| P5 | inc6 | full | del | 0.002837 | 0.000456 | 0.000090 | 0.000269 | 0.000029 |
| P5 | inc6 | full | ins | 0.001350 | 0.000419 | 0.000098 | 0.000296 | 0.000030 |
| P5 | inc6 | inc-naive | del | 0.004655 | 0.003633 | 0.000169 | 0.000690 | 0.000034 |
| P5 | inc6 | inc-naive | ins | 0.004101 | 0.003427 | 0.000143 | 0.000373 | 0.000033 |
| P5 | inc6 | inc-regional | del | 0.005219 | 0.003859 | 0.000231 | 0.000902 | 0.000045 |
| P5 | inc6 | inc-regional | ins | 0.006160 | 0.005477 | 0.000162 | 0.000355 | 0.000033 |
| P5 | inc10 | full | del | 0.003199 | 0.000452 | 0.000113 | 0.000279 | 0.000030 |
| P5 | inc10 | full | ins | 0.001382 | 0.000428 | 0.000113 | 0.000280 | 0.000030 |
| P5 | inc10 | inc-naive | del | 0.005963 | 0.004750 | 0.000202 | 0.000821 | 0.000041 |
| P5 | inc10 | inc-naive | ins | 0.003884 | 0.003174 | 0.000151 | 0.000362 | 0.000041 |
| P5 | inc10 | inc-regional | del | 0.006074 | 0.004818 | 0.000213 | 0.000836 | 0.000047 |
| P5 | inc10 | inc-regional | ins | 0.006631 | 0.005604 | 0.000336 | 0.000446 | 0.000059 |
| P6 | inc3 | full | del | 0.004076 | 0.000977 | 0.000160 | 0.000677 | 0.000046 |
| P6 | inc3 | full | ins | 0.002615 | 0.000910 | 0.000190 | 0.000779 | 0.000052 |
| P6 | inc3 | inc-naive | del | 0.010889 | 0.009649 | 0.000424 | 0.000633 | 0.000030 |
| P6 | inc3 | inc-naive | ins | 0.009203 | 0.007937 | 0.000466 | 0.000615 | 0.000032 |
| P6 | inc3 | inc-regional | del | 0.010298 | 0.009440 | 0.000226 | 0.000476 | 0.000030 |
| P6 | inc3 | inc-regional | ins | 0.009481 | 0.008396 | 0.000229 | 0.000692 | 0.000030 |
| P6 | inc6 | full | del | 0.005564 | 0.000986 | 0.000161 | 0.000749 | 0.000048 |
| P6 | inc6 | full | ins | 0.003184 | 0.000888 | 0.000135 | 0.000719 | 0.000050 |
| P6 | inc6 | inc-naive | del | 0.010502 | 0.009408 | 0.000429 | 0.000485 | 0.000031 |
| P6 | inc6 | inc-naive | ins | 0.009173 | 0.008113 | 0.000276 | 0.000620 | 0.000032 |
| P6 | inc6 | inc-regional | del | 0.010372 | 0.009501 | 0.000237 | 0.000467 | 0.000030 |
| P6 | inc6 | inc-regional | ins | 0.009397 | 0.008265 | 0.000249 | 0.000707 | 0.000031 |
| P6 | inc10 | full | del | 0.004272 | 0.000998 | 0.000161 | 0.000695 | 0.000067 |
| P6 | inc10 | full | ins | 0.002641 | 0.000923 | 0.000171 | 0.000733 | 0.000050 |
| P6 | inc10 | inc-naive | del | 0.010539 | 0.009410 | 0.000253 | 0.000696 | 0.000050 |
| P6 | inc10 | inc-naive | ins | 0.011923 | 0.010569 | 0.000388 | 0.000768 | 0.000044 |
| P6 | inc10 | inc-regional | del | 0.010318 | 0.008843 | 0.000442 | 0.000807 | 0.000061 |
| P6 | inc10 | inc-regional | ins | 0.011132 | 0.009628 | 0.000350 | 0.000941 | 0.000062 |
| P7 | inc3 | full | del | 0.007735 | 0.003072 | 0.000317 | 0.001318 | 0.000072 |
| P7 | inc3 | full | ins | 0.005104 | 0.002549 | 0.000441 | 0.001081 | 0.000072 |
| P7 | inc3 | inc-naive | del | 0.011183 | 0.009030 | 0.000465 | 0.001469 | 0.000079 |
| P7 | inc3 | inc-naive | ins | 0.016991 | 0.015524 | 0.000453 | 0.000828 | 0.000044 |
| P7 | inc3 | inc-regional | del | 0.010898 | 0.008911 | 0.000473 | 0.001303 | 0.000062 |
| P7 | inc3 | inc-regional | ins | 0.010727 | 0.009009 | 0.000406 | 0.001082 | 0.000056 |
| P7 | inc6 | full | del | 0.006692 | 0.002609 | 0.000263 | 0.001097 | 0.000068 |
| P7 | inc6 | full | ins | 0.005167 | 0.002624 | 0.000260 | 0.001096 | 0.000074 |
| P7 | inc6 | inc-naive | del | 0.010927 | 0.008476 | 0.000400 | 0.001851 | 0.000065 |
| P7 | inc6 | inc-naive | ins | 0.011808 | 0.010562 | 0.000327 | 0.000737 | 0.000047 |
| P7 | inc6 | inc-regional | del | 0.010080 | 0.008092 | 0.000352 | 0.001442 | 0.000062 |
| P7 | inc6 | inc-regional | ins | 0.025943 | 0.024235 | 0.000433 | 0.001074 | 0.000051 |
| P7 | inc10 | full | del | 0.005827 | 0.002585 | 0.000265 | 0.001062 | 0.000071 |
| P7 | inc10 | full | ins | 0.004659 | 0.002337 | 0.000217 | 0.001064 | 0.000074 |
| P7 | inc10 | inc-naive | del | 0.013280 | 0.010039 | 0.000534 | 0.002475 | 0.000077 |
| P7 | inc10 | inc-naive | ins | 0.011637 | 0.010228 | 0.000372 | 0.000864 | 0.000046 |
| P7 | inc10 | inc-regional | del | 0.012063 | 0.009856 | 0.000385 | 0.001621 | 0.000062 |
| P7 | inc10 | inc-regional | ins | 0.012166 | 0.010261 | 0.000432 | 0.001280 | 0.000049 |
| P8 | inc3 | full | del | 0.009986 | 0.005081 | 0.000339 | 0.001583 | 0.000093 |
| P8 | inc3 | full | ins | 0.008254 | 0.005026 | 0.000327 | 0.001619 | 0.000102 |
| P8 | inc3 | inc-naive | del | 0.016812 | 0.010811 | 0.000537 | 0.005095 | 0.000205 |
| P8 | inc3 | inc-naive | ins | 0.012540 | 0.010027 | 0.000531 | 0.001769 | 0.000064 |
| P8 | inc3 | inc-regional | del | 0.015039 | 0.010560 | 0.000565 | 0.003674 | 0.000092 |
| P8 | inc3 | inc-regional | ins | 0.021787 | 0.019261 | 0.000637 | 0.001666 | 0.000065 |
| P8 | inc6 | full | del | 0.009873 | 0.005188 | 0.000327 | 0.001531 | 0.000118 |
| P8 | inc6 | full | ins | 0.008763 | 0.005069 | 0.000432 | 0.001831 | 0.000160 |
| P8 | inc6 | inc-naive | del | 0.016524 | 0.010065 | 0.000513 | 0.005714 | 0.000098 |
| P8 | inc6 | inc-naive | ins | 0.012617 | 0.009923 | 0.000632 | 0.001826 | 0.000080 |
| P8 | inc6 | inc-regional | del | 0.015212 | 0.009716 | 0.000666 | 0.004587 | 0.000105 |
| P8 | inc6 | inc-regional | ins | 0.012803 | 0.009940 | 0.000475 | 0.002154 | 0.000093 |
| P8 | inc10 | full | del | 0.010204 | 0.005605 | 0.000316 | 0.001547 | 0.000107 |
| P8 | inc10 | full | ins | 0.008052 | 0.004909 | 0.000284 | 0.001605 | 0.000101 |
| P8 | inc10 | inc-naive | del | 0.017573 | 0.010634 | 0.000659 | 0.006049 | 0.000092 |
| P8 | inc10 | inc-naive | ins | 0.012722 | 0.010119 | 0.000549 | 0.001779 | 0.000087 |
| P8 | inc10 | inc-regional | del | 0.015509 | 0.010179 | 0.000612 | 0.004478 | 0.000101 |
| P8 | inc10 | inc-regional | ins | 0.013307 | 0.010153 | 0.000645 | 0.002239 | 0.000112 |
| P9 | inc3 | full | del | 0.110126 | 0.003911 | 0.000379 | 0.101392 | 0.000134 |
| P9 | inc3 | full | ins | 0.077184 | 0.003310 | 0.000352 | 0.070945 | 0.000127 |
| P9 | inc3 | inc-naive | del | 0.011560 | 0.008603 | 0.000626 | 0.002126 | 0.000069 |
| P9 | inc3 | inc-naive | ins | 0.012278 | 0.010033 | 0.000581 | 0.001481 | 0.000052 |
| P9 | inc3 | inc-regional | del | 0.011623 | 0.008669 | 0.000667 | 0.002033 | 0.000098 |
| P9 | inc3 | inc-regional | ins | 0.012439 | 0.009964 | 0.000495 | 0.001796 | 0.000051 |
| P9 | inc6 | full | del | 0.131475 | 0.003776 | 0.000427 | 0.123427 | 0.000117 |
| P9 | inc6 | full | ins | 0.126140 | 0.003266 | 0.000310 | 0.120863 | 0.000118 |
| P9 | inc6 | inc-naive | del | 0.014419 | 0.009918 | 0.001141 | 0.003126 | 0.000089 |
| P9 | inc6 | inc-naive | ins | 0.012705 | 0.009906 | 0.000579 | 0.002016 | 0.000072 |
| P9 | inc6 | inc-regional | del | 0.015504 | 0.011690 | 0.000770 | 0.002812 | 0.000081 |
| P9 | inc6 | inc-regional | ins | 0.014689 | 0.011305 | 0.000623 | 0.002554 | 0.000061 |
| P9 | inc10 | full | del | 0.062011 | 0.003385 | 0.000350 | 0.054452 | 0.000145 |
| P9 | inc10 | full | ins | 0.114801 | 0.003975 | 0.000413 | 0.108678 | 0.000118 |
| P9 | inc10 | inc-naive | del | 0.014834 | 0.010594 | 0.000683 | 0.003358 | 0.000069 |
| P9 | inc10 | inc-naive | ins | 0.014105 | 0.011351 | 0.000683 | 0.001868 | 0.000064 |
| P9 | inc10 | inc-regional | del | 0.018421 | 0.013134 | 0.000946 | 0.004083 | 0.000105 |
| P9 | inc10 | inc-regional | ins | 0.014690 | 0.011411 | 0.000699 | 0.002364 | 0.000061 |
| P10 | inc3 | full | del | 0.041681 | 0.036236 | 0.001333 | 0.000339 | 0.000033 |
| P10 | inc3 | full | ins | 0.037809 | 0.034464 | 0.001129 | 0.000337 | 0.000033 |
| P10 | inc3 | inc-naive | del | 0.021178 | 0.018931 | 0.001804 | 0.000283 | 0.000032 |
| P10 | inc3 | inc-naive | ins | 0.012239 | 0.010174 | 0.001609 | 0.000281 | 0.000033 |
| P10 | inc3 | inc-regional | del | 0.023759 | 0.020501 | 0.002754 | 0.000336 | 0.000034 |
| P10 | inc3 | inc-regional | ins | 0.012702 | 0.010186 | 0.001978 | 0.000372 | 0.000034 |
| P10 | inc6 | full | del | 0.040701 | 0.034846 | 0.001169 | 0.000340 | 0.000034 |
| P10 | inc6 | full | ins | 0.037318 | 0.033975 | 0.001051 | 0.000339 | 0.000034 |
| P10 | inc6 | inc-naive | del | 0.025754 | 0.021892 | 0.003347 | 0.000350 | 0.000035 |
| P10 | inc6 | inc-naive | ins | 0.012594 | 0.010073 | 0.002092 | 0.000268 | 0.000032 |
| P10 | inc6 | inc-regional | del | 0.024273 | 0.021279 | 0.002481 | 0.000343 | 0.000039 |
| P10 | inc6 | inc-regional | ins | 0.013080 | 0.010402 | 0.002150 | 0.000359 | 0.000034 |
| P10 | inc10 | full | del | 0.044395 | 0.038555 | 0.001272 | 0.000302 | 0.000036 |
| P10 | inc10 | full | ins | 0.043220 | 0.038737 | 0.001674 | 0.000429 | 0.000043 |
| P10 | inc10 | inc-naive | del | 0.027303 | 0.023542 | 0.003178 | 0.000420 | 0.000034 |
| P10 | inc10 | inc-naive | ins | 0.014036 | 0.010843 | 0.002702 | 0.000326 | 0.000034 |
| P10 | inc10 | inc-regional | del | 0.028607 | 0.024157 | 0.003808 | 0.000465 | 0.000033 |
| P10 | inc10 | inc-regional | ins | 0.014242 | 0.010701 | 0.002938 | 0.000437 | 0.000034 |
| P11 | inc3 | full | del | 0.043773 | 0.037833 | 0.001715 | 0.000216 | 0.000032 |
| P11 | inc3 | full | ins | 0.039734 | 0.036170 | 0.001271 | 0.000187 | 0.000029 |
| P11 | inc3 | inc-naive | del | 0.019850 | 0.016711 | 0.002886 | 0.000091 | 0.000033 |
| P11 | inc3 | inc-naive | ins | 0.008694 | 0.005904 | 0.002530 | 0.000089 | 0.000031 |
| P11 | inc3 | inc-regional | del | 0.018744 | 0.016774 | 0.001707 | 0.000031 | 0.000085 |
| P11 | inc3 | inc-regional | ins | 0.008167 | 0.005694 | 0.002194 | 0.000033 | 0.000113 |
| P11 | inc6 | full | del | 0.040539 | 0.035247 | 0.001552 | 0.000197 | 0.000029 |
| P11 | inc6 | full | ins | 0.037775 | 0.034506 | 0.001066 | 0.000176 | 0.000027 |
| P11 | inc6 | inc-naive | del | 0.021125 | 0.018598 | 0.002284 | 0.000086 | 0.000031 |
| P11 | inc6 | inc-naive | ins | 0.008463 | 0.005801 | 0.002403 | 0.000084 | 0.000050 |
| P11 | inc6 | inc-regional | del | 0.021603 | 0.018803 | 0.002536 | 0.000043 | 0.000089 |
| P11 | inc6 | inc-regional | ins | 0.007836 | 0.005676 | 0.001912 | 0.000031 | 0.000090 |
| P11 | inc10 | full | del | 0.037466 | 0.032805 | 0.001027 | 0.000183 | 0.000027 |
| P11 | inc10 | full | ins | 0.035712 | 0.032358 | 0.001355 | 0.000242 | 0.000033 |
| P11 | inc10 | inc-naive | del | 0.024962 | 0.021649 | 0.003061 | 0.000093 | 0.000032 |
| P11 | inc10 | inc-naive | ins | 0.009331 | 0.006691 | 0.002388 | 0.000086 | 0.000031 |
| P11 | inc10 | inc-regional | del | 0.025400 | 0.021880 | 0.003251 | 0.000035 | 0.000096 |
| P11 | inc10 | inc-regional | ins | 0.013700 | 0.008288 | 0.005129 | 0.000037 | 0.000102 |
| P12 | inc3 | full | del | 0.468937 | 0.053643 | 0.003128 | 0.387558 | 0.000764 |
| P12 | inc3 | full | ins | 0.524494 | 0.076303 | 0.005038 | 0.413475 | 0.000766 |
| P12 | inc3 | inc-naive | del | 0.044428 | 0.015022 | 0.005793 | 0.022966 | 0.000502 |
| P12 | inc3 | inc-naive | ins | 0.028993 | 0.009261 | 0.003775 | 0.015459 | 0.000345 |
| P12 | inc3 | inc-regional | del | 0.038680 | 0.015315 | 0.004972 | 0.017832 | 0.000411 |
| P12 | inc3 | inc-regional | ins | 0.029149 | 0.009564 | 0.002933 | 0.016218 | 0.000283 |
| P12 | inc6 | full | del | 0.467552 | 0.053436 | 0.002588 | 0.386524 | 0.000832 |
| P12 | inc6 | full | ins | 0.489382 | 0.054560 | 0.002589 | 0.383755 | 0.000841 |
| P12 | inc6 | inc-naive | del | 0.068897 | 0.024774 | 0.005690 | 0.037704 | 0.000572 |
| P12 | inc6 | inc-naive | ins | 0.032700 | 0.013387 | 0.003715 | 0.015045 | 0.000398 |
| P12 | inc6 | inc-regional | del | 0.068513 | 0.026455 | 0.006883 | 0.034203 | 0.000821 |
| P12 | inc6 | inc-regional | ins | 0.042318 | 0.015350 | 0.004779 | 0.021250 | 0.000757 |
| P12 | inc10 | full | del | 0.539893 | 0.064029 | 0.003322 | 0.442985 | 0.000758 |
| P12 | inc10 | full | ins | 0.485034 | 0.051262 | 0.002100 | 0.399797 | 0.000757 |
| P12 | inc10 | inc-naive | del | 0.099271 | 0.029409 | 0.010408 | 0.058293 | 0.001008 |
| P12 | inc10 | inc-naive | ins | 0.041933 | 0.015929 | 0.006252 | 0.019070 | 0.000510 |
| P12 | inc10 | inc-regional | del | 0.080465 | 0.028065 | 0.007073 | 0.044382 | 0.000794 |
| P12 | inc10 | inc-regional | ins | 0.053409 | 0.020234 | 0.007943 | 0.024343 | 0.000628 |
| P13 | inc3 | full | del | 1.416301 | 0.106255 | 0.006794 | 1.264833 | 0.003944 |
| P13 | inc3 | full | ins | 1.370900 | 0.102218 | 0.005221 | 1.202657 | 0.004348 |
| P13 | inc3 | inc-naive | del | 0.159259 | 0.043701 | 0.015171 | 0.094852 | 0.005371 |
| P13 | inc3 | inc-naive | ins | 0.091867 | 0.026837 | 0.014449 | 0.047588 | 0.002784 |
| P13 | inc3 | inc-regional | del | 0.128717 | 0.037783 | 0.011008 | 0.074377 | 0.005375 |
| P13 | inc3 | inc-regional | ins | 0.085853 | 0.024301 | 0.010576 | 0.048499 | 0.002275 |
| P13 | inc6 | full | del | 1.416393 | 0.097536 | 0.004602 | 1.276567 | 0.003962 |
| P13 | inc6 | full | ins | 1.419488 | 0.104031 | 0.005793 | 1.243437 | 0.004144 |
| P13 | inc6 | inc-naive | del | 0.223889 | 0.049020 | 0.013821 | 0.154739 | 0.006152 |
| P13 | inc6 | inc-naive | ins | 0.114136 | 0.039179 | 0.014455 | 0.056769 | 0.003471 |
| P13 | inc6 | inc-regional | del | 0.189892 | 0.047827 | 0.012103 | 0.125158 | 0.004647 |
| P13 | inc6 | inc-regional | ins | 0.107309 | 0.023712 | 0.011161 | 0.068830 | 0.003335 |
| P13 | inc10 | full | del | 1.061039 | 0.104225 | 0.005346 | 0.910913 | 0.003145 |
| P13 | inc10 | full | ins | 1.381738 | 0.099496 | 0.005353 | 1.221631 | 0.003706 |
| P13 | inc10 | inc-naive | del | 0.412047 | 0.072887 | 0.016575 | 0.316454 | 0.005973 |
| P13 | inc10 | inc-naive | ins | 0.125889 | 0.033590 | 0.014522 | 0.072523 | 0.004951 |
| P13 | inc10 | inc-regional | del | 0.364922 | 0.063556 | 0.013689 | 0.281839 | 0.005553 |
| P13 | inc10 | inc-regional | ins | 0.128798 | 0.026300 | 0.014338 | 0.083693 | 0.004129 |
| P14 | inc3 | full | del | 2.674232 | 0.140231 | 0.006897 | 2.340396 | 0.025870 |
| P14 | inc3 | full | ins | 3.363076 | 0.147598 | 0.007486 | 2.977080 | 0.031112 |
| P14 | inc3 | inc-naive | del | 0.370067 | 0.063686 | 0.017280 | 0.257708 | 0.031222 |
| P14 | inc3 | inc-naive | ins | 0.206415 | 0.034701 | 0.015811 | 0.134924 | 0.020690 |
| P14 | inc3 | inc-regional | del | 0.380698 | 0.062328 | 0.020217 | 0.259833 | 0.038125 |
| P14 | inc3 | inc-regional | ins | 0.266763 | 0.033238 | 0.017526 | 0.194063 | 0.021608 |
| P14 | inc6 | full | del | 2.695480 | 0.177799 | 0.012812 | 2.256663 | 0.017391 |
| P14 | inc6 | full | ins | 3.271488 | 0.147563 | 0.007102 | 2.936720 | 0.032622 |
| P14 | inc6 | inc-naive | del | 0.558780 | 0.063430 | 0.019804 | 0.433018 | 0.042348 |
| P14 | inc6 | inc-naive | ins | 0.324664 | 0.039373 | 0.023171 | 0.226617 | 0.035095 |
| P14 | inc6 | inc-regional | del | 0.491488 | 0.066593 | 0.020847 | 0.369960 | 0.033913 |
| P14 | inc6 | inc-regional | ins | 0.313857 | 0.040539 | 0.019420 | 0.227581 | 0.025940 |
| P14 | inc10 | full | del | 2.590025 | 0.146695 | 0.008784 | 2.246375 | 0.012644 |
| P14 | inc10 | full | ins | 3.462156 | 0.143104 | 0.006059 | 3.084330 | 0.041667 |
| P14 | inc10 | inc-naive | del | 0.919208 | 0.096508 | 0.026137 | 0.760270 | 0.036112 |
| P14 | inc10 | inc-naive | ins | 0.318080 | 0.048503 | 0.021389 | 0.216669 | 0.031055 |
| P14 | inc10 | inc-regional | del | 0.869053 | 0.097614 | 0.021662 | 0.713571 | 0.036030 |
| P14 | inc10 | inc-regional | ins | 0.396680 | 0.050101 | 0.020295 | 0.295168 | 0.030615 |


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


## Dataset D: `side_channel_inc_strengthen_fresh` (P15–P20 compare-all, 2026-01-20)

**Location**
`/home/hugh/research/datalog/souffle/problog-benchmark/side_channel_inc_strengthen_fresh`

**Notes**
- Compare-all run for P15–P20 only (inc1/inc3/inc5, 1 sample each).
- Mismatches observed:
  - P19 inc3: 102 mismatches (iter1 vs full); iter2 outputs missing (lhs/rhs missing).
  - P19 inc5: 21 mismatches (iter1 vs full); iter2 outputs missing (lhs/rhs missing).
  - P20 inc5: 30 mismatches (iter1 vs full).
- Performance (inc-naive / inc-regional, end-to-end): median 0.98x (min 0.96x, max 1.02x, n=16).
- Profiling summary (turn3 insert, inc-regional, n=18): median analyze=220.108 ms, scc_close=2.744 ms (0.20%),
  plan=197.684 ms, rebuild=495.154 ms (61.95%), total=894.252 ms.
- Detailed per-delta logs: `problog-benchmark/side_channel_inc_strengthen_fresh/P*/output/log_P*_inc*_inc-regional_*.json`.

**Commands used**
```bash
# compile
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH   python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_fresh   compile --cases 15-20 --timeout 1200 --jobs 4

# run (P15–P16)
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH   python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_fresh   run --cases 15-16 --delta-labels inc1,inc3,inc5 --delta-samples 1   --timeout 3600 --run-arg=--det-opt --run-arg=--profile-inc-regional --compare-all

# run (P17–P20)
PATH=/home/hugh/research/datalog/souffle/build/src:$PATH   python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_fresh   run --cases 17-20 --delta-labels inc1,inc3,inc5 --delta-samples 1   --timeout 3600 --run-arg=--det-opt --run-arg=--profile-inc-regional --compare-all

# collect
python3 side_channel_inc.py --base-dir side_channel_inc_strengthen_fresh collect
```

**Timestamps (from `operation_inc.log`)**
- Compile: 2026-01-20 13:32:43 → 2026-01-20 13:35:31
- Run (P15–P16): 2026-01-20 13:35:37 → 2026-01-20 13:43:07
- Run (P17–P20): 2026-01-20 13:43:28 → 2026-01-20 14:03:06
- Collect: 2026-01-20 14:03:29

### Run Results (end-to-end, elapsed_s)
turn1 turn2 turn3

| Case | Delta | inc-naive (s) | inc-regional (s) | full (s) | Status | Mismatches | Max |Δ| |
| --- | --- | ---: | ---: | ---: | --- | ---: | ---: |
| P15 | inc1 | 8.844 | 8.899 | 21.585 | OK | - | 0.00e+00 |
| P15 | inc3 | 9.792 | 9.820 | 21.818 | OK | - | 0.00e+00 |
| P15 | inc5 | 10.591 | 11.016 | 21.554 | OK | - | 0.00e+00 |
| P16 | inc1 | 15.054 | 14.817 | 34.303 | OK | - | 0.00e+00 |
| P16 | inc3 | 16.881 | 16.913 | 33.401 | OK | - | 0.00e+00 |
| P16 | inc5 | 20.268 | 21.048 | 33.074 | OK | - | 0.00e+00 |
| P17 | inc1 | 9.281 | 9.478 | 23.638 | OK | - | 0.00e+00 |
| P17 | inc3 | 10.777 | 10.812 | 21.191 | OK | - | 0.00e+00 |
| P17 | inc5 | 12.485 | 12.753 | 20.177 | OK | - | 0.00e+00 |
| P18 | inc1 | 20.022 | 20.468 | 45.474 | OK | - | 0.00e+00 |
| P18 | inc3 | 22.445 | 22.946 | 42.503 | OK | - | 0.00e+00 |
| P18 | inc5 | 25.244 | 26.047 | 42.762 | OK | - | 0.00e+00 |
| P19 | inc1 | 32.832 | 33.801 | 79.382 | OK | - | 0.00e+00 |
| P19 | inc3 | n/a | n/a | 82.722 | Mismatch | 102 | 6.93e-02 |
| P19 | inc5 | 53.393 | n/a | 68.234 | Mismatch | 21 | 6.54e-02 |
| P20 | inc1 | 11.174 | 11.520 | 23.145 | OK | - | 0.00e+00 |
| P20 | inc3 | 12.018 | 11.828 | 24.231 | OK | - | 0.00e+00 |
| P20 | inc5 | 12.964 | 12.749 | 22.992 | Mismatch | 30 | 6.85e-01 |
### Per-turn Per-stage (seconds; turn2=del, turn3=ins)

| Case | Delta | Mode | Phase | Total (s) | SEM | PRN | FC | WMC |
| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| P15 | inc1 | full | del | 7.125757 | 0.337245 | 0.016055 | 6.179383 | 0.224670 |
| P15 | inc1 | full | ins | 7.206223 | 0.347868 | 0.019939 | 6.261338 | 0.250089 |
| P15 | inc1 | inc-naive | del | 1.039214 | 0.077011 | 0.034597 | 0.776935 | 0.150411 |
| P15 | inc1 | inc-naive | ins | 0.353239 | 0.056263 | 0.032283 | 0.217858 | 0.046497 |
| P15 | inc1 | inc-regional | del | 1.019406 | 0.078371 | 0.032987 | 0.769878 | 0.137947 |
| P15 | inc1 | inc-regional | ins | 0.414695 | 0.063116 | 0.028543 | 0.275627 | 0.047053 |
| P15 | inc3 | full | del | 6.994714 | 0.334708 | 0.019003 | 6.089125 | 0.162117 |
| P15 | inc3 | full | ins | 7.250442 | 0.316328 | 0.016505 | 6.360750 | 0.257178 |
| P15 | inc3 | inc-naive | del | 1.704548 | 0.116525 | 0.033288 | 1.376535 | 0.177975 |
| P15 | inc3 | inc-naive | ins | 0.730891 | 0.078313 | 0.035359 | 0.495715 | 0.120833 |
| P15 | inc3 | inc-regional | del | 1.674250 | 0.115928 | 0.034164 | 1.339172 | 0.184753 |
| P15 | inc3 | inc-regional | ins | 0.831460 | 0.066753 | 0.030709 | 0.620073 | 0.113307 |
| P15 | inc5 | full | del | 6.959213 | 0.297192 | 0.014575 | 6.114092 | 0.156792 |
| P15 | inc5 | full | ins | 7.135519 | 0.333941 | 0.016971 | 6.238259 | 0.243443 |
| P15 | inc5 | inc-naive | del | 2.333372 | 0.150360 | 0.043013 | 1.944377 | 0.195387 |
| P15 | inc5 | inc-naive | ins | 0.864351 | 0.071861 | 0.037831 | 0.606151 | 0.147708 |
| P15 | inc5 | inc-regional | del | 2.330093 | 0.155883 | 0.038890 | 1.913522 | 0.221553 |
| P15 | inc5 | inc-regional | ins | 1.146372 | 0.068938 | 0.035353 | 0.895798 | 0.145524 |
| P16 | inc1 | full | del | 9.618209 | 0.451569 | 0.023874 | 8.291020 | 0.302745 |
| P16 | inc1 | full | ins | 12.207402 | 0.459384 | 0.023873 | 11.024652 | 0.375324 |
| P16 | inc1 | inc-naive | del | 1.574978 | 0.131390 | 0.052845 | 1.080677 | 0.309755 |
| P16 | inc1 | inc-naive | ins | 0.956592 | 0.089352 | 0.050472 | 0.700452 | 0.115644 |
| P16 | inc1 | inc-regional | del | 1.345499 | 0.124143 | 0.043578 | 0.892344 | 0.285147 |
| P16 | inc1 | inc-regional | ins | 1.019058 | 0.085516 | 0.047699 | 0.773434 | 0.111731 |
| P16 | inc3 | full | del | 8.702528 | 0.484123 | 0.030051 | 7.433662 | 0.222336 |
| P16 | inc3 | full | ins | 12.209346 | 0.462963 | 0.034086 | 10.932568 | 0.422287 |
| P16 | inc3 | inc-naive | del | 2.779016 | 0.198939 | 0.051509 | 2.210885 | 0.317293 |
| P16 | inc3 | inc-naive | ins | 1.633233 | 0.094232 | 0.049368 | 1.283617 | 0.204638 |
| P16 | inc3 | inc-regional | del | 2.772704 | 0.201992 | 0.049393 | 2.172400 | 0.348573 |
| P16 | inc3 | inc-regional | ins | 1.901950 | 0.097166 | 0.051235 | 1.540060 | 0.212410 |
| P16 | inc5 | full | del | 8.417196 | 0.454415 | 0.022677 | 7.248543 | 0.180964 |
| P16 | inc5 | full | ins | 12.281969 | 0.490989 | 0.031036 | 10.980500 | 0.442592 |
| P16 | inc5 | inc-naive | del | 4.447436 | 0.292512 | 0.072282 | 3.748080 | 0.334225 |
| P16 | inc5 | inc-naive | ins | 3.282505 | 0.109245 | 0.054729 | 2.809995 | 0.306897 |
| P16 | inc5 | inc-regional | del | 4.479403 | 0.305222 | 0.082897 | 3.728892 | 0.362062 |
| P16 | inc5 | inc-regional | ins | 3.887118 | 0.105834 | 0.062520 | 3.402266 | 0.314826 |
| P17 | inc1 | full | del | 7.753911 | 0.653824 | 0.056674 | 6.631467 | 0.109564 |
| P17 | inc1 | full | ins | 7.866537 | 0.677757 | 0.057303 | 6.707674 | 0.146890 |
| P17 | inc1 | inc-naive | del | 0.835641 | 0.223606 | 0.056622 | 0.482462 | 0.072618 |
| P17 | inc1 | inc-naive | ins | 0.588299 | 0.127723 | 0.054317 | 0.367148 | 0.038382 |
| P17 | inc1 | inc-regional | del | 0.825641 | 0.226557 | 0.062434 | 0.453125 | 0.083181 |
| P17 | inc1 | inc-regional | ins | 0.713563 | 0.129906 | 0.061118 | 0.483285 | 0.038537 |
| P17 | inc3 | full | del | 5.880337 | 0.595554 | 0.029028 | 4.890746 | 0.065466 |
| P17 | inc3 | full | ins | 7.488945 | 0.611624 | 0.028188 | 6.460539 | 0.139524 |
| P17 | inc3 | inc-naive | del | 1.801086 | 0.405819 | 0.078726 | 1.233030 | 0.083049 |
| P17 | inc3 | inc-naive | ins | 0.992003 | 0.151355 | 0.073281 | 0.684289 | 0.081486 |
| P17 | inc3 | inc-regional | del | 1.680940 | 0.401726 | 0.086397 | 1.098557 | 0.093845 |
| P17 | inc3 | inc-regional | ins | 1.238463 | 0.152499 | 0.088326 | 0.919906 | 0.076330 |
| P17 | inc5 | full | del | 4.707006 | 0.606191 | 0.033095 | 3.719555 | 0.046602 |
| P17 | inc5 | full | ins | 7.581420 | 0.605908 | 0.033379 | 6.596433 | 0.135580 |
| P17 | inc5 | inc-naive | del | 3.396487 | 0.491362 | 0.098596 | 2.725511 | 0.080516 |
| P17 | inc5 | inc-naive | ins | 1.198853 | 0.172301 | 0.084905 | 0.825960 | 0.113766 |
| P17 | inc5 | inc-regional | del | 3.236863 | 0.491617 | 0.105983 | 2.557469 | 0.081322 |
| P17 | inc5 | inc-regional | ins | 1.621241 | 0.161547 | 0.085110 | 1.279499 | 0.093290 |
| P18 | inc1 | full | del | 12.126816 | 0.817737 | 0.048252 | 10.333644 | 0.266731 |
| P18 | inc1 | full | ins | 16.491037 | 0.853790 | 0.049985 | 14.670329 | 0.376155 |
| P18 | inc1 | inc-naive | del | 1.840092 | 0.428088 | 0.083934 | 1.127468 | 0.200211 |
| P18 | inc1 | inc-naive | ins | 1.456811 | 0.159692 | 0.075761 | 1.070308 | 0.149596 |
| P18 | inc1 | inc-regional | del | 1.834352 | 0.452412 | 0.089383 | 1.076116 | 0.215974 |
| P18 | inc1 | inc-regional | ins | 1.721013 | 0.151629 | 0.080489 | 1.355432 | 0.132172 |
| P18 | inc3 | full | del | 9.034634 | 0.800299 | 0.046652 | 7.469179 | 0.099063 |
| P18 | inc3 | full | ins | 16.690742 | 0.823168 | 0.059486 | 14.947700 | 0.378351 |
| P18 | inc3 | inc-naive | del | 3.237793 | 0.669273 | 0.108342 | 2.258581 | 0.200915 |
| P18 | inc3 | inc-naive | ins | 2.584420 | 0.204462 | 0.096327 | 1.983381 | 0.297755 |
| P18 | inc3 | inc-regional | del | 3.006501 | 0.651394 | 0.103068 | 2.057263 | 0.194047 |
| P18 | inc3 | inc-regional | ins | 3.134224 | 0.205339 | 0.096740 | 2.556192 | 0.273657 |
| P18 | inc5 | full | del | 8.917390 | 0.819477 | 0.045288 | 7.350127 | 0.072057 |
| P18 | inc5 | full | ins | 16.416966 | 0.822856 | 0.052667 | 14.729445 | 0.370997 |
| P18 | inc5 | inc-naive | del | 5.441373 | 0.746992 | 0.130527 | 4.364643 | 0.198603 |
| P18 | inc5 | inc-naive | ins | 3.012999 | 0.218559 | 0.108641 | 2.324084 | 0.358635 |
| P18 | inc5 | inc-regional | del | 5.194088 | 0.763556 | 0.143630 | 4.088885 | 0.197384 |
| P18 | inc5 | inc-regional | ins | 4.195829 | 0.218577 | 0.118205 | 3.546029 | 0.309962 |
| P19 | inc1 | full | del | 25.084960 | 1.081666 | 0.087607 | 22.369073 | 0.479874 |
| P19 | inc1 | full | ins | 26.747928 | 1.040696 | 0.074722 | 24.112643 | 0.868019 |
| P19 | inc1 | inc-naive | del | 3.040572 | 0.483515 | 0.110474 | 2.002189 | 0.443862 |
| P19 | inc1 | inc-naive | ins | 2.368033 | 0.187451 | 0.101833 | 1.757370 | 0.319598 |
| P19 | inc1 | inc-regional | del | 2.982926 | 0.469469 | 0.112188 | 1.952362 | 0.448429 |
| P19 | inc1 | inc-regional | ins | 3.291123 | 0.189397 | 0.092890 | 2.674739 | 0.332432 |
| P19 | inc3 | full | del | 21.094375 | 1.163323 | 0.097949 | 18.576724 | 0.282145 |
| P19 | inc3 | full | ins | 30.296327 | 1.141072 | 0.090543 | 27.320451 | 1.204951 |
| P19 | inc5 | full | del | 14.348168 | 1.080265 | 0.081240 | 11.715649 | 0.159728 |
| P19 | inc5 | full | ins | 26.093538 | 1.060677 | 0.078056 | 23.596879 | 0.899890 |
| P19 | inc5 | inc-naive | del | 15.582194 | 1.074506 | 0.180635 | 13.905124 | 0.421165 |
| P19 | inc5 | inc-naive | ins | 10.376594 | 0.278716 | 0.150943 | 9.125266 | 0.816968 |
| P20 | inc1 | full | del | 5.585789 | 1.118094 | 0.097997 | 3.910625 | 0.020412 |
| P20 | inc1 | full | ins | 8.368099 | 1.048589 | 0.106773 | 6.771083 | 0.021212 |
| P20 | inc1 | inc-naive | del | 1.068889 | 0.349741 | 0.121675 | 0.571444 | 0.024791 |
| P20 | inc1 | inc-naive | ins | 0.965902 | 0.334554 | 0.114869 | 0.501469 | 0.014039 |
| P20 | inc1 | inc-regional | del | 1.164607 | 0.371700 | 0.136257 | 0.633726 | 0.021631 |
| P20 | inc1 | inc-regional | ins | 1.305439 | 0.345396 | 0.149676 | 0.792693 | 0.016140 |
| P20 | inc3 | full | del | 5.370354 | 1.026372 | 0.091449 | 3.793789 | 0.016289 |
| P20 | inc3 | full | ins | 8.355155 | 1.068717 | 0.097942 | 6.725267 | 0.023291 |
| P20 | inc3 | inc-naive | del | 1.601003 | 0.470231 | 0.144485 | 0.965291 | 0.019650 |
| P20 | inc3 | inc-naive | ins | 1.208051 | 0.425510 | 0.134195 | 0.631526 | 0.015423 |
| P20 | inc3 | inc-regional | del | 1.756474 | 0.507441 | 0.174105 | 1.053529 | 0.019741 |
| P20 | inc3 | inc-regional | ins | 1.511399 | 0.414301 | 0.136507 | 0.941519 | 0.017458 |
| P20 | inc5 | full | del | 5.365278 | 1.035238 | 0.087774 | 3.793203 | 0.015858 |
| P20 | inc5 | full | ins | 8.374383 | 1.072545 | 0.099933 | 6.773827 | 0.022181 |
| P20 | inc5 | inc-naive | del | 2.518493 | 0.576030 | 0.161421 | 1.762568 | 0.017235 |
| P20 | inc5 | inc-naive | ins | 1.306495 | 0.448106 | 0.160314 | 0.678996 | 0.017519 |
| P20 | inc5 | inc-regional | del | 2.381500 | 0.554498 | 0.163041 | 1.644041 | 0.018653 |
| P20 | inc5 | inc-regional | ins | 1.669228 | 0.436935 | 0.153734 | 1.057810 | 0.018975 |

# 2026-01-22 Dataset D (det-opt) — no-profile timings + profile breakdown

Runs:
- No-profile (timing source): `--det-opt`
- Profile (ratio/breakdown): `--det-opt --profile-inc-regional`

Data snapshots:
- No-profile JSON: `problog-benchmark/side_channel_inc_strengthen_fresh/noprofile_json_20260122_060201`
- Profile JSON: `problog-benchmark/side_channel_inc_strengthen_fresh/profile_json_20260122_063148`

## Dataset D: No-profile end-to-end times (seconds)
| Case | Delta | Full | Inc-naive | Inc-regional | OK |
|---|---|---:|---:|---:|:--:|
| P1 | inc1 | 0.093 | 0.052 | 0.052 | Y |
| P1 | inc3 | 0.097 | 0.053 | 0.053 | Y |
| P1 | inc5 | 0.095 | 0.051 | 0.055 | Y |
| P3 | inc1 | 0.107 | 0.050 | 0.048 | Y |
| P3 | inc3 | 0.119 | 0.048 | 0.048 | Y |
| P3 | inc5 | 0.107 | 0.049 | 0.048 | Y |
| P4 | inc1 | 0.014 | 0.016 | 0.017 | Y |
| P4 | inc3 | 0.015 | 0.031 | 0.017 | Y |
| P4 | inc5 | 0.014 | 0.016 | 0.017 | Y |
| P5 | inc1 | 0.017 | 0.022 | 0.021 | Y |
| P5 | inc3 | 0.018 | 0.022 | 0.033 | Y |
| P5 | inc5 | 0.018 | 0.026 | 0.038 | Y |
| P6 | inc1 | 0.027 | 0.040 | 0.040 | Y |
| P6 | inc3 | 0.026 | 0.042 | 0.038 | Y |
| P6 | inc5 | 0.051 | 0.039 | 0.039 | Y |
| P7 | inc1 | 0.032 | 0.045 | 0.045 | Y |
| P7 | inc3 | 0.035 | 0.063 | 0.045 | Y |
| P7 | inc5 | 0.053 | 0.049 | 0.047 | Y |
| P8 | inc1 | 0.057 | 0.071 | 0.057 | Y |
| P8 | inc3 | 0.046 | 0.058 | 0.059 | Y |
| P8 | inc5 | 0.047 | 0.065 | 0.073 | Y |
| P9 | inc1 | 0.266 | 0.132 | 0.132 | Y |
| P9 | inc3 | 0.325 | 0.146 | 0.143 | Y |
| P9 | inc5 | 0.258 | 0.138 | 0.129 | Y |
| P10 | inc1 | 0.163 | 0.103 | 0.107 | Y |
| P10 | inc3 | 0.163 | 0.115 | 0.113 | Y |
| P10 | inc5 | 0.171 | 0.133 | 0.144 | Y |
| P11 | inc1 | 0.157 | 0.094 | 0.087 | Y |
| P11 | inc3 | 0.153 | 0.110 | 0.109 | Y |
| P11 | inc5 | 0.173 | 0.126 | 0.131 | Y |
| P12 | inc1 | 1.426 | 0.582 | 0.569 | Y |
| P12 | inc3 | 1.487 | 0.623 | 0.616 | Y |
| P12 | inc5 | 1.521 | 0.657 | 0.671 | Y |
| P13 | inc1 | 4.240 | 1.652 | 1.657 | Y |
| P13 | inc3 | 4.191 | 1.732 | 1.763 | Y |
| P13 | inc5 | 3.882 | 1.850 | 1.856 | Y |
| P14 | inc1 | 9.477 | 3.895 | 3.772 | Y |
| P14 | inc3 | 9.601 | 4.123 | 4.126 | Y |
| P14 | inc5 | 9.761 | 4.331 | 4.466 | Y |
| P15 | inc1 | 21.220 | 8.834 | 8.641 | Y |
| P15 | inc3 | 21.040 | 9.900 | 10.048 | Y |
| P15 | inc5 | 21.016 | 10.824 | 10.804 | Y |
| P16 | inc1 | 34.283 | 15.169 | 15.413 | Y |
| P16 | inc3 | 33.917 | 17.619 | 17.868 | Y |
| P16 | inc5 | 33.957 | 20.826 | 21.126 | Y |
| P17 | inc1 | 23.803 | 9.836 | 10.064 | Y |
| P17 | inc3 | 22.028 | 11.375 | 11.583 | Y |
| P17 | inc5 | 20.959 | 13.237 | 13.423 | Y |
| P18 | inc1 | 46.767 | 20.958 | 21.166 | Y |
| P18 | inc3 | 43.578 | 23.583 | 24.097 | Y |
| P18 | inc5 | 43.830 | 26.460 | 26.718 | Y |
| P19 | inc1 | 81.153 | 33.945 | 34.672 | Y |
| P19 | inc3 | 73.524 | 40.518 | 39.641 | N |
| P19 | inc5 | 68.678 | 52.364 | 51.746 | N |
| P20 | inc1 | 23.602 | 11.922 | 12.306 | N |
| P20 | inc3 | 23.590 | 13.060 | 12.930 | N |
| P20 | inc5 | 23.676 | 14.244 | 14.610 | N |

## Insert turn (turn=3) time + FC time (no-profile, seconds)
| Case | Delta | Full T3 | Naive T3 | Regional T3 | Full FC3 | Naive FC3 | Regional FC3 |
|---|---|---:|---:|---:|---:|---:|---:|
| P1 | inc1 | 0.035 | 0.006 | 0.006 | 0.006 | 0.006 | 0.006 |
| P1 | inc3 | 0.040 | 0.006 | 0.006 | 0.006 | 0.006 | 0.006 |
| P1 | inc5 | 0.038 | 0.006 | 0.006 | 0.006 | 0.006 | 0.006 |
| P3 | inc1 | 0.040 | 0.005 | 0.005 | 0.005 | 0.005 | 0.005 |
| P3 | inc3 | 0.041 | 0.005 | 0.005 | 0.005 | 0.005 | 0.005 |
| P3 | inc5 | 0.040 | 0.005 | 0.005 | 0.005 | 0.005 | 0.005 |
| P4 | inc1 | 0.003 | 0.003 | 0.003 | 0.002 | 0.002 | 0.002 |
| P4 | inc3 | 0.003 | 0.003 | 0.003 | 0.002 | 0.002 | 0.002 |
| P4 | inc5 | 0.003 | 0.003 | 0.003 | 0.002 | 0.002 | 0.002 |
| P5 | inc1 | 0.004 | 0.004 | 0.004 | 0.002 | 0.003 | 0.003 |
| P5 | inc3 | 0.004 | 0.004 | 0.004 | 0.003 | 0.003 | 0.003 |
| P5 | inc5 | 0.004 | 0.004 | 0.004 | 0.003 | 0.003 | 0.003 |
| P6 | inc1 | 0.006 | 0.006 | 0.006 | 0.004 | 0.004 | 0.004 |
| P6 | inc3 | 0.006 | 0.006 | 0.006 | 0.004 | 0.004 | 0.004 |
| P6 | inc5 | 0.006 | 0.006 | 0.006 | 0.004 | 0.004 | 0.004 |
| P7 | inc1 | 0.010 | 0.010 | 0.010 | 0.008 | 0.008 | 0.008 |
| P7 | inc3 | 0.010 | 0.010 | 0.010 | 0.008 | 0.008 | 0.008 |
| P7 | inc5 | 0.010 | 0.010 | 0.010 | 0.008 | 0.008 | 0.008 |
| P8 | inc1 | 0.016 | 0.016 | 0.016 | 0.013 | 0.013 | 0.013 |
| P8 | inc3 | 0.016 | 0.016 | 0.016 | 0.013 | 0.013 | 0.013 |
| P8 | inc5 | 0.016 | 0.016 | 0.016 | 0.013 | 0.013 | 0.013 |
| P9 | inc1 | 0.074 | 0.018 | 0.018 | 0.013 | 0.013 | 0.013 |
| P9 | inc3 | 0.074 | 0.018 | 0.018 | 0.013 | 0.013 | 0.013 |
| P9 | inc5 | 0.075 | 0.018 | 0.018 | 0.013 | 0.013 | 0.013 |
| P10 | inc1 | 0.068 | 0.020 | 0.020 | 0.014 | 0.014 | 0.014 |
| P10 | inc3 | 0.068 | 0.020 | 0.020 | 0.014 | 0.014 | 0.014 |
| P10 | inc5 | 0.068 | 0.020 | 0.020 | 0.014 | 0.014 | 0.014 |
| P11 | inc1 | 0.067 | 0.019 | 0.019 | 0.013 | 0.013 | 0.013 |
| P11 | inc3 | 0.067 | 0.019 | 0.019 | 0.013 | 0.013 | 0.013 |
| P11 | inc5 | 0.067 | 0.019 | 0.019 | 0.013 | 0.013 | 0.013 |
| P12 | inc1 | 0.582 | 0.098 | 0.099 | 0.085 | 0.085 | 0.085 |
| P12 | inc3 | 0.582 | 0.098 | 0.099 | 0.085 | 0.085 | 0.085 |
| P12 | inc5 | 0.582 | 0.098 | 0.099 | 0.085 | 0.085 | 0.085 |
| P13 | inc1 | 1.751 | 0.240 | 0.240 | 0.212 | 0.212 | 0.212 |
| P13 | inc3 | 1.751 | 0.240 | 0.240 | 0.212 | 0.212 | 0.212 |
| P13 | inc5 | 1.751 | 0.240 | 0.240 | 0.212 | 0.212 | 0.212 |
| P14 | inc1 | 4.493 | 0.508 | 0.512 | 0.483 | 0.483 | 0.483 |
| P14 | inc3 | 4.493 | 0.508 | 0.512 | 0.483 | 0.483 | 0.483 |
| P14 | inc5 | 4.493 | 0.508 | 0.512 | 0.483 | 0.483 | 0.483 |
| P15 | inc1 | 10.483 | 1.040 | 1.042 | 1.014 | 1.014 | 1.014 |
| P15 | inc3 | 10.483 | 1.040 | 1.042 | 1.014 | 1.014 | 1.014 |
| P15 | inc5 | 10.483 | 1.040 | 1.042 | 1.014 | 1.014 | 1.014 |
| P16 | inc1 | 16.961 | 1.697 | 1.697 | 1.663 | 1.663 | 1.663 |
| P16 | inc3 | 16.961 | 1.697 | 1.697 | 1.663 | 1.663 | 1.663 |
| P16 | inc5 | 16.961 | 1.697 | 1.697 | 1.663 | 1.663 | 1.663 |
| P17 | inc1 | 11.754 | 1.143 | 1.143 | 1.121 | 1.121 | 1.121 |
| P17 | inc3 | 11.754 | 1.143 | 1.143 | 1.121 | 1.121 | 1.121 |
| P17 | inc5 | 11.754 | 1.143 | 1.143 | 1.121 | 1.121 | 1.121 |
| P18 | inc1 | 20.729 | 1.788 | 1.792 | 1.756 | 1.756 | 1.756 |
| P18 | inc3 | 20.729 | 1.788 | 1.792 | 1.756 | 1.756 | 1.756 |
| P18 | inc5 | 20.729 | 1.788 | 1.792 | 1.756 | 1.756 | 1.756 |
| P19 | inc1 | 38.345 | 2.323 | 2.329 | 2.300 | 2.300 | 2.300 |
| P19 | inc3 | 38.345 | 2.323 | 2.329 | 2.300 | 2.300 | 2.300 |
| P19 | inc5 | 38.345 | 2.323 | 2.329 | 2.300 | 2.300 | 2.300 |
| P20 | inc1 | 7.737 | 1.049 | 1.053 | 1.022 | 1.022 | 1.022 |
| P20 | inc3 | 7.737 | 1.049 | 1.053 | 1.022 | 1.022 | 1.022 |
| P20 | inc5 | 7.737 | 1.049 | 1.053 | 1.022 | 1.022 | 1.022 |

## Region/DR ratios (profile run)
| Case | Delta | Analyze R/DR | Final R/DR | Expand summary |
|---|---|---:|---:|---|
| P1 | inc1 | 85/85 (1.000) | 85/85 (1.000) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P1 | inc3 | 85/85 (1.000) | 85/85 (1.000) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P1 | inc5 | 85/85 (1.000) | 85/85 (1.000) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P3 | inc1 | 60/71 (0.845) | 60/71 (0.845) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P3 | inc3 | 60/71 (0.845) | 60/71 (0.845) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P3 | inc5 | 60/71 (0.845) | 60/71 (0.845) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P4 | inc1 | 13/13 (1.000) | 13/13 (1.000) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P4 | inc3 | 13/13 (1.000) | 13/13 (1.000) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P4 | inc5 | 13/13 (1.000) | 13/13 (1.000) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P5 | inc1 | 12/23 (0.522) | 12/23 (0.522) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P5 | inc3 | 12/23 (0.522) | 12/23 (0.522) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P5 | inc5 | 12/23 (0.522) | 12/23 (0.522) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P6 | inc1 | 29/52 (0.558) | 29/52 (0.558) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P6 | inc3 | 29/52 (0.558) | 29/52 (0.558) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P6 | inc5 | 29/52 (0.558) | 29/52 (0.558) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P7 | inc1 | 33/53 (0.623) | 33/53 (0.623) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P7 | inc3 | 33/53 (0.623) | 33/53 (0.623) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P7 | inc5 | 33/53 (0.623) | 33/53 (0.623) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P8 | inc1 | 58/72 (0.806) | 58/72 (0.806) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P8 | inc3 | 58/72 (0.806) | 58/72 (0.806) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P8 | inc5 | 58/72 (0.806) | 58/72 (0.806) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P9 | inc1 | 133/212 (0.627) | 133/212 (0.627) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P9 | inc3 | 133/212 (0.627) | 133/212 (0.627) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P9 | inc5 | 133/212 (0.627) | 133/212 (0.627) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P10 | inc1 | 134/276 (0.486) | 134/276 (0.486) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P10 | inc3 | 134/276 (0.486) | 134/276 (0.486) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P10 | inc5 | 134/276 (0.486) | 134/276 (0.486) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P11 | inc1 | 229/313 (0.732) | 229/313 (0.732) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P11 | inc3 | 229/313 (0.732) | 229/313 (0.732) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P11 | inc5 | 229/313 (0.732) | 229/313 (0.732) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P12 | inc1 | 398/570 (0.698) | 398/570 (0.698) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P12 | inc3 | 398/570 (0.698) | 398/570 (0.698) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P12 | inc5 | 398/570 (0.698) | 398/570 (0.698) | close_inputs:+0n/+0e x1; overlap_closure:+0n/+0e x1; overlap_forward:+0n/+0e x1 |
| P13 | inc1 | 505/931 (0.542) | 886/931 (0.952) | close_inputs:+381n/+381e x1; overlap_closure:+381n/+0e x1; overlap_forward:+381n/+381e x1 |
| P13 | inc3 | 505/931 (0.542) | 886/931 (0.952) | close_inputs:+381n/+381e x1; overlap_closure:+381n/+0e x1; overlap_forward:+381n/+381e x1 |
| P13 | inc5 | 505/931 (0.542) | 886/931 (0.952) | close_inputs:+381n/+381e x1; overlap_closure:+381n/+0e x1; overlap_forward:+381n/+381e x1 |
| P14 | inc1 | 462/1101 (0.420) | 944/1101 (0.857) | close_inputs:+482n/+482e x1; overlap_closure:+482n/+0e x1; overlap_forward:+482n/+482e x1 |
| P14 | inc3 | 462/1101 (0.420) | 944/1101 (0.857) | close_inputs:+482n/+482e x1; overlap_closure:+482n/+0e x1; overlap_forward:+482n/+482e x1 |
| P14 | inc5 | 462/1101 (0.420) | 944/1101 (0.857) | close_inputs:+482n/+482e x1; overlap_closure:+482n/+0e x1; overlap_forward:+482n/+482e x1 |
| P15 | inc1 | 344/1149 (0.299) | 759/1149 (0.661) | close_inputs:+415n/+415e x1; overlap_closure:+415n/+0e x1; overlap_forward:+415n/+415e x1 |
| P15 | inc3 | 1400/3358 (0.417) | 3327/3358 (0.991) | close_inputs:+1927n/+1927e x1; overlap_closure:+1927n/+0e x1; overlap_forward:+1927n/+1927e x1; expand_missing_anchor:+0n/+0e x1 |
| P15 | inc5 | 2049/4574 (0.448) | 4516/4574 (0.987) | close_inputs:+2467n/+2467e x1; overlap_closure:+2467n/+0e x1; overlap_forward:+2467n/+2467e x1; expand_missing_anchor:+0n/+0e x1 |
| P16 | inc1 | 879/2750 (0.320) | 2739/2750 (0.996) | close_inputs:+1860n/+1860e x1; overlap_closure:+1860n/+0e x1; overlap_forward:+1860n/+1860e x1; expand_missing_anchor:+0n/+0e x1 |
| P16 | inc3 | 2708/5652 (0.479) | 5596/5652 (0.990) | close_inputs:+2888n/+2888e x1; overlap_closure:+2888n/+0e x1; overlap_forward:+2888n/+2888e x1; expand_missing_anchor:+0n/+0e x1 |
| P16 | inc5 | 4777/9150 (0.522) | 9067/9150 (0.991) | close_inputs:+4290n/+4290e x1; overlap_closure:+4290n/+0e x1; overlap_forward:+4290n/+4290e x1; expand_missing_anchor:+0n/+0e x1 |
| P17 | inc1 | 1941/3812 (0.509) | 3802/3812 (0.997) | close_inputs:+1861n/+1861e x1; overlap_closure:+1861n/+0e x1; overlap_forward:+1861n/+1861e x1; expand_missing_anchor:+0n/+0e x1 |
| P17 | inc3 | 3517/6440 (0.546) | 6390/6440 (0.992) | close_inputs:+2873n/+2873e x1; overlap_closure:+2873n/+0e x1; overlap_forward:+2873n/+2873e x1; expand_missing_anchor:+0n/+0e x1 |
| P17 | inc5 | 5335/9007 (0.592) | 8958/9007 (0.995) | close_inputs:+3623n/+3623e x1; overlap_closure:+3623n/+0e x1; overlap_forward:+3623n/+3623e x1; expand_missing_anchor:+0n/+0e x1 |
| P18 | inc1 | 5630/9052 (0.622) | 8968/9052 (0.991) | close_inputs:+3338n/+3338e x1; overlap_closure:+3338n/+0e x1; overlap_forward:+3338n/+3338e x1; expand_missing_anchor:+0n/+0e x1 |
| P18 | inc3 | 7536/12771 (0.590) | 12664/12771 (0.992) | close_inputs:+5128n/+5128e x1; overlap_closure:+5128n/+0e x1; overlap_forward:+5128n/+5128e x1; expand_missing_anchor:+0n/+0e x1 |
| P18 | inc5 | 9390/16569 (0.567) | 16473/16569 (0.994) | close_inputs:+7083n/+7083e x1; overlap_closure:+7083n/+0e x1; overlap_forward:+7083n/+7083e x1; expand_missing_anchor:+0n/+0e x1 |
| P19 | inc1 | 7455/14759 (0.505) | 14747/14759 (0.999) | close_inputs:+7292n/+7292e x1; overlap_closure:+7292n/+0e x1; overlap_forward:+7292n/+7292e x1; expand_missing_anchor:+0n/+0e x1 |
| P19 | inc3 | 10446/17983 (0.581) | 17952/17983 (0.998) | close_inputs:+7506n/+7506e x1; overlap_closure:+7506n/+0e x1; overlap_forward:+7506n/+7506e x1; expand_missing_anchor:+0n/+0e x1 |
| P19 | inc5 | 11491/20052 (0.573) | 20023/20052 (0.999) | close_inputs:+8532n/+8532e x1; overlap_closure:+8532n/+0e x1; overlap_forward:+8532n/+8532e x1; expand_missing_anchor:+0n/+0e x1 |
| P20 | inc1 | 2349/4195 (0.560) | 4195/4195 (1.000) | close_inputs:+1846n/+1846e x1; overlap_closure:+1846n/+0e x1; overlap_forward:+1846n/+1846e x1; expand_missing_anchor:+0n/+0e x1 |
| P20 | inc3 | 2970/5065 (0.587) | 5065/5065 (1.000) | close_inputs:+2095n/+2095e x1; overlap_closure:+2095n/+0e x1; overlap_forward:+2095n/+2095e x1; expand_missing_anchor:+0n/+0e x1 |
| P20 | inc5 | 3800/6470 (0.587) | 6470/6470 (1.000) | close_inputs:+2670n/+2670e x1; overlap_closure:+2670n/+0e x1; overlap_forward:+2670n/+2670e x1; expand_missing_anchor:+0n/+0e x1 |

## Inc-regional insert FC timing breakdown (profile run, ms)
| Case | Delta | analyze | sccClose | plan | rebuild | calibrate | total |
|---|---|---:|---:|---:|---:|---:|---:|
| P1 | inc1 | 9.082 | 0.369 | 0.098 | 0.342 | 0.004 | 9.894 |
| P1 | inc3 | 9.128 | 0.325 | 0.103 | 0.349 | 0.004 | 9.913 |
| P1 | inc5 | 9.022 | 0.320 | 0.110 | 0.333 | 0.004 | 9.793 |
| P3 | inc1 | 7.527 | 0.266 | 0.091 | 0.281 | 0.004 | 8.169 |
| P3 | inc3 | 7.636 | 0.270 | 0.091 | 0.287 | 0.004 | 8.306 |
| P3 | inc5 | 7.417 | 0.261 | 0.088 | 0.285 | 0.004 | 8.071 |
| P4 | inc1 | 2.341 | 0.108 | 0.053 | 0.080 | 0.003 | 2.652 |
| P4 | inc3 | 2.367 | 0.109 | 0.053 | 0.079 | 0.003 | 2.676 |
| P4 | inc5 | 2.306 | 0.104 | 0.051 | 0.077 | 0.003 | 2.614 |
| P5 | inc1 | 2.864 | 0.095 | 0.061 | 0.168 | 0.004 | 3.191 |
| P5 | inc3 | 2.868 | 0.095 | 0.061 | 0.168 | 0.004 | 3.195 |
| P5 | inc5 | 2.864 | 0.095 | 0.061 | 0.168 | 0.004 | 3.193 |
| P6 | inc1 | 4.240 | 0.141 | 0.079 | 0.209 | 0.004 | 4.676 |
| P6 | inc3 | 4.192 | 0.140 | 0.079 | 0.211 | 0.004 | 4.642 |
| P6 | inc5 | 4.189 | 0.140 | 0.079 | 0.210 | 0.004 | 4.639 |
| P7 | inc1 | 5.937 | 0.192 | 0.092 | 0.287 | 0.004 | 6.512 |
| P7 | inc3 | 5.947 | 0.191 | 0.092 | 0.286 | 0.004 | 6.520 |
| P7 | inc5 | 5.916 | 0.192 | 0.092 | 0.286 | 0.004 | 6.491 |
| P8 | inc1 | 7.837 | 0.263 | 0.101 | 0.343 | 0.004 | 8.548 |
| P8 | inc3 | 7.748 | 0.262 | 0.102 | 0.345 | 0.004 | 8.461 |
| P8 | inc5 | 7.788 | 0.262 | 0.102 | 0.345 | 0.004 | 8.502 |
| P9 | inc1 | 19.122 | 0.483 | 0.169 | 0.796 | 0.006 | 20.576 |
| P9 | inc3 | 18.832 | 0.474 | 0.169 | 0.799 | 0.006 | 20.279 |
| P9 | inc5 | 18.758 | 0.471 | 0.169 | 0.796 | 0.006 | 20.200 |
| P10 | inc1 | 22.081 | 0.544 | 0.188 | 0.858 | 0.006 | 23.678 |
| P10 | inc3 | 22.237 | 0.548 | 0.189 | 0.859 | 0.006 | 23.839 |
| P10 | inc5 | 22.187 | 0.546 | 0.190 | 0.858 | 0.006 | 23.789 |
| P11 | inc1 | 21.421 | 0.465 | 0.180 | 0.793 | 0.006 | 22.865 |
| P11 | inc3 | 21.737 | 0.467 | 0.180 | 0.794 | 0.006 | 23.184 |
| P11 | inc5 | 21.717 | 0.466 | 0.180 | 0.794 | 0.006 | 23.162 |
| P12 | inc1 | 76.621 | 0.994 | 0.948 | 4.832 | 0.031 | 83.676 |
| P12 | inc3 | 77.126 | 0.999 | 0.951 | 4.842 | 0.031 | 84.197 |
| P12 | inc5 | 77.093 | 1.000 | 0.950 | 4.842 | 0.031 | 84.174 |
| P13 | inc1 | 181.902 | 1.367 | 2.450 | 13.706 | 0.226 | 199.651 |
| P13 | inc3 | 184.609 | 1.382 | 2.474 | 13.740 | 0.229 | 202.434 |
| P13 | inc5 | 185.931 | 1.386 | 2.503 | 13.792 | 0.229 | 203.841 |
| P14 | inc1 | 367.380 | 1.850 | 4.435 | 29.163 | 0.638 | 403.466 |
| P14 | inc3 | 372.137 | 1.852 | 4.458 | 29.227 | 0.639 | 408.314 |
| P14 | inc5 | 372.236 | 1.852 | 4.464 | 29.242 | 0.639 | 408.433 |
| P15 | inc1 | 91.798 | 0.360 | 16.090 | 77.415 | 12.007 | 197.670 |
| P15 | inc3 | 231.553 | 0.591 | 9.251 | 127.231 | 0.355 | 368.994 |
| P15 | inc5 | 324.229 | 0.678 | 10.452 | 155.659 | 0.356 | 491.374 |
| P16 | inc1 | 129.433 | 0.580 | 23.243 | 184.510 | 0.606 | 338.371 |
| P16 | inc3 | 372.117 | 0.811 | 15.598 | 225.252 | 0.601 | 614.381 |
| P16 | inc5 | 624.659 | 0.975 | 19.080 | 282.726 | 0.611 | 928.081 |
| P17 | inc1 | 217.677 | 0.662 | 24.991 | 131.799 | 0.465 | 375.593 |
| P17 | inc3 | 431.289 | 0.891 | 23.371 | 178.912 | 0.472 | 634.935 |
| P17 | inc5 | 641.431 | 1.050 | 30.408 | 239.342 | 0.480 | 912.711 |
| P18 | inc1 | 424.993 | 0.852 | 29.578 | 222.296 | 0.474 | 678.237 |
| P18 | inc3 | 715.920 | 1.045 | 33.663 | 277.223 | 0.481 | 1028.332 |
| P18 | inc5 | 1073.058 | 1.160 | 41.801 | 360.836 | 0.491 | 1477.346 |
| P19 | inc1 | 1364.921 | 1.496 | 51.107 | 538.014 | 0.930 | 1956.468 |
| P19 | inc3 | 1405.368 | 1.525 | 50.899 | 598.564 | 0.928 | 2057.284 |
| P19 | inc5 | 1567.714 | 1.635 | 56.729 | 668.969 | 0.940 | 2295.987 |
| P20 | inc1 | 457.518 | 0.981 | 16.695 | 146.917 | 0.381 | 622.835 |
| P20 | inc3 | 531.100 | 1.020 | 17.427 | 162.757 | 0.383 | 712.687 |
| P20 | inc5 | 651.193 | 1.116 | 20.611 | 190.033 | 0.388 | 863.341 |

## Inc-regional rebuild subcomponents (profile run, ms)
| Case | Delta | snapshot | initNodes | initEdges | depGraph | regionCycles | indegree | rebuildLoop | total |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P1 | inc1 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P1 | inc3 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.257 | 0.321 |
| P1 | inc5 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P3 | inc1 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P3 | inc3 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P3 | inc5 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P4 | inc1 | 0.000 | 0.031 | 0.016 | 0.000 | 0.008 | 0.007 | 0.256 | 0.318 |
| P4 | inc3 | 0.000 | 0.031 | 0.016 | 0.000 | 0.008 | 0.007 | 0.256 | 0.318 |
| P4 | inc5 | 0.000 | 0.031 | 0.016 | 0.000 | 0.008 | 0.007 | 0.256 | 0.318 |
| P5 | inc1 | 0.000 | 0.031 | 0.016 | 0.000 | 0.008 | 0.007 | 0.256 | 0.318 |
| P5 | inc3 | 0.000 | 0.031 | 0.016 | 0.000 | 0.008 | 0.007 | 0.256 | 0.318 |
| P5 | inc5 | 0.000 | 0.031 | 0.016 | 0.000 | 0.008 | 0.007 | 0.256 | 0.318 |
| P6 | inc1 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P6 | inc3 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P6 | inc5 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P7 | inc1 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P7 | inc3 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P7 | inc5 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P8 | inc1 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P8 | inc3 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P8 | inc5 | 0.000 | 0.032 | 0.017 | 0.000 | 0.008 | 0.007 | 0.256 | 0.320 |
| P9 | inc1 | 0.000 | 0.040 | 0.034 | 0.000 | 0.011 | 0.009 | 0.673 | 0.767 |
| P9 | inc3 | 0.000 | 0.040 | 0.034 | 0.000 | 0.011 | 0.009 | 0.675 | 0.769 |
| P9 | inc5 | 0.000 | 0.040 | 0.034 | 0.000 | 0.011 | 0.009 | 0.674 | 0.769 |
| P10 | inc1 | 0.000 | 0.040 | 0.034 | 0.000 | 0.011 | 0.009 | 0.675 | 0.769 |
| P10 | inc3 | 0.000 | 0.040 | 0.034 | 0.000 | 0.011 | 0.009 | 0.675 | 0.769 |
| P10 | inc5 | 0.000 | 0.040 | 0.034 | 0.000 | 0.011 | 0.009 | 0.675 | 0.769 |
| P11 | inc1 | 0.000 | 0.040 | 0.034 | 0.000 | 0.011 | 0.009 | 0.675 | 0.769 |
| P11 | inc3 | 0.000 | 0.040 | 0.034 | 0.000 | 0.011 | 0.009 | 0.675 | 0.769 |
| P11 | inc5 | 0.000 | 0.040 | 0.034 | 0.000 | 0.011 | 0.009 | 0.675 | 0.769 |
| P12 | inc1 | 0.000 | 0.100 | 0.080 | 0.000 | 0.015 | 0.012 | 4.618 | 4.825 |
| P12 | inc3 | 0.000 | 0.100 | 0.080 | 0.000 | 0.015 | 0.012 | 4.621 | 4.828 |
| P12 | inc5 | 0.000 | 0.100 | 0.080 | 0.000 | 0.015 | 0.012 | 4.622 | 4.829 |
| P13 | inc1 | 0.000 | 0.160 | 0.140 | 0.000 | 0.019 | 0.015 | 13.278 | 13.671 |
| P13 | inc3 | 0.000 | 0.160 | 0.140 | 0.000 | 0.019 | 0.015 | 13.311 | 13.704 |
| P13 | inc5 | 0.000 | 0.160 | 0.140 | 0.000 | 0.019 | 0.015 | 13.311 | 13.704 |
| P14 | inc1 | 0.000 | 0.230 | 0.210 | 0.000 | 0.022 | 0.018 | 28.465 | 29.165 |
| P14 | inc3 | 0.000 | 0.230 | 0.210 | 0.000 | 0.022 | 0.018 | 28.548 | 29.249 |
| P14 | inc5 | 0.000 | 0.230 | 0.210 | 0.000 | 0.022 | 0.018 | 28.540 | 29.241 |
| P15 | inc1 | 0.000 | 0.290 | 0.250 | 0.000 | 0.026 | 0.022 | 76.785 | 77.377 |
| P15 | inc3 | 0.000 | 0.290 | 0.250 | 0.000 | 0.026 | 0.022 | 126.563 | 127.203 |
| P15 | inc5 | 0.000 | 0.290 | 0.250 | 0.000 | 0.026 | 0.022 | 155.078 | 155.718 |
| P16 | inc1 | 0.000 | 0.430 | 0.360 | 0.000 | 0.031 | 0.026 | 183.683 | 184.532 |
| P16 | inc3 | 0.000 | 0.430 | 0.360 | 0.000 | 0.031 | 0.026 | 224.310 | 225.157 |
| P16 | inc5 | 0.000 | 0.430 | 0.360 | 0.000 | 0.031 | 0.026 | 281.792 | 282.639 |
| P17 | inc1 | 0.000 | 0.380 | 0.330 | 0.000 | 0.030 | 0.025 | 131.064 | 131.801 |
| P17 | inc3 | 0.000 | 0.380 | 0.330 | 0.000 | 0.030 | 0.025 | 178.159 | 178.896 |
| P17 | inc5 | 0.000 | 0.380 | 0.330 | 0.000 | 0.030 | 0.025 | 238.588 | 239.326 |
| P18 | inc1 | 0.000 | 0.490 | 0.420 | 0.000 | 0.035 | 0.029 | 221.208 | 222.182 |
| P18 | inc3 | 0.000 | 0.490 | 0.420 | 0.000 | 0.035 | 0.029 | 276.070 | 277.044 |
| P18 | inc5 | 0.000 | 0.490 | 0.420 | 0.000 | 0.035 | 0.029 | 359.678 | 360.652 |
| P19 | inc1 | 0.000 | 0.600 | 0.520 | 0.000 | 0.044 | 0.036 | 536.767 | 538.010 |
| P19 | inc3 | 0.000 | 0.600 | 0.520 | 0.000 | 0.044 | 0.036 | 597.317 | 598.559 |
| P19 | inc5 | 0.000 | 0.600 | 0.520 | 0.000 | 0.044 | 0.036 | 667.727 | 668.969 |
| P20 | inc1 | 0.000 | 0.350 | 0.310 | 0.000 | 0.029 | 0.023 | 145.785 | 146.902 |
| P20 | inc3 | 0.000 | 0.350 | 0.310 | 0.000 | 0.029 | 0.023 | 161.625 | 162.742 |
| P20 | inc5 | 0.000 | 0.350 | 0.310 | 0.000 | 0.029 | 0.023 | 188.901 | 190.018 |

## Mismatches (no-profile)
| Case | Delta | Comment |
|---|---|---|
| P19 | inc3 | mismatch |
| P19 | inc5 | mismatch |
| P20 | inc1 | mismatch |
| P20 | inc3 | mismatch |
| P20 | inc5 | mismatch |

## 2026-01-22: Regional insert aligned with naive (P1-P18, no-profile)

### 1) 最近实验的设置
- 数据集：`problog-benchmark/side_channel_inc_strengthen_fresh`
- 编译（P1-P18）：`PATH=/home/hugh/research/datalog/souffle/build/src:$PATH python3 problog-benchmark/side_channel_inc.py --base-dir problog-benchmark/side_channel_inc_strengthen_fresh compile --cases 1-18 --timeout 900 --jobs 8`
  - P2 缺少 `compute.souffle.dl`，被自动跳过（无 delta）
- 运行（无 profile）：`python3 problog-benchmark/side_channel_inc.py --base-dir problog-benchmark/side_channel_inc_strengthen_fresh run --cases 1-18 --compare-all --timeout 1200 --run-arg=--det-opt`
- 代码调整（本次实验基线）：
  - inc-regional insert 改为 **delta 驱动传播**（worklist 仅由 deltaInsertedEdges 种子触发，范围限于 region）
  - inc-regional insert 增加 **preConfig**，与 inc-naive insert 对齐
  - 关闭 inc-regional DAG fast-path（保持与 inc-naive cyclewise 行为一致）

### 2) 实验的效果
- **正确性**：P1-P18 全部 delta（inc1/inc3/inc5）均 **OK 无 mismatch**；P2 无 delta 文件跳过。
- **性能（端到端、无 profile）**：inc-regional 与 inc-naive 基本持平。
  - 例：P15
    - inc1：inc-naive=9.784s，inc-regional=10.345s
    - inc3：inc-naive=11.528s，inc-regional=11.457s
    - inc5：inc-naive=12.896s，inc-regional=12.683s

### 3) 观察与结论
- 当 region≈delta-reachable（如 P15 inc3/5 的 ratio ≈ 0.99），inc-regional 的 insert 计算量与 inc-naive 基本等价；
  将 inc-regional 的 insert 改为 delta 驱动传播后，两者耗时已接近。
- 在 region 明显小的 case（如 P15 inc1，ratio≈0.66），inc-regional 有更小的 BDD 更新规模，但整体耗时仍受 analyze/plan/calibrate 影响。
- 当前版本至少保证 **正确性与性能不劣于 inc-naive**（P1-P18 范围）。

### 4) 下一步计划
- 对 P1-P18 重新收集 **profile breakdown**（analyze/plan/rebuild/calibrate）以定位 residual overhead。
- 若稳定无 mismatch，再扩展到 P19-P20（确认旧 mismatch 是否仍存在）。
- 在保证正确性的前提下，继续降低 analyze/plan 的固定开销（避免 region≈dr 时的“无收益分析”）。
