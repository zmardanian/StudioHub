# StudioHub 1.0.3 — Foundation Hardening

This maintenance release hardens the integrated 1.0.3 foundation before production DSP development.

## Fixed and strengthened

- Repaired `scripts/doctor.sh` root-variable failure under `set -u`.
- Doctor now resolves its own repository root, verifies the version, and fails clearly on missing tools/files.
- `DspCore` now fails closed to 48 kHz for invalid sample rates and sanitizes non-finite input.
- `AudioBuffer` now returns empty spans for invalid/null/out-of-range channel access.
- Detector now implements attack/release envelope behavior and a true RMS-domain detector.
- Added allocation-free streaming FIR oversampling for x2/x4/x8/x16 factors with complete high-rate decimation, stronger stopband filtering, state reset, and filter redesign on factor changes.
- Added nonlinear alias-regression coverage and adversarial x16 sanitizer probing.
- Traceability evidence is now required to be structurally valid and current at assessment time.
- Measurement records now fail closed without instrument, calibration certificate, and traceability metadata.
- Date validation no longer relies on exception-capable parsing inside noexcept paths.
- Added DSP tests for buffer safety, detector behavior, oversampling, invalid-rate handling, and non-finite input.
- Traceability assessment now rejects duplicate nodes, root-reference mismatches, and missing evidence IDs.
- Installation exports StudioHub targets and installs DSP/analysis/engine headers.
- Added a relocatable CMake package configuration for downstream `find_package(StudioHub CONFIG)` consumers.

## Verification performed in the packaging environment

- Structural check PASS.
- Strict warnings-as-errors production-library build PASS.
- Install-prefix verification PASS.
- ASan/UBSan standalone DSP component check PASS.

## Verification boundary

The package remains a foundation, not a finished commercial plugin suite. Production compressor/EQ/saturation models, plugin-format validation, latency reporting, THD/IMD/aliasing characterization, CPU budgets, regression renders, and physical metrology validation remain release gates for the eventual flagship products.
