# Procedure Registry

The framework deliberately does not hide measurement methodology inside generic helpers. Procedures should be versioned and referenced by stable IDs.

Recommended StudioHub procedure identifiers:

- `SH-LEVEL-001` — calibrated level measurement
- `SH-FR-001` — frequency response
- `SH-THD-001` — harmonic distortion
- `SH-THDN-001` — THD+N
- `SH-IMD-001` — two-tone intermodulation
- `SH-NOISE-001` — noise / noise density
- `SH-TP-001` — true peak
- `SH-LUFS-001` — loudness
- `SH-LAT-001` — latency
- `SH-XTALK-001` — crosstalk

Each procedure should define stimulus, settling, capture length, sample rate, bandwidth, weighting, estimator, rejection criteria, uncertainty model, and acceptance criteria. The Measurement Lab implementation is the numerical executor; this layer records which procedure produced a result.
