# Measurement Lab Contract

Every production plugin should eventually have automated measurements for:

- frequency response
- impulse response
- RMS and peak
- true peak
- THD / harmonic spectrum
- IMD / SMPTE or CCIF-style tests where appropriate
- aliasing energy
- noise floor
- latency
- attack time
- release time
- gain-reduction curve
- stereo-link behavior
- parameter discontinuity
- sample-rate independence
- block-size independence
- NaN/Inf/denormal safety
- CPU cost

Reference signals:
- sine
- swept sine
- logarithmic sweep
- impulse
- white/pink noise
- multitone
- silence

All results should be machine-readable so they can become regression gates.
