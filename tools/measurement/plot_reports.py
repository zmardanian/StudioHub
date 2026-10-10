#!/usr/bin/env python3
"""Render transfer and machine-specific benchmark charts from retained JSON."""
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

root = Path(__file__).resolve().parent
measurements = json.loads((root / "current-baseline-verified-v2/measurements.json").read_text())
rows = measurements["transfer"]
levels = [row["input_dbfs"] for row in rows]
fig, ax = plt.subplots(figsize=(8.2, 4.8), layout="constrained")
for label, actual, expected, marker, color in (
    ("Hard knee measured", "hard_knee_output_gr_db", "hard_knee_expected_gr_db", "o", "#d96649"),
    ("12 dB soft knee measured", "soft_knee_output_gr_db", "soft_knee_expected_gr_db", "s", "#3e9c84"),
):
    ax.plot(levels, [r[actual] for r in rows], marker=marker, color=color, label=label)
ax.plot(levels, [r["hard_knee_expected_gr_db"] for r in rows], "--", color="#d96649", alpha=.7, label="Hard knee analytical")
ax.plot(levels, [r["soft_knee_expected_gr_db"] for r in rows], "--", color="#3e9c84", alpha=.7, label="Soft knee analytical")
ax.set(title="StudioComp FET steady-state gain reduction", xlabel="Input level (dBFS peak)", ylabel="Gain reduction (dB)")
ax.grid(True, alpha=.25)
ax.legend(frameon=False, fontsize=8)
fig.savefig(root / "transfer-curve.png", dpi=160)

bench = json.loads((root / "performance.json").read_text())
trials = bench["trials"]
fig, ax = plt.subplots(figsize=(7.8, 3.8), layout="constrained")
ax.plot([r["trial"] for r in trials], [r["seconds_for_10s_stereo_48k"] * 1000 for r in trials], "o-", color="#4a8fc2")
ax.axhline(bench["seconds_mean"] * 1000, linestyle="--", color="#d18d48", label=f"Mean {bench['seconds_mean']*1000:.3f} ms")
ax.set(title="StudioComp FET processing benchmark (10 trials)", xlabel="Trial", ylabel="Wall time (ms per 10 s stereo block)")
ax.grid(True, alpha=.25)
ax.legend(frameon=False)
fig.savefig(root / "performance-trials.png", dpi=160)
print("wrote transfer-curve.png and performance-trials.png")
