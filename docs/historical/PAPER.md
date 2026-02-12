# PAPER

This document collects the data used to generate paper tables/figures.

## Scope
- Repository: `/home/hugh/research/datalog/souffle`
- Base outputs: `/home/hugh/research/datalog/souffle/archive/2026-02-11/side_channel/side_channel_inc_p12_20_mix_seed42`
- Canonical batch note: `archive/2026-02-11/README.md`
- Cases: P12–P20
- Delta ratios: mix-d0-i100, mix-d25-i75, mix-d50-i50, mix-d75-i25, mix-d100-i0

## Table: P12–P20 Average Speedup (FULL ÷ INC) by Delta Ratio

Averaging pipeline:
1. For each delta sample (5 runs), compute the mean stage time per mode (FULL / INC).
2. Compute per-delta speedup = FULL_mean / INC_mean.
3. Average speedup across the 5 delta samples (same ratio).
4. Average across cases P12–P20.

### inc-naive speedup

| Delta ratio | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|
| 0% del / 100% ins | 4.49x | 45.75x | 8.90x |
| 25% del / 75% ins | 3.09x | 17.78x | 12.27x |
| 50% del / 50% ins | 2.96x | 17.99x | 11.67x |
| 75% del / 25% ins | 2.84x | 20.18x | 12.13x |
| 100% del / 0% ins | 3.90x | 27.50x | 2.90x |

### inc-regional speedup

| Delta ratio | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|
| 0% del / 100% ins | 4.49x | 69.43x | 6.40x |
| 25% del / 75% ins | 3.09x | 22.70x | 9.78x |
| 50% del / 50% ins | 2.98x | 21.75x | 10.23x |
| 75% del / 25% ins | 2.84x | 21.99x | 10.11x |
| 100% del / 0% ins | 3.90x | 27.90x | 2.96x |

## Notes
- Stage timings are taken from the second (delta) turn in each per-run JSON log under each run directory.
- FULL stages: `SEMINAIVE_FULL`, `FORWARD_COMPILATION_FULL`, `WEIGHTED_MODEL_COUNTING_FULL`.
- INC stages: `SEMINAIVE_INC`, `FORWARD_COMPILATION_INC`, `WEIGHTED_MODEL_COUNTING_INC`.

## Tables: Per-Case Speedup (FULL / INC) by Delta Ratio

Averaging: 5 runs -> per-delta mean; per-delta FULL/INC speedup; average across 5 delta samples (same ratio). No cross-case averaging.

### P12

<!-- #### inc-naive speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 1.41x | 2.95x | 1.20x | 3.46x |
| 25% del / 75% ins | 1.76x | 2.67x | 2.70x | 5.58x |
| 50% del / 50% ins | 1.63x | 2.29x | 2.62x | 3.45x |
| 75% del / 25% ins | 2.84x | 1.92x | 6.78x | 4.77x |
| 100% del / 0% ins | 4.85x | 2.59x | 11.28x | 3.21x |

#### inc-regional speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 8.81x | 2.58x | 58.86x | 3.72x |
| 25% del / 75% ins | 4.58x | 2.47x | 9.55x | 7.12x |
| 50% del / 50% ins | 4.30x | 2.24x | 9.43x | 4.86x |
| 75% del / 25% ins | 3.90x | 1.84x | 10.63x | 6.10x |
| 100% del / 0% ins | 4.50x | 2.42x | 10.41x | 3.69x | -->

### P13

#### inc-naive speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 6.39x | 3.31x | 20.59x | 5.72x |
| 25% del / 75% ins | 5.23x | 2.39x | 9.94x | 5.28x |
| 50% del / 50% ins | 7.30x | 2.21x | 15.45x | 6.36x |
| 75% del / 25% ins | 7.16x | 2.06x | 15.29x | 6.69x |
| 100% del / 0% ins | 8.21x | 2.23x | 20.67x | 2.56x |

#### inc-regional speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 12.12x | 3.30x | 46.85x | 2.78x |
| 25% del / 75% ins | 7.58x | 2.51x | 15.02x | 3.54x |
| 50% del / 50% ins | 7.73x | 2.37x | 16.06x | 4.75x |
| 75% del / 25% ins | 7.64x | 2.23x | 16.39x | 4.80x |
| 100% del / 0% ins | 8.48x | 2.30x | 21.84x | 2.55x |

### P14

#### inc-naive speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 2.78x | 3.65x | 2.68x | 3.70x |
| 25% del / 75% ins | 2.05x | 2.43x | 1.96x | 5.70x |
| 50% del / 50% ins | 3.19x | 2.28x | 4.49x | 5.61x |
| 75% del / 25% ins | 8.06x | 2.32x | 15.81x | 7.54x |
| 100% del / 0% ins | 9.20x | 2.38x | 21.93x | 1.89x |

#### inc-regional speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 2.82x | 3.80x | 2.72x | 3.24x |
| 25% del / 75% ins | 8.79x | 2.49x | 17.51x | 4.49x |
| 50% del / 50% ins | 8.01x | 2.35x | 15.55x | 4.76x |
| 75% del / 25% ins | 8.12x | 2.26x | 16.85x | 5.88x |
| 100% del / 0% ins | 9.50x | 2.52x | 22.39x | 1.93x |

### P15

#### inc-naive speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 13.41x | 4.31x | 16.71x | 6.97x |
| 25% del / 75% ins | 10.56x | 2.96x | 12.99x | 7.42x |
| 50% del / 50% ins | 11.28x | 2.83x | 14.28x | 8.75x |
| 75% del / 25% ins | 13.43x | 2.57x | 18.62x | 12.75x |
| 100% del / 0% ins | 15.68x | 3.34x | 27.69x | 2.83x |

#### inc-regional speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 18.44x | 4.37x | 31.20x | 6.02x |
| 25% del / 75% ins | 12.80x | 2.93x | 17.74x | 6.96x |
| 50% del / 50% ins | 13.34x | 2.78x | 18.27x | 11.58x |
| 75% del / 25% ins | 14.32x | 2.56x | 21.16x | 12.06x |
| 100% del / 0% ins | 15.37x | 3.26x | 27.34x | 2.73x |

### P16

#### inc-naive speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 23.27x | 4.97x | 29.32x | 11.25x |
| 25% del / 75% ins | 19.03x | 3.34x | 23.62x | 14.67x |
| 50% del / 50% ins | 16.11x | 2.88x | 20.12x | 11.34x |
| 75% del / 25% ins | 16.11x | 2.79x | 20.45x | 13.39x |
| 100% del / 0% ins | 19.43x | 3.77x | 32.94x | 2.43x |

