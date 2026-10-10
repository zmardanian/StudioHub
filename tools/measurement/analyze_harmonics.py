#!/usr/bin/env python3
"""Measure coherent harmonic levels in the deterministic FET lab WAV fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import wave
from pathlib import Path


def read_pcm16(path: Path) -> tuple[int, list[list[float]], bytes]:
    raw_file = path.read_bytes()
    with wave.open(str(path), "rb") as wav:
        if wav.getcomptype() != "NONE" or wav.getsampwidth() != 2:
            raise ValueError(f"expected uncompressed PCM16 WAV: {path}")
        channels = wav.getnchannels()
        frames = wav.getnframes()
        sample_rate = wav.getframerate()
        raw = wav.readframes(frames)
    interleaved = struct.unpack("<" + "h" * (len(raw) // 2), raw)
    separated = [
        [interleaved[i * channels + channel] / 32768.0 for i in range(frames)]
        for channel in range(channels)
    ]
    return sample_rate, separated, raw_file


def coherent_amplitude(samples: list[float], bin_index: int) -> float:
    """Return peak amplitude at an integer DFT bin using the Goertzel recurrence."""
    count = len(samples)
    omega = 2.0 * math.pi * bin_index / count
    coefficient = 2.0 * math.cos(omega)
    previous = 0.0
    previous2 = 0.0
    mean = sum(samples) / count
    for value in samples:
        current = (value - mean) + coefficient * previous - previous2
        previous2 = previous
        previous = current
    power = max(0.0, previous * previous + previous2 * previous2 - coefficient * previous * previous2)
    return 2.0 * math.sqrt(power) / count


def measure_fixture(path: Path, sample_rate: int, frequency: float) -> dict:
    wav_rate, channels, raw = read_pcm16(path)
    if wav_rate != sample_rate:
        raise ValueError(f"sample-rate mismatch for {path}: {wav_rate} != {sample_rate}")
    if len(channels[0]) < sample_rate:
        raise ValueError(f"need at least one second of audio in {path}")

    # The lab uses integer-Hz tones. A one-second tail contains an integer number
    # of cycles, avoiding leakage from a non-coherent rectangular window.
    start = len(channels[0]) - sample_rate
    window = [channel[start:] for channel in channels]
    fundamental_bin = round(frequency)
    fundamental_frequency = fundamental_bin * sample_rate / sample_rate
    if abs(fundamental_frequency - frequency) > 1e-9:
        raise ValueError("frequency must be coherent with a one-second analysis window")

    amplitudes = []
    for channel in window:
        fundamental = coherent_amplitude(channel, fundamental_bin)
        harmonics = []
        harmonic_power = 0.0
        for order in range(1, int((sample_rate / 2) // frequency) + 1):
            bin_index = round(order * frequency)
            amplitude = coherent_amplitude(channel, bin_index)
            if order > 1:
                harmonic_power += amplitude * amplitude
            harmonics.append({
                "order": order,
                "frequency_hz": bin_index * sample_rate / len(channel),
                "amplitude_peak": amplitude,
                "level_dbfs": 20.0 * math.log10(max(amplitude, 1e-15)),
                "relative_to_fundamental_db": 20.0 * math.log10(
                    max(amplitude, 1e-15) / max(fundamental, 1e-15)
                ),
            })
        thd = math.sqrt(harmonic_power) / max(fundamental, 1e-15)
        amplitudes.append({
            "fundamental_peak": fundamental,
            "thd_ratio": thd,
            "thd_percent": thd * 100.0,
            "thd_db": 20.0 * math.log10(max(thd, 1e-15)),
            "harmonics": harmonics,
        })

    max_stereo_delta = 0.0
    if len(window) >= 2:
        max_stereo_delta = max(
            abs(left - right)
            for left, right in zip(window[0], window[1], strict=True)
        )
    return {
        "fixture": path.name,
        "fixture_sha256": hashlib.sha256(raw).hexdigest(),
        "channels": len(channels),
        "frames_analyzed_per_channel": sample_rate,
        "analysis_start_frame": start,
        "sample_rate_hz": sample_rate,
        "fundamental_hz": frequency,
        "stereo_max_abs_delta": max_stereo_delta,
        "channels_analysis": amplitudes,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("measurements_json", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    metadata = json.loads(args.measurements_json.read_text(encoding="utf-8"))
    sample_rate = int(metadata["sample_rate_hz"])
    frequency = float(metadata["input_frequency_hz"])
    root = args.measurements_json.parent
    fixture_names = ("dry-0dB.wav", "hard-knee-0dB.wav", "soft-knee-0dB.wav")
    report = {
        "schema_version": 1,
        "analysis_origin": "offline DSP WAV renders; not VST3 host renders",
        "plugin": metadata.get("plugin"),
        "source_revision": metadata.get("source_revision"),
        "plugin_module_sha256": metadata.get("plugin_module_sha256"),
        "sample_rate_hz": sample_rate,
        "frequency_hz": frequency,
        "window_seconds": 1,
        "window_selection": "last second of each fixture; coherent rectangular window",
        "settings": metadata.get("settings", {}),
        "fixtures": [
            measure_fixture(root / name, sample_rate, frequency)
            for name in fixture_names
        ],
        "interpretation": "Descriptive measurements only; no tonal-quality threshold is asserted.",
        "human_listening_required": True,
    }
    destination = args.output or root / "spectral-analysis.json"
    destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(destination)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, json.JSONDecodeError, wave.Error) as error:
        raise SystemExit(f"harmonic analysis failed: {error}")
