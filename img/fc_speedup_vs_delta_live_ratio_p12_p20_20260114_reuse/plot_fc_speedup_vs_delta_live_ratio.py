#!/usr/bin/env python3
import csv
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parent
DATA = ROOT / "fc_delta_live_ratio.tsv"
OUT = ROOT / "fc_speedup_vs_delta_live_ratio.png"

rows = []
with DATA.open() as f:
    reader = csv.DictReader(f, delimiter="\t")
    for row in reader:
        rows.append(row)

xs_del = []
ys_del = []
xs_ins = []
ys_ins = []
for row in rows:
    ratio = float(row["DeltaLiveRatio"])
    speedup = float(row["Speedup"])
    if row["Turn"] == "del":
        xs_del.append(ratio)
        ys_del.append(speedup)
    else:
        xs_ins.append(ratio)
        ys_ins.append(speedup)

fig, ax = plt.subplots(figsize=(10, 7.5))
ax.scatter(xs_del, ys_del, c="#4e79a7", alpha=0.85, label="delete")
ax.scatter(xs_ins, ys_ins, c="#f28e2b", alpha=0.85, label="insert")

# Theoretical curve: speedup ~= 1 / DeltaLiveRatio
curve_x = np.linspace(0.001, 1.0, 500)
curve_y = 1.0 / curve_x
ax.plot(curve_x, curve_y, linestyle="--", color="#7f7f7f", label="theory 1/x")

ax.set_title("FC Speedup vs DeltaLiveRatio (P12-P20, inc0p1/inc0p3/inc0p5, reuse-var-index)")
ax.set_xlabel("DeltaLiveRatio (DeltaLiveNodes / LiveNodes)")
ax.set_ylabel("FC Speedup (Full / Inc)")
ax.set_xlim(0.0, 1.0)
if ys_del or ys_ins:
    max_speedup = max(ys_del + ys_ins)
    ax.set_ylim(0.0, max_speedup * 1.05)
ax.grid(True, alpha=0.25)
ax.legend()

fig.tight_layout()
fig.savefig(OUT, dpi=150)
print(f"Wrote {OUT}")