#### inc-regional speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 30.51x | 5.07x | 50.71x | 6.55x |
| 25% del / 75% ins | 21.10x | 3.28x | 29.38x | 9.67x |
| 50% del / 50% ins | 17.69x | 2.89x | 23.53x | 11.65x |
| 75% del / 25% ins | 16.96x | 2.80x | 22.43x | 12.65x |
| 100% del / 0% ins | 19.37x | 3.76x | 32.62x | 2.47x |

### P17

#### inc-naive speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 26.33x | 5.47x | 59.96x | 10.00x |
| 25% del / 75% ins | 15.27x | 3.79x | 24.57x | 14.34x |
| 50% del / 50% ins | 15.30x | 3.89x | 24.49x | 15.89x |
| 75% del / 25% ins | 15.24x | 3.85x | 24.36x | 20.29x |
| 100% del / 0% ins | 18.82x | 5.17x | 33.53x | 2.90x |

#### inc-regional speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 28.34x | 5.58x | 81.43x | 6.07x |
| 25% del / 75% ins | 15.73x | 3.81x | 26.42x | 10.57x |
| 50% del / 50% ins | 15.84x | 3.93x | 26.04x | 13.96x |
| 75% del / 25% ins | 15.76x | 3.85x | 25.82x | 18.44x |
| 100% del / 0% ins | 18.99x | 5.16x | 34.14x | 2.98x |

### P18

#### inc-naive speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 33.45x | 6.20x | 81.37x | 13.86x |
| 25% del / 75% ins | 18.08x | 4.04x | 29.31x | 23.08x |
| 50% del / 50% ins | 17.55x | 4.00x | 27.95x | 19.96x |
| 75% del / 25% ins | 18.08x | 4.01x | 29.27x | 16.99x |
| 100% del / 0% ins | 21.36x | 5.78x | 36.05x | 3.30x |

#### inc-regional speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 36.50x | 6.18x | 106.93x | 11.99x |
| 25% del / 75% ins | 18.59x | 4.07x | 30.51x | 17.55x |
| 50% del / 50% ins | 17.96x | 3.95x | 30.17x | 13.04x |
| 75% del / 25% ins | 18.22x | 3.98x | 30.76x | 9.41x |
| 100% del / 0% ins | 21.60x | 5.69x | 37.26x | 3.21x |

### P19

#### inc-naive speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 46.80x | 6.05x | 115.76x | 15.76x |
| 25% del / 75% ins | 23.32x | 4.09x | 38.00x | 25.31x |
| 50% del / 50% ins | 21.62x | 4.10x | 34.76x | 23.96x |
| 75% del / 25% ins | 20.07x | 4.07x | 32.01x | 17.57x |
| 100% del / 0% ins | 23.80x | 6.50x | 39.22x | 3.74x |

#### inc-regional speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 50.14x | 5.99x | 149.91x | 11.67x |
| 25% del / 75% ins | 23.89x | 4.11x | 39.60x | 21.50x |
| 50% del / 50% ins | 22.73x | 4.15x | 37.78x | 20.55x |
| 75% del / 25% ins | 20.95x | 4.11x | 34.13x | 14.95x |
| 100% del / 0% ins | 24.37x | 6.59x | 40.82x | 3.80x |

### P20

#### inc-naive speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 19.69x | 3.50x | 84.18x | 9.37x |
| 25% del / 75% ins | 9.10x | 2.14x | 16.94x | 9.06x |
| 50% del / 50% ins | 9.34x | 2.13x | 17.78x | 9.73x |
| 75% del / 25% ins | 9.38x | 1.94x | 19.04x | 9.21x |
| 100% del / 0% ins | 13.30x | 3.37x | 24.20x | 3.26x |

#### inc-regional speedup

| Delta ratio | End-to-end | Semi-naive | Forward Compilation | WMC |
|---|---:|---:|---:|---:|
| 0% del / 100% ins | 20.03x | 3.51x | 96.23x | 5.57x |
| 25% del / 75% ins | 9.46x | 2.13x | 18.55x | 6.63x |
| 50% del / 50% ins | 9.59x | 2.13x | 18.91x | 6.93x |
| 75% del / 25% ins | 9.46x | 1.92x | 19.77x | 6.66x |
| 100% del / 0% ins | 13.24x | 3.36x | 24.33x | 3.26x |

## Table: Pruned Graph Size per Case

Pre-prune nodes/edges from per-delta `inc_naive` stdout stats (`apply_delta_graph.totalNodes/totalEdges`).
Pruned nodes/edges from per-delta `inc_naive` stdout stats (`pruned_delta.totalNodes/totalEdges`).
Delta-reachable nodes and region/delta ratio use `inc_regional` stdout stats (`inc_regional_final`).
Rand vars from baseline FULL `FORWARD_COMPILATION_FULL.rand_vars`.

| Case | Pre-prune nodes | Pre-prune edges | Pruned nodes | Pruned edges | Rand vars | Avg delta reachable nodes | Region/Delta ratio (min-max) | Samples (pruned) | Samples (delta) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P12 | 13731 | 3537 | 3781 | 1668 | 874 | 55 | 0.54-1.00 | 125 | 85 |
| P13 | 26671 | 6841 | 8848 | 3899 | 2147 | 375 | 0.15-0.79 | 120 | 95 |
| P14 | 39807 | 10228 | 13128 | 5799 | 3168 | 514 | 0.20-0.79 | 125 | 100 |
| P15 | 91454 | 23259 | 28851 | 12775 | 6965 | 776 | 0.25-0.78 | 125 | 100 |
| P16 | 141596 | 36462 | 45263 | 20079 | 10878 | 600 | 0.20-0.95 | 125 | 100 |

## Source references
- [archive/README.md](archive/README.md)
- [archive/2026-02-11/README.md](archive/2026-02-11/README.md)

## Related commits
- `UNCOMMITTED` — docs(historical): archive paper table/figure working note under docs/historical
| P17 | 185506 | 46428 | 51364 | 22783 | 12369 | 778 | 0.26-0.66 | 125 | 100 |
| P18 | 235157 | 59109 | 67111 | 29773 | 16155 | 481 | 0.12-0.67 | 125 | 100 |
| P19 | 296752 | 74555 | 83687 | 37187 | 20169 | 576 | 0.18-0.56 | 125 | 100 |
| P20 | 341384 | 88599 | 122553 | 51341 | 33903 | 482 | 0.70-0.94 | 125 | 100 |
## Table: End-to-end speedup summary (inc-regional vs full, P13–P20)

Speedup = T_full / T_inc, using end-to-end turn time. For each delta sample, average 5 runs before computing speedup; aggregate across P13–P20 and all ratios/samples.

