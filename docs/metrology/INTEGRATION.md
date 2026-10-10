# StudioHub Metrology Integration

StudioHub 1.0.3 integrates the Metrology & Calibration framework as the native `StudioHubMetrology` CMake target.

## Architecture

- `StudioHubDSP`: DSP primitives and analysis signal generation.
- `StudioHubMetrology`: calibration evidence, uncertainty budgets, reference-signal specifications, measurement provenance, traceability, and inter-laboratory comparison.
- The metrology layer does not own plugin formats, GUI code, or real-time DSP processing.

## Calibration boundary

The software can validate and record calibration evidence and refuse expired or malformed calibration. It cannot create physical calibration or accreditation. Physical traceability requires actual calibrated instruments, certificates, competent calibration laboratories, and an externally assessed accreditation scope where applicable.

## Fresh build

```bash
./scripts/doctor.sh
./scripts/build-and-test.sh
```

For an install prefix:

```bash
cmake -S . -B build -G Ninja -DSTUDIOHUB_WARNINGS_AS_ERRORS=ON
cmake --build build -j"$(nproc)"
cmake --install build --prefix "$HOME/.local"
```
