#!/usr/bin/env python3
"""Fail if measured hard/soft knee transfer differs from its analytic curve."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("report", type=Path)
parser.add_argument("--tolerance-db", type=float, default=0.1)
args = parser.parse_args()
data = json.loads(args.report.read_text())
failures = []
for row in data["transfer"]:
    for curve in ("hard_knee", "soft_knee"):
        error = abs(row[f"{curve}_error_db"])
        if error > args.tolerance_db:
            failures.append((curve, row["input_dbfs"], error))
print(f"revision={data.get('source_revision', 'unknown')} module_sha256={data.get('plugin_module_sha256', 'unknown')}")
if failures:
    for curve, level, error in failures:
        print(f"FAIL {curve} at {level:g} dBFS: error {error:.6f} dB exceeds {args.tolerance_db:g} dB")
    raise SystemExit(1)
print(f"PASS transfer errors within {args.tolerance_db:g} dB")