| Metric | Value |
|---|---:|
| N (delta samples) | 199 |
| Min | 2.78x |
| Median | 26.26x |
| Max | 55.19x |
| IQR | 10.90x |
| Average | 17.10x |
| % better (speedup > 1) | 100.0% |

## Table: Stage-level speedup summary (INC vs FULL, P13–P20)

Speedup = T_full / T_inc. For each delta sample, average 5 runs before computing speedup; aggregate across P13–P20 and all ratios/samples.

| Stage | N | Min | Median | Max | IQR | Average | % better (>1) |
|---|---:|---:|---:|---:|---:|---:|---:|
| SEMI_NAIVE_INC (inc-naive vs full) | 199 | 1.83x | 3.49x | 7.49x | 1.57x | 2.20x | 100.0% |
| SEMI_NAIVE_INC (inc-regional vs full) | 199 | 1.83x | 3.52x | 7.50x | 1.56x | 2.22x | 100.0% |
| FC_NAIVE_INC (inc-naive vs full) | 199 | 1.80x | 23.46x | 133.57x | 15.65x | 29.36x | 100.0% |
| FC_REGIONAL_INC (inc-regional vs full) | 199 | 2.68x | 26.26x | 162.54x | 17.08x | 35.36x | 100.0% |
| WMC_NAIVE_INC (inc-naive vs full) | 199 | 1.42x | 8.27x | 39.34x | 9.11x | 17.51x | 100.0% |
| WMC_REGIONAL_INC (inc-regional vs full) | 199 | 1.18x | 5.52x | 30.40x | 8.12x | 13.99x | 100.0% |
## Table: Stage times by insert ratio (FULL)

Each cell is the mean stage time (seconds) per case and insert ratio. Averaging: 5 runs -> per-sample mean; then average across 5 samples.

| Case | ins100-END_TO_END | ins100-SEMI_NAIVE | ins100-FORWARD_COMP | ins100-WMC | ins100-OVERHEAD | ins75-END_TO_END | ins75-SEMI_NAIVE | ins75-FORWARD_COMP | ins75-WMC | ins75-OVERHEAD | ins50-END_TO_END | ins50-SEMI_NAIVE | ins50-FORWARD_COMP | ins50-WMC | ins50-OVERHEAD | ins25-END_TO_END | ins25-SEMI_NAIVE | ins25-FORWARD_COMP | ins25-WMC | ins25-OVERHEAD | ins0-END_TO_END | ins0-SEMI_NAIVE | ins0-FORWARD_COMP | ins0-WMC | ins0-OVERHEAD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P12 | 0.2489 | 0.0476 | 0.1852 | 0.0006 | 0.0155 | 0.1627 | 0.0481 | 0.0979 | 0.0006 | 0.0161 | 0.1608 | 0.0466 | 0.0982 | 0.0005 | 0.0155 | 0.1617 | 0.0473 | 0.0983 | 0.0006 | 0.0155 | 0.1591 | 0.0462 | 0.0976 | 0.0006 | 0.0147 |
| P13 | 0.6297 | 0.0921 | 0.4932 | 0.0027 | 0.0416 | 0.6439 | 0.0937 | 0.5032 | 0.0024 | 0.0446 | 0.6531 | 0.0950 | 0.5077 | 0.0023 | 0.0481 | 0.6479 | 0.0919 | 0.5054 | 0.0021 | 0.0485 | 0.6472 | 0.0919 | 0.5055 | 0.0020 | 0.0479 |
| P14 | 1.4615 | 0.1472 | 1.2469 | 0.0052 | 0.0622 | 1.1928 | 0.1426 | 0.9787 | 0.0053 | 0.0662 | 1.1469 | 0.1460 | 0.9295 | 0.0050 | 0.0664 | 1.1270 | 0.1418 | 0.9186 | 0.0047 | 0.0618 | 1.0919 | 0.1369 | 0.8939 | 0.0042 | 0.0569 |
| P15 | 7.1074 | 0.3468 | 6.0660 | 0.2821 | 0.4125 | 7.1046 | 0.3511 | 6.0738 | 0.2588 | 0.4210 | 6.9036 | 0.3480 | 5.9356 | 0.2173 | 0.4027 | 6.4576 | 0.3122 | 5.6016 | 0.1859 | 0.3579 | 5.8043 | 0.3116 | 4.9722 | 0.1592 | 0.3612 |
| P16 | 15.6161 | 0.4649 | 13.9852 | 0.4147 | 0.7513 | 15.5439 | 0.4600 | 13.9395 | 0.4037 | 0.7407 | 14.4236 | 0.4509 | 12.9093 | 0.3587 | 0.7046 | 13.1628 | 0.4540 | 11.6433 | 0.3474 | 0.7182 | 12.5388 | 0.4624 | 11.0247 | 0.3178 | 0.7339 |
| P17 | 8.4714 | 0.6424 | 7.2851 | 0.0804 | 0.4636 | 8.0446 | 0.6355 | 6.8734 | 0.0769 | 0.4589 | 8.0735 | 0.6589 | 6.8725 | 0.0670 | 0.4751 | 8.1039 | 0.6416 | 6.9226 | 0.0711 | 0.4685 | 8.0826 | 0.6692 | 6.8700 | 0.0613 | 0.4822 |
| P18 | 12.6239 | 0.8564 | 11.1531 | 0.0915 | 0.5229 | 12.6690 | 0.8719 | 11.1768 | 0.0886 | 0.5316 | 12.6856 | 0.8705 | 11.1950 | 0.0896 | 0.5305 | 12.6836 | 0.8676 | 11.2017 | 0.0888 | 0.5256 | 12.9942 | 0.9165 | 11.4350 | 0.0911 | 0.5515 |
| P19 | 23.7193 | 1.0994 | 21.8431 | 0.1582 | 0.6186 | 21.0710 | 1.0915 | 19.2189 | 0.1439 | 0.6167 | 20.3594 | 1.1220 | 18.4647 | 0.1460 | 0.6268 | 21.0137 | 1.2055 | 18.9651 | 0.1526 | 0.6905 | 20.4435 | 1.2983 | 18.2364 | 0.1603 | 0.7485 |
| P20 | 15.3593 | 1.5905 | 13.0183 | 0.0496 | 0.7008 | 15.3555 | 1.5829 | 13.0215 | 0.0499 | 0.7011 | 15.3585 | 1.5573 | 13.0514 | 0.0515 | 0.6984 | 15.4854 | 1.5100 | 13.2560 | 0.0487 | 0.6706 | 15.2412 | 1.4474 | 13.0824 | 0.0445 | 0.6669 |

## Table: Stage times by insert ratio (INC-regional)

