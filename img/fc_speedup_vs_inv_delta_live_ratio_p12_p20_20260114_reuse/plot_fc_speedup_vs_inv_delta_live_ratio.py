#!/usr/bin/env python3
import csv
from pathlib import Path
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent
DATA = ROOT / "fc_inv_delta_live_ratio.tsv"
OUT = ROOT / "fc_speedup_vs_inv_delta_live_ratio.png"

rows = []
with DATA.open() as f:
    reader = csv.DictReader(f, delimiter="\t")
    for row in reader:
        rows.append(row)

xs = []
ys = []
labels = []
for row in rows:
    inv_ratio = float(row["InvDeltaLiveRatio"])
    speedup = float(row["Speedup"])
    xs.append(inv_ratio)
    ys.append(speedup)
    labels.append(row.get("Turn", "ins"))

fig, ax = plt.subplots(figsize=(10, 7.5))
ax.scatter(xs, ys, c="#f28e2b", alpha=0.85, label="insert")

# x=y reference line
line_min = min(xs + ys)
line_max = max(xs + ys)
ax.plot([line_min, line_max], [line_min, line_max], color="gray", lw=1.3, ls="--", label="x = y")

ax.set_title("FC Speedup vs 1/DeltaLiveRatio (P12-P20, inc0p1/inc0p3/inc0p5, reuse-var-index)")
ax.set_xlabel("1 / DeltaLiveRatio (LiveNodes / DeltaLiveNodes)")
ax.set_ylabel("FC Speedup (Full / Inc)")
ax.grid(True, alpha=0.25)
ax.legend()

fig.tight_layout()
fig.savefig(OUT, dpi=150)
print(f"Wrote {OUT}")
