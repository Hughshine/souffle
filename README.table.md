# README.table.md

This file is a **table-generation guide** (and a set of ready-to-use prompts) for Codex.
It explains **what to extract from the collected experiment data**, **how to compute the
reported metrics**, and **how to translate the results into LaTeX tables** consistent with
our current paper drafts (LNCS/CAV style, `booktabs`).

---

## A. Incremental derivation-graph generation (SEM stage)

### A.1 Data source

Use:

- `semnaive-comparison-with-graph.tsv`

Each row corresponds to **one benchmark case**, one **edit ratio** (Δ), and one **update type**
(insertion-only or deletion-only), and reports both runtime and graph-size information.

### A.2 Row schema (TSV columns)

The TSV contains the following columns (names must match exactly):

- `Case`: benchmark id, e.g., `P1`, `P3`, ..., `P20` (note: `P2` is missing)
- `DeltaPct`: edit ratio as a fraction (e.g., `0.01`, `0.03`, `0.05`)
- `DeltaLabel`: string label for the edit ratio (e.g., `inc1`, `inc3`, `inc5`)
- `Turn`: update type, one of:
  - `ins` (insertion-only)
  - `del` (deletion-only)
- `Full_SEM_s`: SEM (semi-naive) time under **full recomputation** (seconds)
- `Inc_SEM_s`: SEM time under **incremental propagation** (seconds)
- `Speedup_x`: precomputed speedup (may be redundant; recompute to avoid rounding drift)
- `Improvement_pct`: precomputed percentage improvement (optional; not needed for our tables)

Graph-size columns for the **resulting derivation graph after the update** (full graph,
post-applyDelta, pre-prune):

- `GraphNodes`: number of nodes (atoms) in the full derivation graph after applyDelta
- `GraphEdges`: number of edges (grounded rule applications / derivation edges) in the full derivation graph after applyDelta

These are taken from the incremental log line:
`[inc-iter N] mode=... apply_delta_graph: totalNodes=... totalEdges=...`
and **do not** use pruned/view totals.

Delta columns (net changes between the old graph and the resulting updated graph):

- `DeltaInsertNodes`, `DeltaInsertEdges`: counts of inserted nodes/edges
- `DeltaDeleteNodes`, `DeltaDeleteEdges`: counts of deleted nodes/edges

Pre-prune applyDelta operation counts (symmetric between delete/insert turns):

- `ApplyDelTuples`, `ApplyDelRuleApps`, `ApplyDelFacts`
- `ApplyInsTuples`, `ApplyInsRuleApps`, `ApplyInsFacts`

These come from the `[inc-iter N] ... apply_delta_ops: ...` log line (after applyDelta,
before prune). Use these for SEM-stage delta proxies; they reflect full-graph changes,
not view/prune bookkeeping.

### A.3 Derived per-row quantities (recompute in the script)

For every row:

1) **Speedup** (always recompute):
\[
\text{speedup} := \frac{T_{\mathrm{full}}}{T_{\mathrm{inc}}}
= \frac{\texttt{Full\_SEM\_s}}{\texttt{Inc\_SEM\_s}}.
\]

2) **Net changed edge count** `ΔE` (SEM proxy; use applyDelta operation counts):
- if `Turn == "ins"`:  
  \[
  \Delta E := \texttt{ApplyInsRuleApps}.
  \]
- if `Turn == "del"`:  
  \[
  \Delta E := \texttt{ApplyDelRuleApps}.
  \]

3) **Resulting edge count** `|E|`:
\[
|E| := \texttt{GraphEdges}.
\]

4) **Workload proxy** `ΔE/|E|` (per instance, full graph):
\[
\Delta E/|E| := \frac{\Delta E}{|E|}.
\]

**Important interpretation note for the paper.**

- For **insertions**, `ΔE/|E|` is a reasonable coarse proxy for *incremental vs full work*:
  incremental processing is primarily driven by the newly enabled rule instances (`ΔE`),
  while full recomputation regenerates the entire updated edge set (`|E|`).

- For **deletions**, `ΔE/|E|` is still informative but can **underestimate** incremental work,
  because deletion maintenance may involve extra steps (e.g., temporary over-deletion and subsequent re-derivation,
  and cycle repair) that are not reflected in the **net** delta `ΔE`.

