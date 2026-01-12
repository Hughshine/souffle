from __future__ import annotations

import csv
import os
from pathlib import Path

# Avoid writing to ~/.config/matplotlib on systems without permissions.
os.environ.setdefault("MPLCONFIGDIR", str(Path(__file__).parent / ".mplconfig"))

import matplotlib.pyplot as plt

BASE_DIR = Path(__file__).parent
INPUT_TSV = BASE_DIR / "semnaive-sem-delta-ratio.tsv"
OUTPUT_PNG = BASE_DIR / "sem_speedup_vs_delta_ratio_filtered_50ms_by_turn_with_ideal_nofit.png"
OUTPUT_SUMMARY = BASE_DIR / "sem_speedup_vs_delta_ratio_filtered_50ms_summary.tsv"

FILTER_THRESHOLD_S = 0.05


def load_points() -> list[tuple[float, float, str, float, float]]:
    points: list[tuple[float, float, str, float, float]] = []
    with INPUT_TSV.open() as f:
        reader = csv.DictReader(f, delimiter="\t")
        for row in reader:
            ratio = float(row["DeltaEdgeRatio"])
            speed = float(row["Speedup"])
            full = float(row["Full_SEM_s"])
            inc = float(row["Inc_SEM_s"])
            if full < FILTER_THRESHOLD_S and inc < FILTER_THRESHOLD_S:
                continue
            points.append((ratio, speed, row["Turn"], full, inc))
    return points


def write_summary(points: list[tuple[float, float, str, float, float]]) -> None:
    bins = [0.0, 0.05, 0.1, 0.3, 1.0]
    labels = ["[0,0.05)", "[0.05,0.1)", "[0.1,0.3)", "[0.3,1.0]"]

    xs = [p[0] for p in points]
    ys = [p[1] for p in points]

    rows = []
    for b0, b1, label in zip(bins[:-1], bins[1:], labels):
        items = [
            (x, y)
            for x, y in zip(xs, ys)
            if x >= b0 and (x < b1 or (b1 == bins[-1] and x <= b1))
        ]
        if not items:
            rows.append((label, 0, 0.0, 0.0, 0.0))
            continue
        ratios = sorted(x for x, _ in items)
        speeds = sorted(y for _, y in items)
        count = len(speeds)
        mid = count // 2
        median = speeds[mid] if count % 2 else (speeds[mid - 1] + speeds[mid]) / 2.0
        rows.append((label, count, sum(ratios) / count, sum(speeds) / count, median))

    with OUTPUT_SUMMARY.open("w", encoding="utf-8") as f:
        f.write("\t".join([
            "DeltaRatioBin",
            "Count",
            "AvgDeltaEdgeRatio",
            "AvgSpeedup",
            "MedianSpeedup",
        ]) + "\n")
        for label, count, avg_ratio, avg_speed, median_speed in rows:
            f.write(
                f"{label}\t{count}\t{avg_ratio:.4f}\t{avg_speed:.3f}\t{median_speed:.3f}\n"
            )


def plot_points(points: list[tuple[float, float, str, float, float]]) -> None:
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]

    fig, ax = plt.subplots(figsize=(6.4, 4.8))
    colors = {"del": "tab:blue", "ins": "tab:orange"}
    labels = {"del": "delete", "ins": "insert"}

    for turn in ["del", "ins"]:
        tx = [p[0] for p in points if p[2] == turn]
        ty = [p[1] for p in points if p[2] == turn]
        if tx:
            ax.scatter(tx, ty, s=24, alpha=0.75, label=labels[turn], color=colors[turn])

    if xs:
        y_cap = max(ys) * 1.1
        ideal_x = [x for x in sorted(set(xs)) if x > 0]
        ideal_y = [min(1.0 / x, y_cap) for x in ideal_x]
        ax.plot(ideal_x, ideal_y, color="gray", lw=1.3, ls="--", label="ideal: 1/(ΔE/|E|) (clipped)")

    ax.set_xlabel("DeltaEdgeRatio (DeltaE / |E|)")
    ax.set_ylabel("SEM Speedup (Full / Inc)")
    ax.set_title("SEM Speedup vs DeltaE/|E| (>=0.05s, by turn + ideal)")
    ax.grid(True, alpha=0.2)
    ax.legend(loc="upper right", fontsize=8)
    fig.tight_layout()
    fig.savefig(OUTPUT_PNG, dpi=160)


def main() -> None:
    points = load_points()
    write_summary(points)
    plot_points(points)


if __name__ == "__main__":
    main()
