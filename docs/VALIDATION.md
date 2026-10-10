# Validation and Evidence

## Run verification

The Desktop **Verify Project** workflow runs the selected project's build, DSP tests, metadata checks, Steinberg validation, applicable pluginval checks, and measurement job. Each gate has its own status and evidence. A failed or blocked gate remains visible; an earlier passing result does not certify a changed artifact.

For a focused command-line check, consult the project's manifest and the root build instructions. The standard StudioComp FET CTest target is `StudioCompFETTests`; the Stereo Utility project declares its own unit, integration, state, and measurement targets.

## Plugin GUI lifecycle

Full-GUI pluginval testing is separate from `--skip-gui-tests`. A GUI PASS requires observed completion markers for Editor, Editor Automation, and opening the editor while processing. Each run records the pluginval result and exact bundle identity. The Linux VSTGUI Cairo device teardown correction is included in the pinned SDK source; it was exercised through controlled failing/patched/reverted experiments during development.

Host and graphics stacks vary by distribution. Recheck GUI lifecycle behavior on the target machine and preserve crash logs rather than interpreting an aborted run as success. Manual DAW interaction is useful independent evidence, but does not replace automated lifecycle checks.

## DSP measurements

StudioComp FET measurements cover transfer and dynamics behavior with deterministic signals. Stereo Utility measurements cover its declared gain, balance, bypass, channel, smoothing, and state behavior. Reports identify the source/build/artifact context where available.

The laboratory prepares repeatable, level-matched WAV comparisons. Listening judgment remains human work. Measurements do not prove that a processor sounds warm, punchy, musical, or commercially competitive.

## Evidence state

Desktop distinguishes PASS, FAIL, BLOCKED, STALE, CANCELED, TIMEOUT, and NOT RUN. Evidence-write errors are shown separately from command success. Build success, test success, and release readiness are separate conclusions. Local receipts and user audio are intentionally not included in the public source repository.

## Known verification limitation

ThreadSanitizer execution was blocked in the development Toolbox by a missing runtime library. Other targeted state-exchange and regression tests passed, but that unavailable sanitizer result should not be represented as a TSAN PASS.