- `DeltaInsertNodes/DeltaDeleteNodes` and `DeltaInsertEdges/DeltaDeleteEdges` come from
  `apply_delta_view` (view/prune deltas) and can be asymmetric; they are useful for diagnosing
  pruning overhead, not for SEM delta proxies.

### A.4 Aggregation rules for Table 1 (summary table)

We group rows by:

- edit ratio `DeltaPct` in `{0.001, 0.003, 0.005}` (0.1/0.3/0.5% deltas) and
- update type `Turn` in `{ins, del}`.

For legacy 1/3/5% runs, use `{0.01, 0.03, 0.05}` instead.

For each group (Δ, Turn), compute:

1) **Avg. runtime** across cases (arithmetic means):
\[
\mathrm{Avg}(T_{\mathrm{full}}) := \mathrm{mean}_i(\texttt{Full\_SEM\_s}_i),\quad
\mathrm{Avg}(T_{\mathrm{inc}}) := \mathrm{mean}_i(\texttt{Inc\_SEM\_s}_i).
\]

2) **Avg. speedup** as a ratio of averages (NOT average of per-case ratios):
\[
\mathrm{AvgSpeedup} := \frac{\mathrm{Avg}(T_{\mathrm{full}})}{\mathrm{Avg}(T_{\mathrm{inc}})}.
\]

3) **Avg. ΔE/|E|** as a suite-level ratio (best aligned with “work proxy” meaning):
\[
\mathrm{Avg}\,\Delta E/|E| := \frac{\sum_i \Delta E_i}{\sum_i |E_i|}.
\]

This ratio answers: “across the whole suite, what fraction of the resulting derivation edges changed?”

Formatting:

- Report `Avg ΔE/|E|` as a **percentage** with **one decimal place** (e.g., `15.2\%`).
- Report `Avg T_full` and `Avg T_inc` in seconds with **two decimals**.
- Report `Avg speedup` with **two decimals** and a `\times` suffix.

### A.5 Aggregation rules for Table 2 (per-case speedup table)

Table 2 is per-case and shows speedups for each Δ and update type.

For each case:

- Extract the row for each `(DeltaPct, Turn)` pair.
- Output `speedup = Full_SEM_s / Inc_SEM_s` with **two decimals**.

Ordering:

- Sort cases by numeric index: `P1, P3, P4, ..., P20` (skip missing P2).

Optional final row:

- Add `GeoMean` speedup per column using:
\[
\mathrm{GeoMean} := \exp\Big(\frac{1}{n}\sum_i \ln(\text{speedup}_i)\Big).
\]
(Exclude missing cases; include all cases with valid times.)

---

## B. LaTeX formatting rules (for all tables in this file)

Use:

- `\usepackage{booktabs}`

Table style:

- Prefer narrow, single-column-friendly tables.
- Use `\small` (summary) and `\scriptsize` (per-case) if needed.
- Do not include thousands separators unless it improves readability.
- Keep captions **interpretive**: define each metric briefly, and include the key caveat that
  deletion work is underestimated by net `ΔE`.

---

## D. FC analysis plots (non-table artifacts)

These plots are used as supplemental analysis (not LaTeX tables). Keep paths and
data sources explicit in the README so results are reproducible.

Plotting checklist:
- Update the TSV first (from the latest experiment logs).
- Run the script from its folder, e.g. `python3 plot_fc_speedup_vs_delta_live_ratio.py`.
- If matplotlib cannot write its config cache, set `MPLCONFIGDIR=/tmp/mplconfig` before running.
- Verify the PNG path listed below is regenerated and matches the new TSV content.
- Commit PNG + TSV + script together.