Each cell is the mean stage time (seconds) per case and insert ratio. Averaging: 5 runs -> per-sample mean; then average across 5 samples.

| Case | ins100-END_TO_END | ins100-SEMI_NAIVE | ins100-FORWARD_COMP | ins100-WMC | ins100-OVERHEAD | ins75-END_TO_END | ins75-SEMI_NAIVE | ins75-FORWARD_COMP | ins75-WMC | ins75-OVERHEAD | ins50-END_TO_END | ins50-SEMI_NAIVE | ins50-FORWARD_COMP | ins50-WMC | ins50-OVERHEAD | ins25-END_TO_END | ins25-SEMI_NAIVE | ins25-FORWARD_COMP | ins25-WMC | ins25-OVERHEAD | ins0-END_TO_END | ins0-SEMI_NAIVE | ins0-FORWARD_COMP | ins0-WMC | ins0-OVERHEAD |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P12 | 0.0282 | 0.0191 | 0.0032 | 0.0002 | 0.0058 | 0.0356 | 0.0197 | 0.0104 | 0.0001 | 0.0055 | 0.0369 | 0.0211 | 0.0102 | 0.0001 | 0.0056 | 0.0420 | 0.0261 | 0.0093 | 0.0001 | 0.0064 | 0.0360 | 0.0203 | 0.0094 | 0.0002 | 0.0061 |
| P13 | 0.0521 | 0.0281 | 0.0110 | 0.0012 | 0.0119 | 0.0854 | 0.0379 | 0.0338 | 0.0008 | 0.0129 | 0.0846 | 0.0400 | 0.0318 | 0.0005 | 0.0123 | 0.0852 | 0.0414 | 0.0309 | 0.0005 | 0.0124 | 0.0764 | 0.0400 | 0.0232 | 0.0008 | 0.0125 |
| P14 | 0.5191 | 0.0388 | 0.4591 | 0.0016 | 0.0195 | 0.1359 | 0.0574 | 0.0563 | 0.0012 | 0.0210 | 0.1436 | 0.0624 | 0.0600 | 0.0011 | 0.0201 | 0.1390 | 0.0632 | 0.0547 | 0.0009 | 0.0203 | 0.1151 | 0.0545 | 0.0401 | 0.0022 | 0.0182 |
| P15 | 0.3990 | 0.0793 | 0.2137 | 0.0570 | 0.0490 | 0.5660 | 0.1207 | 0.3533 | 0.0434 | 0.0486 | 0.5242 | 0.1255 | 0.3303 | 0.0228 | 0.0456 | 0.4533 | 0.1219 | 0.2657 | 0.0257 | 0.0401 | 0.3884 | 0.0963 | 0.1917 | 0.0605 | 0.0399 |
| P16 | 0.5188 | 0.0919 | 0.2764 | 0.0920 | 0.0585 | 0.7384 | 0.1413 | 0.4767 | 0.0630 | 0.0574 | 0.8475 | 0.1570 | 0.5720 | 0.0630 | 0.0555 | 0.7792 | 0.1620 | 0.5229 | 0.0362 | 0.0581 | 0.6506 | 0.1239 | 0.3401 | 0.1284 | 0.0583 |
| P17 | 0.2985 | 0.1154 | 0.0895 | 0.0210 | 0.0726 | 0.5115 | 0.1672 | 0.2602 | 0.0108 | 0.0733 | 0.5097 | 0.1680 | 0.2641 | 0.0060 | 0.0717 | 0.5146 | 0.1668 | 0.2686 | 0.0044 | 0.0748 | 0.4261 | 0.1302 | 0.2017 | 0.0208 | 0.0733 |
| P18 | 0.3470 | 0.1389 | 0.1051 | 0.0087 | 0.0943 | 0.6816 | 0.2142 | 0.3665 | 0.0056 | 0.0953 | 0.7079 | 0.2209 | 0.3717 | 0.0153 | 0.1000 | 0.6969 | 0.2185 | 0.3656 | 0.0137 | 0.0991 | 0.6021 | 0.1614 | 0.3076 | 0.0285 | 0.1046 |
| P19 | 0.4735 | 0.1836 | 0.1458 | 0.0198 | 0.1243 | 0.8822 | 0.2653 | 0.4850 | 0.0072 | 0.1248 | 0.8943 | 0.2713 | 0.4883 | 0.0073 | 0.1274 | 1.0088 | 0.2948 | 0.5600 | 0.0126 | 0.1414 | 0.8399 | 0.1981 | 0.4477 | 0.0424 | 0.1517 |
| P20 | 0.7673 | 0.4531 | 0.1353 | 0.0089 | 0.1699 | 1.6236 | 0.7438 | 0.7026 | 0.0075 | 0.1697 | 1.6014 | 0.7307 | 0.6914 | 0.0074 | 0.1719 | 1.6364 | 0.7853 | 0.6711 | 0.0073 | 0.1727 | 1.1524 | 0.4308 | 0.5394 | 0.0137 | 0.1686 |

## Tables: Per-case stage times by insert ratio (FULL + INC-naive + INC-regional)

Each cell is the mean stage time (seconds). Averaging: 5 runs -> per-sample mean; then average across 5 samples.

### P12

#### FULL

| Insert ratio | End-to-end | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.2489 | 0.0476 | 0.1852 | 0.0006 | 0.0155 |
| 25% del / 75% ins | 0.1627 | 0.0481 | 0.0979 | 0.0006 | 0.0161 |
| 50% del / 50% ins | 0.1608 | 0.0466 | 0.0982 | 0.0005 | 0.0155 |
| 75% del / 25% ins | 0.1617 | 0.0473 | 0.0983 | 0.0006 | 0.0155 |
| 100% del / 0% ins | 0.1591 | 0.0462 | 0.0976 | 0.0006 | 0.0147 |

#### INC-naive

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.1758 | 1.42x | 0.0163 | 0.1537 | 0.0002 | 0.0056 |
| 25% del / 75% ins | 0.1537 | 1.06x | 0.0180 | 0.1304 | 0.0001 | 0.0052 |
| 50% del / 50% ins | 0.1547 | 1.04x | 0.0205 | 0.1282 | 0.0002 | 0.0059 |
| 75% del / 25% ins | 0.0991 | 1.63x | 0.0249 | 0.0681 | 0.0001 | 0.0060 |
| 100% del / 0% ins | 0.0336 | 4.74x | 0.0189 | 0.0087 | 0.0002 | 0.0058 |

