# Metrology Architecture

```text
Reference stimulus
      |
      v
  physical DUT <---- calibrated measurement chain
      |
      v
Measurement Lab procedure
      |
      +--> raw quantity
      +--> calibration correction
      +--> uncertainty components
      |
      v
MeasurementResult
      |
      +--> procedure ID
      +--> software version/revision
      +--> timestamp
      +--> instrument/certificate
      +--> traceability statement
      +--> value/unit
      +--> standard + expanded uncertainty
      +--> warnings
      |
      v
machine-readable report / golden dataset
```

The key design rule is that **a numerical result without provenance is not a complete metrology record**.

Calibration correction is interpolated only inside explicitly supplied frequency points. Out-of-range requests fail rather than silently extrapolating. Calibration application also requires a valid measurement date and rejects expired or not-yet-valid calibration evidence.

Calibration-point uncertainty is not interpolated as though its frequency dependence were known; the larger bracketing standard uncertainty is used conservatively. Uncertainty is represented as standard components and combined by RSS under the independence assumption. Correlated components require a future covariance-aware model and must not be disguised as independent components.