- FC speedup vs `1/DeltaLiveRatio` (P12–P20, inc0p1/inc0p3/inc0p5):
  - Plot: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114/fc_speedup_vs_inv_delta_live_ratio.png`
  - Data: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114/fc_inv_delta_live_ratio.tsv`
  - Script: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114/plot_fc_speedup_vs_inv_delta_live_ratio.py`
  - Note: x-axis is `1/DeltaLiveRatio = LiveNodes / DeltaLiveNodes` computed from inc FC logs
    (`changed_node_count` / `live_nodes`); y-axis is FC speedup (Full / Inc).
- FC speedup vs `1/DeltaLiveRatio` (reuse-var-index run, P12–P20, inc0p1/inc0p3/inc0p5):
  - Plot: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114_reuse/fc_speedup_vs_inv_delta_live_ratio.png`
  - Data: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114_reuse/fc_inv_delta_live_ratio.tsv`
  - Script: `img/fc_speedup_vs_inv_delta_live_ratio_p12_p20_20260114_reuse/plot_fc_speedup_vs_inv_delta_live_ratio.py`
  - Note: reuse-var-index is enabled; use this plot to contrast with the baseline distribution above.
- FC speedup vs `DeltaLiveRatio` (reuse-var-index run, P12–P20, inc0p1/inc0p3/inc0p5):
  - Plot: `img/fc_speedup_vs_delta_live_ratio_p12_p20_20260114_reuse/fc_speedup_vs_delta_live_ratio.png`
  - Data: `img/fc_speedup_vs_delta_live_ratio_p12_p20_20260114_reuse/fc_delta_live_ratio.tsv`
  - Script: `img/fc_speedup_vs_delta_live_ratio_p12_p20_20260114_reuse/plot_fc_speedup_vs_delta_live_ratio.py`
  - Note: x-axis uses `DeltaLiveRatio = DeltaLiveNodes / LiveNodes` (smaller is better).

---

## C. Ready-to-use Codex prompts

### Prompt T1: SEM summary table (Avg ΔE/|E| + Avg times + Avg speedup)

Paste into Codex with `semnaive-comparison-with-graph.tsv` provided as input:

```text
You are given a TSV file `semnaive-comparison-with-graph.tsv` with columns:
Case, DeltaPct, Turn (ins/del), Full_SEM_s, Inc_SEM_s, GraphEdges,
DeltaInsertEdges, DeltaDeleteEdges, plus other columns.

Task: generate a LaTeX table (tabular + caption + label) using booktabs.

Compute per row:
- speedup = Full_SEM_s / Inc_SEM_s
- ΔE = DeltaInsertEdges if Turn=ins else DeltaDeleteEdges
- |E| = GraphEdges

Group by DeltaPct in {0.001,0.003,0.005} and Turn in {ins,del}. For each group output:
- Avg ΔE/|E| as (sum_i ΔE_i)/(sum_i |E_i|), formatted as a percentage with 1 decimal.
- Avg T_full = mean_i Full_SEM_s, 2 decimals.
- Avg T_inc  = mean_i Inc_SEM_s, 2 decimals.
- Avg speedup = Avg T_full / Avg T_inc, 2 decimals, with '×'.

Table columns (keep it narrow):
Δ (%), Update (Insert/Delete), Avg ΔE/|E|, Avg T_full (s), Avg T_inc (s), Avg spdup.

Use Δ (%) values 0.1,0.3,0.5 (not 0.001 etc).
Turn mapping: ins→Insert, del→Delete.
Sort rows by Δ then Update (Insert first, Delete second).
Caption must define ΔE/|E| and explain that for deletions it underestimates work
because net ΔE does not capture over-deletion and re-derivation.
Label: tab:semnaive-summary-avg.
```

### Prompt T2: Per-case SEM speedup table

```text
You are given `semnaive-comparison-with-graph.tsv`.

Task: generate a LaTeX table (booktabs) reporting per-case speedup for SEM stage.

For each row compute speedup = Full_SEM_s / Inc_SEM_s.
Create a wide per-case table with columns:
Case,
1% edits: Del, Ins,
3% edits: Del, Ins,
5% edits: Del, Ins.

Use two decimals in each cell.
Sort cases as P1, P3, P4, ..., P20 (skip missing P2).
Add a final row GeoMean that reports the geometric mean speedup per column.

Use \scriptsize and a small \tabcolsep (e.g., 4pt) if needed.
Caption: “Per-instance speedup of incremental SEM over full recomputation for deletion-only (Del) and insertion-only (Ins) updates.”
Label: tab:semnaive-percase-speedup.
```

### Prompt T3 (optional): Per-case ΔE/|E| percentage table

```text
You are given `semnaive-comparison-with-graph.tsv`.

Task: generate a LaTeX table that reports per-case workload proxy ΔE/|E| (%) for each Δ and update type.

