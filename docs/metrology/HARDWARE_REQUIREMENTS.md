# Golden-chain hardware requirements

This document is a procurement and characterization specification, not a claim that any particular commercial device has been selected or calibrated.

## Reference source / DAC

The selected source should have manufacturer-published specifications appropriate to the intended measurands, including frequency accuracy, output-level accuracy, THD+N, dynamic range, channel matching and clock/reference behavior. The required performance must be set below the target uncertainty of each StudioHub procedure.

## Reference ADC / analyzer

The selected measurement input should have documented level accuracy, frequency response, THD+N, dynamic range, input range, channel matching and clock/reference behavior. Independent verification should be possible without relying solely on the same software stack being tested.

## Fixture / analog path

The fixture must have documented connector topology, gain/attenuation, bandwidth, shielding/grounding strategy, channel isolation and repeatability. Cable and fixture effects must be included where material to the uncertainty budget.

## Environmental instrumentation

Where environmental sensitivity is material, temperature, relative humidity and pressure should be measured with instruments whose calibration evidence is retained in the same chain.

## Selection rule

Do not select hardware because a manufacturer claims a headline THD+N or dynamic-range number. Select against the measurand-specific uncertainty budget and verify the complete chain experimentally.
