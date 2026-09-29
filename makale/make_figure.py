#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Publication figure: outcome matrix (12 problems x 4 tools) under the official
ProjDevBench time/memory limits. Every cell carries a text label (value, CE, MLE),
so colour is never the only channel. Rows are grouped by our complexity tier.
Data are read from ../tum-sonuclar.csv (no hand-copied numbers)."""
import csv, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle

HERE = os.path.dirname(os.path.abspath(__file__))
ROWS = list(csv.DictReader(open(os.path.join(HERE, "..", "tum-sonuclar.csv"), encoding="utf-8")))
R = {(r["problem"], r["arac"]): r for r in ROWS}

TOOLS = ["claude-code", "cursor", "codex", "antigravity"]
TLAB = ["Claude Code\n(Opus 4.8)", "Cursor\n(Opus 4.8)", "Codex\n(GPT-5.4)", "Antigravity\n(Gemini 3.1 Pro)"]
# order and tiers exactly as in Table 2 of the paper
TIERS = [("Low", [("001", "A+B"), ("009", "vector"), ("020", "Buddy alloc.")]),
         ("Medium", [("005", "QOI codec"), ("002", "int2048"), ("013", "map")]),
         ("High", [("006", "Minesweeper"), ("014", "Python interp."), ("018", "Scheme interp."),
                   ("019", "Attention sim."), ("004", "Bookstore"), ("017", "Train ticket")])]

# status palette (fixed roles); pass = recessive neutral so failures stand out
PASS, PARTIAL, MLE, CE = "#eeede9", "#fab219", "#ec835a", "#d03b3b"
INK, MUTED = "#222222", "#8a8a86"


def num(x):
    try: return float(x)
    except (TypeError, ValueError): return None


def cell(p, t):
    """(fill, label, label_colour) for one run."""
    r = R[p, t]
    e, el = num(r["exec_score"]), num(r["exec_lim"])
    if r["binary_kb"] in ("-", "") and not e:
        return CE, "CE", "white"
    if e is not None and e < 1:          # builds, but crashes almost immediately (017 Antigravity: segfault)
        return CE, "RE", "white"
    if el is not None and e is not None and el < e and int(r["mle"] or 0) > 0:
        return MLE, "MLE", INK
    if el is not None and el < e:
        return MLE, "TLE", INK
    if e is not None and e < 100:
        return PARTIAL, f"{e:.0f}" if e.is_integer() else f"{e:.1f}", INK
    return PASS, "100", MUTED


plt.rcParams.update({"font.family": "serif", "font.size": 9})
nrow = sum(len(ps) for _, ps in TIERS)
W = 1.3   # cell width (x units); row height = 1
fig, ax = plt.subplots(figsize=(6.2, 5.5))
ax.set_xlim(-2.55, len(TOOLS) * W)
ax.set_ylim(nrow + 2.35, -0.95)
ax.axis("off")

GAP = 0.06  # thin surface gap between cells
y = 0
for tier, probs in TIERS:
    top = y
    for pid, name in probs:
        ax.text(-0.1, y + 0.5, f"{pid}  {name}", ha="right", va="center", fontsize=8.5, color=INK)
        for j, t in enumerate(TOOLS):
            fill, lab, col = cell(pid, t)
            ax.add_patch(Rectangle((j * W + GAP, y + GAP), W - 2 * GAP, 1 - 2 * GAP,
                                   facecolor=fill, edgecolor="none"))
            ax.text(j * W + W / 2, y + 0.5, lab, ha="center", va="center", fontsize=8.5, color=col,
                    fontweight="bold" if fill != PASS else "normal")
        y += 1
    # tier bracket on the far left
    ax.plot([-2.4, -2.4], [top + 0.12, y - 0.12], color=MUTED, lw=0.9)
    ax.text(-2.47, (top + y) / 2, tier, rotation=90, ha="right", va="center", fontsize=8.5, color=MUTED)
    if y < nrow:
        ax.plot([-2.3, len(TOOLS) * W], [y, y], color="white", lw=2.5)

for j, lab in enumerate(TLAB):
    ax.text(j * W + W / 2, -0.12, lab, ha="center", va="bottom", fontsize=8.5, color=INK)

# legend (identity never by colour alone: each swatch repeats the in-cell label)
leg = [(PASS, "100", "all tests pass"), (PARTIAL, "50", "partial (% passed)"),
       (MLE, "MLE", "memory limit exceeded"), (CE, "CE", "compile/link error"),
       (CE, "RE", "runtime crash")]
for k, (fill, lab, text) in enumerate(leg):
    lx = 0.0 + (k % 2) * 2.6
    ly = nrow + 0.35 + (k // 2) * 0.62
    ax.add_patch(Rectangle((lx, ly), 0.5, 0.48, facecolor=fill, edgecolor="none"))
    ax.text(lx + 0.25, ly + 0.25, lab, ha="center", va="center", fontsize=6.5,
            color="white" if fill == CE else (MUTED if fill == PASS else INK))
    ax.text(lx + 0.62, ly + 0.25, text, ha="left", va="center", fontsize=8, color=INK)

fig.tight_layout()
fig.savefig(os.path.join(HERE, "fig_outcomes.pdf"), bbox_inches="tight")
fig.savefig(os.path.join(HERE, "fig_outcomes.png"), dpi=200, bbox_inches="tight")
print("OK -> fig_outcomes.pdf + fig_outcomes.png")