#### INC-regional

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.0282 | 8.83x | 0.0191 | 0.0032 | 0.0002 | 0.0058 |
| 25% del / 75% ins | 0.0356 | 4.57x | 0.0197 | 0.0104 | 0.0001 | 0.0055 |
| 50% del / 50% ins | 0.0369 | 4.35x | 0.0211 | 0.0102 | 0.0001 | 0.0056 |
| 75% del / 25% ins | 0.0420 | 3.85x | 0.0261 | 0.0093 | 0.0001 | 0.0064 |
| 100% del / 0% ins | 0.0360 | 4.42x | 0.0203 | 0.0094 | 0.0002 | 0.0061 |

### P13

#### FULL

| Insert ratio | End-to-end | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.6297 | 0.0921 | 0.4932 | 0.0027 | 0.0416 |
| 25% del / 75% ins | 0.6439 | 0.0937 | 0.5032 | 0.0024 | 0.0446 |
| 50% del / 50% ins | 0.6531 | 0.0950 | 0.5077 | 0.0023 | 0.0481 |
| 75% del / 25% ins | 0.6479 | 0.0919 | 0.5054 | 0.0021 | 0.0485 |
| 100% del / 0% ins | 0.6472 | 0.0919 | 0.5055 | 0.0020 | 0.0479 |

#### INC-naive

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.2014 | 3.13x | 0.0280 | 0.1608 | 0.0005 | 0.0122 |
| 25% del / 75% ins | 0.1831 | 3.52x | 0.0396 | 0.1303 | 0.0005 | 0.0128 |
| 50% del / 50% ins | 0.0894 | 7.31x | 0.0431 | 0.0330 | 0.0004 | 0.0129 |
| 75% del / 25% ins | 0.0906 | 7.15x | 0.0449 | 0.0331 | 0.0003 | 0.0123 |
| 100% del / 0% ins | 0.0789 | 8.20x | 0.0413 | 0.0245 | 0.0008 | 0.0123 |

#### INC-regional

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.0521 | 12.08x | 0.0281 | 0.0110 | 0.0012 | 0.0119 |
| 25% del / 75% ins | 0.0854 | 7.54x | 0.0379 | 0.0338 | 0.0008 | 0.0129 |
| 50% del / 50% ins | 0.0846 | 7.72x | 0.0400 | 0.0318 | 0.0005 | 0.0123 |
| 75% del / 25% ins | 0.0852 | 7.61x | 0.0414 | 0.0309 | 0.0005 | 0.0124 |
| 100% del / 0% ins | 0.0764 | 8.47x | 0.0400 | 0.0232 | 0.0008 | 0.0125 |

### P14

#### FULL

| Insert ratio | End-to-end | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 1.4615 | 0.1472 | 1.2469 | 0.0052 | 0.0622 |
| 25% del / 75% ins | 1.1928 | 0.1426 | 0.9787 | 0.0053 | 0.0662 |
| 50% del / 50% ins | 1.1469 | 0.1460 | 0.9295 | 0.0050 | 0.0664 |
| 75% del / 25% ins | 1.1270 | 0.1418 | 0.9186 | 0.0047 | 0.0618 |
| 100% del / 0% ins | 1.0919 | 0.1369 | 0.8939 | 0.0042 | 0.0569 |

#### INC-naive

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.5261 | 2.78x | 0.0405 | 0.4652 | 0.0014 | 0.0190 |
| 25% del / 75% ins | 0.5809 | 2.05x | 0.0588 | 0.4999 | 0.0010 | 0.0212 |
| 50% del / 50% ins | 0.4878 | 2.35x | 0.0640 | 0.4030 | 0.0009 | 0.0200 |
| 75% del / 25% ins | 0.1407 | 8.01x | 0.0620 | 0.0582 | 0.0007 | 0.0198 |
| 100% del / 0% ins | 0.1190 | 9.18x | 0.0579 | 0.0409 | 0.0023 | 0.0179 |

#### INC-regional

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.5191 | 2.82x | 0.0388 | 0.4591 | 0.0016 | 0.0195 |
| 25% del / 75% ins | 0.1359 | 8.78x | 0.0574 | 0.0563 | 0.0012 | 0.0210 |
| 50% del / 50% ins | 0.1436 | 7.99x | 0.0624 | 0.0600 | 0.0011 | 0.0201 |
| 75% del / 25% ins | 0.1390 | 8.11x | 0.0632 | 0.0547 | 0.0009 | 0.0203 |
| 100% del / 0% ins | 0.1151 | 9.49x | 0.0545 | 0.0401 | 0.0022 | 0.0182 |

### P15

#### FULL

| Insert ratio | End-to-end | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 7.1074 | 0.3468 | 6.0660 | 0.2821 | 0.4125 |
| 25% del / 75% ins | 7.1046 | 0.3511 | 6.0738 | 0.2588 | 0.4210 |
| 50% del / 50% ins | 6.9036 | 0.3480 | 5.9356 | 0.2173 | 0.4027 |
| 75% del / 25% ins | 6.4576 | 0.3122 | 5.6016 | 0.1859 | 0.3579 |
| 100% del / 0% ins | 5.8043 | 0.3116 | 4.9722 | 0.1592 | 0.3612 |

#### INC-naive

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.5377 | 13.22x | 0.0806 | 0.3707 | 0.0416 | 0.0449 |
| 25% del / 75% ins | 0.6829 | 10.40x | 0.1192 | 0.4773 | 0.0383 | 0.0481 |
| 50% del / 50% ins | 0.6187 | 11.16x | 0.1233 | 0.4218 | 0.0289 | 0.0447 |
| 75% del / 25% ins | 0.4854 | 13.30x | 0.1215 | 0.3067 | 0.0171 | 0.0401 |
| 100% del / 0% ins | 0.3818 | 15.20x | 0.0944 | 0.1893 | 0.0583 | 0.0398 |

#### INC-regional

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.3990 | 17.81x | 0.0793 | 0.2137 | 0.0570 | 0.0490 |
| 25% del / 75% ins | 0.5660 | 12.55x | 0.1207 | 0.3533 | 0.0434 | 0.0486 |
| 50% del / 50% ins | 0.5242 | 13.17x | 0.1255 | 0.3303 | 0.0228 | 0.0456 |
| 75% del / 25% ins | 0.4533 | 14.25x | 0.1219 | 0.2657 | 0.0257 | 0.0401 |
| 100% del / 0% ins | 0.3884 | 14.94x | 0.0963 | 0.1917 | 0.0605 | 0.0399 |

### P16

#### FULL

| Insert ratio | End-to-end | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 15.6161 | 0.4649 | 13.9852 | 0.4147 | 0.7513 |
| 25% del / 75% ins | 15.5439 | 0.4600 | 13.9395 | 0.4037 | 0.7407 |
| 50% del / 50% ins | 14.4236 | 0.4509 | 12.9093 | 0.3587 | 0.7046 |
| 75% del / 25% ins | 13.1628 | 0.4540 | 11.6433 | 0.3474 | 0.7182 |
| 100% del / 0% ins | 12.5388 | 0.4624 | 11.0247 | 0.3178 | 0.7339 |

