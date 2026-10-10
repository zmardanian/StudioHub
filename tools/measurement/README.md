# Measurement Tools

`fet_lab.cpp` is the deterministic C++ renderer and measurement program used by
the StudioComp FET project. Build it with `-DSTUDIOHUB_BUILD_TOOLS=ON`; output
contains transfer, envelope, timing, and artifact/source identity data.

The companion scripts support repeatable report checking, plotting, and
loudness-matched listening preparation. `prepare_ab.py` requires NumPy, SciPy,
FFmpeg, and `ffmpeg-normalize`/EBU R128 support as documented by its help text.
Generated WAVs and reports should be written to a local ignored output folder.

`analyze_harmonics.py` measures coherent harmonic levels from the existing dry,
hard-knee and soft-knee PCM16 fixtures. It uses a one-second Goertzel analysis
window and reports per-channel harmonics, THD, fixture hashes, source revision
and plugin-module hash without external Python packages.

```sh
python3 tools/measurement/analyze_harmonics.py path/to/local/measurements.json
```

This produces `spectral-analysis.json` beside the transfer report. The output
is a descriptive offline DSP render measurement, not an acceptance threshold or
a VST3 host render. It does not measure intermodulation or alias energy.

Future analyzers may include:

studiohub-sweep
studiohub-thd
studiohub-imd
studiohub-aliasing
studiohub-frequency-response
studiohub-latency
studiohub-benchmark
studiohub-render

Output should support JSON/CSV plus optional WAV artifacts.
