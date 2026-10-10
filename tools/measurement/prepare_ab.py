#!/usr/bin/env python3
"""Make RMS/LUFS-matched, peak-safe listening copies of synthetic FET fixtures."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

import numpy as np
from scipy.io import wavfile


def ebu_loudness(path: Path) -> float:
    result = subprocess.run(
        ["ffmpeg", "-hide_banner", "-nostats", "-i", str(path), "-filter_complex",
         "ebur128=framelog=quiet", "-f", "null", "-"],
        text=True, capture_output=True, check=True,
    )
    matches = re.findall(r"Integrated loudness:\s*I:\s*(-?[0-9.]+) LUFS", result.stderr)
    if not matches:
        raise RuntimeError(f"ffmpeg did not report integrated loudness for {path}")
    return float(matches[-1])


def read_float(path: Path) -> tuple[int, np.ndarray]:
    rate, data = wavfile.read(path)
    if data.dtype == np.int16:
        values = data.astype(np.float64) / 32768.0
    elif data.dtype == np.int32:
        values = data.astype(np.float64) / 2147483648.0
    elif data.dtype.kind == "f":
        values = data.astype(np.float64)
    else:
        raise ValueError(f"unsupported WAV format: {data.dtype}")
    if values.ndim == 1:
        values = values[:, None]
    return rate, values


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    fixture_reports = []
    for name in ("vocal-like", "percussion", "bass", "full-mix"):
        versions = {label: args.fixtures / f"{name}-{suffix}.wav" for label, suffix in
                    (("dry", "dry"), ("hard-knee", "hard-knee"), ("soft-knee", "soft-knee"))}
        source = {key: read_float(path) for key, path in versions.items()}
        rates = {rate for rate, _ in source.values()}
        lengths = {array.shape for _, array in source.values()}
        if len(rates) != 1 or len(lengths) != 1:
            raise ValueError(f"unaligned fixture renders for {name}")
        loudness = {key: ebu_loudness(path) for key, path in versions.items()}
        # Keep at least 2 dB sample-peak headroom and use the quietest safe common LUFS.
        peak_safe_targets = {
            key: loudness[key] + (-2.0 - 20.0 * np.log10(np.max(np.abs(data))))
            for key, (_, data) in source.items()
        }
        target_lufs = min(min(loudness.values()), min(peak_safe_targets.values()))
        rows = []
        for key, path in versions.items():
            rate, data = source[key]
            gain_db = target_lufs - loudness[key]
            scaled = data * (10.0 ** (gain_db / 20.0))
            peak = float(np.max(np.abs(scaled)))
            if peak >= 1.0:
                raise RuntimeError(f"peak protection failed for {path}: {peak}")
            pcm = np.rint(np.clip(scaled, -1.0, 1.0) * 2147483647.0).astype(np.int32)
            output_path = args.output / f"{name}-{key}-matched.wav"
            wavfile.write(output_path, rate, pcm)
            measured_lufs = ebu_loudness(output_path)
            rows.append({
                "version": key,
                "source": str(path),
                "output": str(output_path),
                "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "output_sha256": hashlib.sha256(output_path.read_bytes()).hexdigest(),
                "source_integrated_lufs": loudness[key],
                "gain_db": float(gain_db),
                "matched_integrated_lufs": measured_lufs,
                "sample_peak_dbfs": float(20.0 * np.log10(peak)),
            })
        errors = [abs(row["matched_integrated_lufs"] - target_lufs) for row in rows]
        fixture_reports.append({
            "fixture": name,
            "sample_rate_hz": next(iter(rates)),
            "frames": next(iter(lengths))[0],
            "target_integrated_lufs": target_lufs,
            "max_loudness_match_error_db": max(errors),
            "peak_ceiling_dbfs": -2.0,
            "versions": rows,
        })
    manifest = {
        "method": "FFmpeg EBU R128 integrated loudness matching; linear gain only; 2 dB sample-peak ceiling",
        "source_material": "deterministic synthetic fixtures; no commercial or user recording used",
        "sample_format": "32-bit PCM WAV",
        "human_listening_required": True,
        "human_listening_status": "not evaluated",
        "fixtures": fixture_reports,
    }
    (args.output / "ab-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({f["fixture"]: {"target_lufs": f["target_integrated_lufs"],
                                      "max_error_db": f["max_loudness_match_error_db"]}
                      for f in fixture_reports}, indent=2))


if __name__ == "__main__":
    main()