#### INC-naive

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.6894 | 22.65x | 0.0941 | 0.4963 | 0.0417 | 0.0573 |
| 25% del / 75% ins | 0.8269 | 18.80x | 0.1383 | 0.6009 | 0.0309 | 0.0567 |
| 50% del / 50% ins | 0.9249 | 15.59x | 0.1576 | 0.6684 | 0.0426 | 0.0563 |
| 75% del / 25% ins | 0.8175 | 16.10x | 0.1627 | 0.5711 | 0.0275 | 0.0562 |
| 100% del / 0% ins | 0.6500 | 19.29x | 0.1234 | 0.3379 | 0.1309 | 0.0578 |

#### INC-regional

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.5188 | 30.10x | 0.0919 | 0.2764 | 0.0920 | 0.0585 |
| 25% del / 75% ins | 0.7384 | 21.05x | 0.1413 | 0.4767 | 0.0630 | 0.0574 |
| 50% del / 50% ins | 0.8475 | 17.02x | 0.1570 | 0.5720 | 0.0630 | 0.0555 |
| 75% del / 25% ins | 0.7792 | 16.89x | 0.1620 | 0.5229 | 0.0362 | 0.0581 |
| 100% del / 0% ins | 0.6506 | 19.27x | 0.1239 | 0.3401 | 0.1284 | 0.0583 |

### P17

#### FULL

| Insert ratio | End-to-end | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 8.4714 | 0.6424 | 7.2851 | 0.0804 | 0.4636 |
| 25% del / 75% ins | 8.0446 | 0.6355 | 6.8734 | 0.0769 | 0.4589 |
| 50% del / 50% ins | 8.0735 | 0.6589 | 6.8725 | 0.0670 | 0.4751 |
| 75% del / 25% ins | 8.1039 | 0.6416 | 6.9226 | 0.0711 | 0.4685 |
| 100% del / 0% ins | 8.0826 | 0.6692 | 6.8700 | 0.0613 | 0.4822 |

#### INC-naive

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.3218 | 26.32x | 0.1179 | 0.1216 | 0.0086 | 0.0736 |
| 25% del / 75% ins | 0.5269 | 15.27x | 0.1679 | 0.2799 | 0.0060 | 0.0730 |
| 50% del / 50% ins | 0.5279 | 15.29x | 0.1696 | 0.2809 | 0.0054 | 0.0720 |
| 75% del / 25% ins | 0.5324 | 15.22x | 0.1667 | 0.2849 | 0.0042 | 0.0766 |
| 100% del / 0% ins | 0.4302 | 18.79x | 0.1299 | 0.2057 | 0.0213 | 0.0733 |

#### INC-regional

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.2985 | 28.38x | 0.1154 | 0.0895 | 0.0210 | 0.0726 |
| 25% del / 75% ins | 0.5115 | 15.73x | 0.1672 | 0.2602 | 0.0108 | 0.0733 |
| 50% del / 50% ins | 0.5097 | 15.84x | 0.1680 | 0.2641 | 0.0060 | 0.0717 |
| 75% del / 25% ins | 0.5146 | 15.75x | 0.1668 | 0.2686 | 0.0044 | 0.0748 |
| 100% del / 0% ins | 0.4261 | 18.97x | 0.1302 | 0.2017 | 0.0208 | 0.0733 |

### P18

#### FULL

| Insert ratio | End-to-end | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 12.6239 | 0.8564 | 11.1531 | 0.0915 | 0.5229 |
| 25% del / 75% ins | 12.6690 | 0.8719 | 11.1768 | 0.0886 | 0.5316 |
| 50% del / 50% ins | 12.6856 | 0.8705 | 11.1950 | 0.0896 | 0.5305 |
| 75% del / 25% ins | 12.6836 | 0.8676 | 11.2017 | 0.0888 | 0.5256 |
| 100% del / 0% ins | 12.9942 | 0.9165 | 11.4350 | 0.0911 | 0.5515 |

#### INC-naive

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.3788 | 33.33x | 0.1389 | 0.1384 | 0.0071 | 0.0943 |
| 25% del / 75% ins | 0.7008 | 18.08x | 0.2160 | 0.3818 | 0.0042 | 0.0988 |
| 50% del / 50% ins | 0.7234 | 17.54x | 0.2185 | 0.4009 | 0.0062 | 0.0977 |
| 75% del / 25% ins | 0.7015 | 18.08x | 0.2163 | 0.3832 | 0.0053 | 0.0966 |
| 100% del / 0% ins | 0.6090 | 21.34x | 0.1589 | 0.3176 | 0.0277 | 0.1049 |

#### INC-regional

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.3470 | 36.38x | 0.1389 | 0.1051 | 0.0087 | 0.0943 |
| 25% del / 75% ins | 0.6816 | 18.59x | 0.2142 | 0.3665 | 0.0056 | 0.0953 |
| 50% del / 50% ins | 0.7079 | 17.92x | 0.2209 | 0.3717 | 0.0153 | 0.1000 |
| 75% del / 25% ins | 0.6969 | 18.20x | 0.2185 | 0.3656 | 0.0137 | 0.0991 |
| 100% del / 0% ins | 0.6021 | 21.58x | 0.1614 | 0.3076 | 0.0285 | 0.1046 |

### P19

#### FULL

| Insert ratio | End-to-end | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 23.7193 | 1.0994 | 21.8431 | 0.1582 | 0.6186 |
| 25% del / 75% ins | 21.0710 | 1.0915 | 19.2189 | 0.1439 | 0.6167 |
| 50% del / 50% ins | 20.3594 | 1.1220 | 18.4647 | 0.1460 | 0.6268 |
| 75% del / 25% ins | 21.0137 | 1.2055 | 18.9651 | 0.1526 | 0.6905 |
| 100% del / 0% ins | 20.4435 | 1.2983 | 18.2364 | 0.1603 | 0.7485 |

#### INC-naive

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.5086 | 46.63x | 0.1818 | 0.1906 | 0.0107 | 0.1254 |
| 25% del / 75% ins | 0.9031 | 23.33x | 0.2672 | 0.5052 | 0.0061 | 0.1247 |
| 50% del / 50% ins | 0.9421 | 21.61x | 0.2743 | 0.5330 | 0.0062 | 0.1286 |
| 75% del / 25% ins | 1.0570 | 19.88x | 0.2983 | 0.6021 | 0.0108 | 0.1459 |
| 100% del / 0% ins | 0.8623 | 23.71x | 0.2011 | 0.4667 | 0.0433 | 0.1513 |

