import numpy as np
import matplotlib.pyplot as plt

# Data
labels = ['Deriv', 'BDD', 'WMC']
full = np.array([
    [1.0994, 21.8431, 0.1582],
    [1.0915, 19.2189, 0.1439],
    [1.1220, 18.4647, 0.1460],
    [1.2055, 18.9651, 0.1526],
    [1.2983, 18.2364, 0.1603],
])

inc = np.array([
    [0.1836, 0.1458, 0.0198],
    [0.2653, 0.4850, 0.0072],
    [0.2713, 0.4883, 0.0073],
    [0.2948, 0.5600, 0.0126],
    [0.1981, 0.4477, 0.0424],
])

full_means = full.mean(axis=0)
inc_means = inc.mean(axis=0)

colors = ["#4C78A8", "#F58518", "#54A24B"]

def make_autopct(values, labels, skip_labels):
    total = float(np.sum(values))
    idx = {"value": 0}

    def _fmt(pct):
        i = idx["value"]
        idx["value"] += 1
        if labels[i] in skip_labels:
            return ""
        percent = values[i] / total * 100.0
        return f"{percent:.1f}%"

    return _fmt


def add_outer_label(ax, wedge, label, value, total):
    angle = (wedge.theta2 + wedge.theta1) / 2.0
    x = np.cos(np.deg2rad(angle))
    y = np.sin(np.deg2rad(angle))
    ha = "left" if x >= 0 else "right"
    percent = value / total * 100.0
    ax.annotate(
        f"{label} {percent:.1f}%",
        xy=(x * 0.9, y * 0.9),
        xytext=(x * 1.25, y * 1.25),
        ha=ha,
        va="center",
        fontsize=15,
        arrowprops={
            "arrowstyle": "-",
            "color": "#666666",
            "lw": 1.0,
        },
    )

# # No highlighting
explode = [0, 0, 0]

# Create figure (two side-by-side pies)
fig, axes = plt.subplots(1, 2, figsize=(8.6, 4.4))

wedges_full, texts_full, autotexts_full = axes[0].pie(
    full_means,
    explode=explode,
    startangle=90,
    counterclock=False,
    colors=colors,
    autopct=make_autopct(full_means, labels, {"WMC", "Deriv"}),
    pctdistance=0.6,
    textprops={'fontsize': 16},
    wedgeprops={'linewidth': 1, 'edgecolor': 'white'}
)

wmc_idx = labels.index("WMC")
deriv_idx = labels.index("Deriv")
add_outer_label(
    axes[0],
    wedges_full[deriv_idx],
    labels[deriv_idx],
    full_means[deriv_idx],
    float(np.sum(full_means))
)
add_outer_label(
    axes[0],
    wedges_full[wmc_idx],
    labels[wmc_idx],
    full_means[wmc_idx],
    float(np.sum(full_means))
)
axes[0].set_title("Full", fontsize=18, pad=14)

wedges_inc, texts_inc, autotexts_inc = axes[1].pie(
    inc_means,
    explode=explode,
    startangle=90,
    counterclock=False,
    colors=colors,
    autopct=make_autopct(inc_means, labels, {"WMC"}),
    pctdistance=0.6,
    textprops={'fontsize': 16},
    wedgeprops={'linewidth': 1, 'edgecolor': 'white'}
)

add_outer_label(
    axes[1],
    wedges_inc[wmc_idx],
    labels[wmc_idx],
    inc_means[wmc_idx],
    float(np.sum(inc_means))
)
axes[1].set_title("Incremental", fontsize=18, pad=14)

# Legend (shared)
fig.legend(
    wedges_full,
    labels,
    loc="lower center",
    bbox_to_anchor=(0.5, 0.06),
    ncol=3,
    fontsize=15,
    frameon=False
)

for ax in axes:
    ax.axis('equal')  # keep pies circular

# Adjust layout to keep the plot compact and make room for legend
plt.subplots_adjust(bottom=0.12, wspace=0.45)

# Save locally (no clipping)
plt.savefig(
    "runtime_breakdown_pie.png",
    dpi=300,
    bbox_inches="tight",
    pad_inches=0.05
)
plt.savefig(
    "runtime_breakdown_pie.pdf",
    bbox_inches="tight",
    pad_inches=0.05
)

plt.show()
