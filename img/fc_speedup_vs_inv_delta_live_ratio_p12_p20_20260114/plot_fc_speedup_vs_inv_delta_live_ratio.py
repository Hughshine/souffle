from __future__ import annotations

import csv
import os
from pathlib import Path

# Avoid writing to ~/.config/matplotlib on systems without permissions.
os.environ.setdefault("MPLCONFIGDIR", str(Path(__file__).parent / ".mplconfig"))

import matplotlib.pyplot as plt

BASE_DIR = Path(__file__).parent
INPUT_TSV = BASE_DIR / "fc_inv_delta_live_ratio.tsv"
OUTPUT_PNG = BASE_DIR / "fc_speedup_vs_inv_delta_live_ratio.png"


def load_points() -> list[tuple[float, float, str]]:
    points: list[tuple[float, float, str]] = []
    with INPUT_TSV.open() as f:
        reader = csv.DictReader(f, delimiter="\t")
        for row in reader:
            ratio = float(row["DeltaLiveRatio"])
            speed = float(row["FC_Speedup"])
            points.append((ratio, speed, row["Turn"]))
    return points


def plot_points(points: list[tuple[float, float, str]]) -> None:
    fig, ax = plt.subplots(figsize=(6.4, 4.8))
    colors = {"del": "tab:blue", "ins": "tab:orange"}
    labels = {"del": "delete", "ins": "insert"}

    for turn in ["del", "ins"]:
        tx = [p[0] for p in points if p[2] == turn]
        ty = [p[1] for p in points if p[2] == turn]
        if tx:
            ax.scatter(tx, ty, s=24, alpha=0.75, label=labels[turn], color=colors[turn])

    ax.set_xlim(0.0, 1.0)

    # Plot y = 1/x within the visible axis overlap to show the ideal ratio curve.
    x_min, x_max = ax.get_xlim()
    y_min, y_max = ax.get_ylim()
    if y_max > 0:
        line_min = max(x_min, 1.0 / y_max, 1e-6)
        line_max = x_max if y_min <= 0 else min(x_max, 1.0 / y_min)
        if line_min < line_max:
            steps = 200
            xs = [line_min + i * (line_max - line_min) / steps for i in range(steps + 1)]
            ys = [1.0 / x for x in xs]
            ax.plot(xs, ys, color="gray", lw=1.3, ls="--", label="y = 1/x")

    ax.set_xlabel("DeltaLiveRatio (DeltaLiveNodes / LiveNodes)")
    ax.set_ylabel("FC Speedup (Full / Inc)")
    ax.set_title("FC Speedup vs DeltaLiveRatio (P12–P20, inc0p1/inc0p3/inc0p5)")
    ax.grid(True, alpha=0.2)
    ax.legend(loc="upper right", fontsize=8)
    fig.tight_layout()
    fig.savefig(OUTPUT_PNG, dpi=160)


def main() -> None:
    points = load_points()
    plot_points(points)


if __name__ == "__main__":
    main()