For each row:
- ΔE = DeltaInsertEdges if Turn=ins else DeltaDeleteEdges
- |E| = GraphEdges
- frac = 100 * ΔE / |E|

Output columns:
Case,
1% edits: Del%, Ins%,
3% edits: Del%, Ins%,
5% edits: Del%, Ins%.

Format each cell with 1 decimal and a '\%'.
Sort cases as P1, P3..P20, skip P2.
Caption must note that frac is based on the resulting graph edge count |E|
and that deletion frac underestimates incremental work due to over-deletion/re-derivation.
Label: tab:semnaive-edge-frac.
```

---

## D. Checklist before committing a generated table

- [ ] Confirm `speedup = Full_SEM_s / Inc_SEM_s` is recomputed (do not trust `Speedup_x` blindly).
- [ ] Confirm `ΔE` uses the correct delta column (`DeltaInsertEdges` vs `DeltaDeleteEdges`) based on `Turn`.
- [ ] Confirm averages are arithmetic means, and `Avg ΔE/|E|` is computed as `sum ΔE / sum |E|`.
- [ ] Confirm case ordering and skipping P2 are correct.
- [ ] Use consistent rounding across all tables.

---

## E. Generated tables (run 2026-01-09)

<!-- BEGIN GENERATED TABLES -->

### Table 1: SEM summary (avg $\Delta E/|E|$, avg times, avg speedup)

\begin{table}[t]
\small
\centering
\begin{tabular}{llrrrr}
\toprule
$\Delta$ (\%) & Update & Avg $\Delta E/|E|$ & Avg $T_{\mathrm{full}}$ (s) & Avg $T_{\mathrm{inc}}$ (s) & Avg spdup \\
\midrule
1 & Insert & 5.9\% & 5.55 & 0.36 & 15.46\\
1 & Delete & 6.3\% & 4.98 & 2.74 & 1.82\\
3 & Insert & 15.2\% & 5.50 & 1.02 & 5.38\\
3 & Delete & 18.0\% & 4.44 & 3.60 & 1.23\\
5 & Insert & 23.8\% & 5.48 & 1.63 & 3.37\\
5 & Delete & 31.2\% & 3.98 & 4.13 & 0.96\\
\bottomrule
\end{tabular}
\caption{SEM summary: $\Delta E/|E|$ is computed as $(\sum_i \Delta E_i)/(\sum_i |E_i|)$; for deletions it underestimates work because net $\Delta E$ omits over-deletion and re-derivation.}
\label{tab:semnaive-summary-avg}
\end{table}

### Table 2: Per-case SEM speedup

\begin{table*}[t]
\scriptsize
\setlength{\tabcolsep}{4pt}
\centering
\begin{tabular}{lrrrrrr}
\toprule
Case & \multicolumn{2}{c}{1\% edits} & \multicolumn{2}{c}{3\% edits} & \multicolumn{2}{c}{5\% edits} \\
 & Del & Ins & Del & Ins & Del & Ins \\
\midrule
P1 & 0.34 & 1.66 & 0.20 & 1.14 & 0.24 & 1.36 \\
P3 & 0.50 & 1.54 & 0.54 & 2.07 & 0.26 & 1.35 \\
P4 & 0.96 & 1.02 & 1.01 & 2.08 & 0.78 & 0.62 \\
P5 & 0.19 & 0.12 & 0.26 & 0.22 & 0.14 & 0.16 \\
P6 & 1.07 & 1.25 & 0.51 & 1.03 & 0.60 & 1.52 \\
P7 & 2.77 & 6.40 & 2.78 & 4.06 & 2.51 & 3.79 \\
P8 & 18.21 & 19.65 & 10.39 & 9.21 & 3.46 & 6.51 \\
P9 & 2.38 & 7.56 & 1.98 & 4.05 & 1.68 & 3.86 \\
P10 & 2.72 & 11.57 & 3.71 & 8.17 & 2.86 & 5.21 \\
P11 & 4.04 & 12.51 & 3.18 & 7.27 & 2.90 & 5.79 \\
P12 & 10.21 & 17.89 & 2.92 & 5.88 & 1.48 & 3.11 \\
P13 & 7.75 & 20.42 & 3.28 & 8.25 & 1.67 & 4.58 \\
P14 & 7.92 & 23.10 & 2.23 & 8.30 & 1.49 & 3.85 \\
P15 & 4.13 & 16.17 & 1.71 & 4.87 & 1.12 & 2.99 \\
P16 & 2.26 & 16.77 & 1.57 & 5.62 & 1.14 & 3.06 \\
P17 & 1.95 & 16.36 & 1.41 & 5.19 & 1.13 & 3.33 \\
P18 & 1.56 & 15.91 & 1.19 & 4.76 & 0.96 & 3.16 \\
P19 & 1.19 & 13.55 & 1.04 & 5.18 & 0.87 & 3.37 \\
P20 & 2.63 & 18.09 & 1.06 & 6.69 & 0.82 & 4.09 \\
\midrule
GeoMean & 2.17 & 6.86 & 1.44 & 3.78 & 1.03 & 2.57 \\
\bottomrule
\end{tabular}
\caption{Per-instance speedup of incremental SEM over full recomputation for deletion-only (Del) and insertion-only (Ins) updates.}
\label{tab:semnaive-percase-speedup}
\end{table*}

### Table 3: Per-case $\Delta E/|E|$ (\%)

\begin{table*}[t]
\scriptsize
\setlength{\tabcolsep}{4pt}
\centering
\begin{tabular}{lrrrrrr}
\toprule
Case & \multicolumn{2}{c}{1\% edits} & \multicolumn{2}{c}{3\% edits} & \multicolumn{2}{c}{5\% edits} \\
 & Del\% & Ins\% & Del\% & Ins\% & Del\% & Ins\% \\
\midrule
P1 & 265.1\% & 72.6\% & 376.1\% & 79.0\% & 376.8\% & 79.0\% \\
P3 & 173.6\% & 63.5\% & 175.3\% & 63.7\% & 411.3\% & 80.4\% \\
P4 & 0.0\% & 0.0\% & 0.0\% & 0.0\% & 0.6\% & 0.6\% \\
P5 & 11.5\% & 10.3\% & 11.5\% & 10.3\% & 13.7\% & 12.1\% \\
P6 & 0.3\% & 0.3\% & 42.0\% & 29.6\% & 42.1\% & 29.6\% \\
P7 & 0.1\% & 0.1\% & 8.8\% & 8.0\% & 10.6\% & 9.6\% \\
P8 & 0.5\% & 0.5\% & 2.2\% & 2.1\% & 12.6\% & 11.2\% \\
P9 & 6.1\% & 5.8\% & 23.6\% & 19.1\% & 25.1\% & 20.1\% \\
P10 & 3.8\% & 3.7\% & 9.4\% & 8.6\% & 12.2\% & 10.9\% \\
P11 & 3.7\% & 3.6\% & 8.4\% & 7.7\% & 14.0\% & 12.3\% \\
P12 & 6.2\% & 5.8\% & 23.6\% & 19.1\% & 42.2\% & 29.7\% \\
P13 & 6.6\% & 6.2\% & 15.2\% & 13.2\% & 28.8\% & 22.4\% \\
P14 & 5.9\% & 5.6\% & 17.1\% & 14.6\% & 34.9\% & 25.9\% \\
P15 & 7.1\% & 6.6\% & 21.4\% & 17.6\% & 38.4\% & 27.7\% \\
P16 & 6.3\% & 5.9\% & 19.5\% & 16.3\% & 34.7\% & 25.8\% \\
P17 & 5.7\% & 5.4\% & 18.5\% & 15.6\% & 33.3\% & 25.0\% \\
P18 & 6.0\% & 5.7\% & 18.7\% & 15.7\% & 32.4\% & 24.5\% \\
P19 & 6.7\% & 6.3\% & 18.6\% & 15.7\% & 31.9\% & 24.2\% \\
P20 & 3.6\% & 3.5\% & 11.3\% & 10.2\% & 19.3\% & 16.1\% \\
\bottomrule
\end{tabular}
\caption{Per-instance workload proxy $\Delta E/|E|$ (\%) based on resulting graph edge count $|E|$. For deletions, the fraction underestimates work because net $\Delta E$ omits over-deletion and re-derivation.}
\label{tab:semnaive-edge-frac}
\end{table*}

<!-- END GENERATED TABLES -->