#### INC-regional

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.4735 | 50.09x | 0.1836 | 0.1458 | 0.0198 | 0.1243 |
| 25% del / 75% ins | 0.8822 | 23.88x | 0.2653 | 0.4850 | 0.0072 | 0.1248 |
| 50% del / 50% ins | 0.8943 | 22.77x | 0.2713 | 0.4883 | 0.0073 | 0.1274 |
| 75% del / 25% ins | 1.0088 | 20.83x | 0.2948 | 0.5600 | 0.0126 | 0.1414 |
| 100% del / 0% ins | 0.8399 | 24.34x | 0.1981 | 0.4477 | 0.0424 | 0.1517 |

### P20

#### FULL

| Insert ratio | End-to-end | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 15.3593 | 1.5905 | 13.0183 | 0.0496 | 0.7008 |
| 25% del / 75% ins | 15.3555 | 1.5829 | 13.0215 | 0.0499 | 0.7011 |
| 50% del / 50% ins | 15.3585 | 1.5573 | 13.0514 | 0.0515 | 0.6984 |
| 75% del / 25% ins | 15.4854 | 1.5100 | 13.2560 | 0.0487 | 0.6706 |
| 100% del / 0% ins | 15.2412 | 1.4474 | 13.0824 | 0.0445 | 0.6669 |

#### INC-naive

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.7800 | 19.69x | 0.4549 | 0.1547 | 0.0053 | 0.1651 |
| 25% del / 75% ins | 1.6889 | 9.09x | 0.7418 | 0.7695 | 0.0055 | 0.1722 |
| 50% del / 50% ins | 1.6442 | 9.34x | 0.7320 | 0.7343 | 0.0053 | 0.1725 |
| 75% del / 25% ins | 1.6518 | 9.37x | 0.7797 | 0.6975 | 0.0053 | 0.1693 |
| 100% del / 0% ins | 1.1468 | 13.29x | 0.4303 | 0.5415 | 0.0137 | 0.1613 |

#### INC-regional

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.7673 | 20.02x | 0.4531 | 0.1353 | 0.0089 | 0.1699 |
| 25% del / 75% ins | 1.6236 | 9.46x | 0.7438 | 0.7026 | 0.0075 | 0.1697 |
| 50% del / 50% ins | 1.6014 | 9.59x | 0.7307 | 0.6914 | 0.0074 | 0.1719 |
| 75% del / 25% ins | 1.6364 | 9.46x | 0.7853 | 0.6711 | 0.0073 | 0.1727 |
| 100% del / 0% ins | 1.1524 | 13.23x | 0.4308 | 0.5394 | 0.0137 | 0.1686 |

## Table: INC-regional stage times (P13–P20 average)

Averaging: 5 runs -> per-sample mean; 5 samples -> per-case mean; then average across P13–P20.

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.4219 | 25.18x | 0.1411 | 0.1795 | 0.0263 | 0.0750 |
| 25% del / 75% ins | 0.6531 | 15.62x | 0.2185 | 0.3418 | 0.0174 | 0.0754 |
| 50% del / 50% ins | 0.6642 | 14.98x | 0.2220 | 0.3512 | 0.0154 | 0.0756 |
| 75% del / 25% ins | 0.6642 | 14.81x | 0.2317 | 0.3424 | 0.0127 | 0.0773 |
| 100% del / 0% ins | 0.5314 | 18.08x | 0.1544 | 0.2614 | 0.0372 | 0.0784 |

## Table: FC speedup summary (insert ratio 100%, P13–P20)

Speedup = T_full / T_inc for FORWARD_COMPILATION. For each delta sample, average 5 runs before computing speedup; aggregate across P13–P20, insert ratio 100% only.

| Stage | N | Min | Median | Max | IQR | Average | % better (>1) |
|---|---:|---:|---:|---:|---:|---:|---:|
| FC_NAIVE_INC (inc-naive vs full) | 40 | 1.82x | 50.31x | 133.57x | 67.14x | 51.32x | 100.0% |
| FC_REGIONAL_INC (inc-regional vs full) | 40 | 2.68x | 68.05x | 162.54x | 60.19x | 70.75x | 100.0% |

## Table: SEMI_NAIVE speedup summary (insert ratio 100%, P13–P20)

Speedup = T_full / T_inc for SEMI_NAIVE. For each delta sample, average 5 runs before computing speedup; aggregate across P13–P20, insert ratio 100% only.

| Stage | N | Min | Median | Max | IQR | Average | % better (>1) |
|---|---:|---:|---:|---:|---:|---:|---:|
| SEMI_NAIVE_INC (inc-naive vs full) | 40 | 1.74x | 2.94x | 4.04x | 1.20x | 2.88x | 100.0% |
| SEMI_NAIVE_INC (inc-regional vs full) | 40 | 1.70x | 2.96x | 3.94x | 1.18x | 2.90x | 100.0% |




## Table: INC-regional stage times (P13–P20 average) for 0.01 mix-ratio; we need the same table for 0.005 and 0.015

Averaging: 5 runs -> per-sample mean; 5 samples -> per-case mean; then average across P13–P20.

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.4219 | 25.18x | 0.1411 | 0.1795 | 0.0263 | 0.0750 |
| 25% del / 75% ins | 0.6531 | 15.62x | 0.2185 | 0.3418 | 0.0174 | 0.0754 |
| 50% del / 50% ins | 0.6642 | 14.98x | 0.2220 | 0.3512 | 0.0154 | 0.0756 |
| 75% del / 25% ins | 0.6642 | 14.81x | 0.2317 | 0.3424 | 0.0127 | 0.0773 |
| 100% del / 0% ins | 0.5314 | 18.08x | 0.1544 | 0.2614 | 0.0372 | 0.0784 |
## Table: INC-regional stage times (P13–P20 average) for 0.005 mix-ratio

Averaging: 3 runs -> per-sample mean; 5 samples -> per-case mean; then average across P13–P20.

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.3059 | 31.24x | 0.1308 | 0.0958 | 0.0123 | 0.0669 |
| 25% del / 75% ins | 0.5692 | 17.88x | 0.2020 | 0.2914 | 0.0072 | 0.0685 |
| 50% del / 50% ins | 0.6060 | 16.59x | 0.2093 | 0.3150 | 0.0110 | 0.0706 |
| 75% del / 25% ins | 0.5948 | 16.40x | 0.2065 | 0.3122 | 0.0073 | 0.0688 |
| 100% del / 0% ins | 0.4743 | 19.88x | 0.1336 | 0.2380 | 0.0332 | 0.0694 |

## Table: INC-regional stage times (P13–P20 average) for 0.015 mix-ratio

Averaging: 3 runs -> per-sample mean; 5 samples -> per-case mean; then average across P13–P20.

| Insert ratio | End-to-end | End-to-end speedup vs full | Semi-naive | Forward Comp | WMC | Overhead |
|---|---:|---:|---:|---:|---:|---:|
| 0% del / 100% ins | 0.4128 | 22.58x | 0.1495 | 0.1435 | 0.0421 | 0.0777 |
| 25% del / 75% ins | 0.7118 | 13.20x | 0.2313 | 0.3682 | 0.0333 | 0.0791 |
| 50% del / 50% ins | 0.7088 | 12.55x | 0.2451 | 0.3689 | 0.0155 | 0.0794 |
| 75% del / 25% ins | 0.6861 | 12.45x | 0.2444 | 0.3495 | 0.0133 | 0.0789 |
| 100% del / 0% ins | 0.5852 | 14.51x | 0.1719 | 0.2924 | 0.0387 | 0.0823 |

### Notes: problematic runs

The following inc-regional runs exited with non-zero status and were excluded from averages:
- mix-ratio 0.015: P13 mix-d50-i50 sample-1 (runs 1-3)
- mix-ratio 0.015: P17 mix-d75-i25 sample-3 (runs 1-3)

## Table: FC+WMC speedup (naive / regional) statistics

Based on `avg_fc_wmc_speedup_naive_over_regional` from `inc_regional_vs_naive_fc_wmc_per_case_delta.tsv`
(per case + per delta averages). IQR uses linear percentiles; % better counts speedup > 1.

| Group | N | Arithmetic Mean | Geometric Mean | Min | Median | Max | IQR | % better |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| All ratios | 40 | 2.0314 | 1.4123 | 0.9851 | 1.0759 | 11.9651 | 0.1728 | 97.5% |
| 100% insert (mix-d0-i100) | 8 | 3.8357 | 2.2027 | 1.0136 | 1.3693 | 11.9651 | 2.8361 | 100.0% |
| 100% delete (mix-d100-i0) | 8 | 1.0232 | 1.0230 | 0.9851 | 1.0260 | 1.0600 | 0.0285 | 87.5% |
| Other ratios (mix-d25/50/75) | 24 | 1.7661 | 1.3560 | 1.0303 | 1.0808 | 8.8110 | 0.0820 | 100.0% |



  | Insert ratio | Case | End‑to‑end naive (s) | End‑to‑end regional (s) | Speedup (naive/regional) | Samples |
  |---|---|---:|---:|---:|---:|
  | 0% del / 100% ins | P13 | 0.2014 | 0.0521 | 3.86x | 5 |
  | 0% del / 100% ins | P14 | 0.5261 | 0.5191 | 1.01x | 5 |
  | 0% del / 100% ins | P15 | 0.5377 | 0.3990 | 1.35x | 5 |
  | 0% del / 100% ins | P16 | 0.6894 | 0.5188 | 1.33x | 5 |
  | 0% del / 100% ins | P17 | 0.3218 | 0.2985 | 1.08x | 5 |
  | 0% del / 100% ins | P18 | 0.3788 | 0.3470 | 1.09x | 5 |
  | 0% del / 100% ins | P19 | 2.5395 | 0.4735 | 5.36x | 5 |
  | 0% del / 100% ins | P20 | 0.7800 | 0.7673 | 1.02x | 5 |
  | 25% del / 75% ins | P13 | 0.1831 | 0.0854 | 2.14x | 5 |
  | 25% del / 75% ins | P14 | 0.5809 | 0.1359 | 4.28x | 5 |
  | 25% del / 75% ins | P15 | 0.6829 | 0.5660 | 1.21x | 5 |
  | 25% del / 75% ins | P16 | 0.8269 | 0.7384 | 1.12x | 5 |
  | 25% del / 75% ins | P17 | 0.5269 | 0.5115 | 1.03x | 5 |
  | 25% del / 75% ins | P18 | 0.7008 | 0.6816 | 1.03x | 5 |
  | 25% del / 75% ins | P19 | 0.9031 | 0.8822 | 1.02x | 5 |
  | 25% del / 75% ins | P20 | 1.6889 | 1.6236 | 1.04x | 5 |
  | 50% del / 50% ins | P13 | 0.0894 | 0.0846 | 1.06x | 4 |
  | 50% del / 50% ins | P14 | 0.4878 | 0.1436 | 3.40x | 5 |
  | 50% del / 50% ins | P15 | 0.6187 | 0.5242 | 1.18x | 5 |
  | 50% del / 50% ins | P16 | 0.9249 | 0.8475 | 1.09x | 5 |
  | 50% del / 50% ins | P17 | 0.5279 | 0.5097 | 1.04x | 5 |
  | 50% del / 50% ins | P18 | 0.7234 | 0.7079 | 1.02x | 5 |
  | 50% del / 50% ins | P19 | 0.9421 | 0.8943 | 1.05x | 5 |
  | 50% del / 50% ins | P20 | 1.6442 | 1.6014 | 1.03x | 5 |
  | 75% del / 25% ins | P13 | 0.0906 | 0.0852 | 1.06x | 5 |
  | 75% del / 25% ins | P14 | 0.1407 | 0.1390 | 1.01x | 5 |
  | 75% del / 25% ins | P15 | 0.4854 | 0.4533 | 1.07x | 5 |
  | 75% del / 25% ins | P16 | 0.8175 | 0.7792 | 1.05x | 5 |
  | 75% del / 25% ins | P17 | 0.5324 | 0.5146 | 1.03x | 5 |
  | 75% del / 25% ins | P18 | 0.7015 | 0.6969 | 1.01x | 5 |
  | 75% del / 25% ins | P19 | 1.0570 | 1.0088 | 1.05x | 5 |
  | 75% del / 25% ins | P20 | 1.6518 | 1.6364 | 1.01x | 5 |
  | 100% del / 0% ins | P13 | 0.0789 | 0.0764 | 1.03x | 5 |
  | 100% del / 0% ins | P14 | 0.1190 | 0.1151 | 1.03x | 5 |
  | 100% del / 0% ins | P15 | 0.3818 | 0.3884 | 0.98x | 5 |
  | 100% del / 0% ins | P16 | 0.6500 | 0.6506 | 1.00x | 5 |
  | 100% del / 0% ins | P17 | 0.4302 | 0.4261 | 1.01x | 5 |
  | 100% del / 0% ins | P18 | 0.6090 | 0.6021 | 1.01x | 5 |
  | 100% del / 0% ins | P19 | 0.8623 | 0.8399 | 1.03x | 5 |
  | 100% del / 0% ins | P20 | 1.1468 | 1.1524 | 1.00x | 5 |
