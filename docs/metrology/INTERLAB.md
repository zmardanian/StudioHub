# Inter-laboratory comparison model

The framework now computes a two-party normalized difference:

`En = (x_participant - x_reference) / sqrt(u_participant^2 + u_reference^2)`

The acceptance threshold is explicit input and is evaluated before any result is interpreted. The implementation does not claim that `|En| <= 1` is universally required; the acceptance criterion must be justified by the comparison design and procedure.

A future comparison record should additionally identify the artefact/reference, measurand, procedure revision, participating laboratories, raw result and uncertainty for each participant, measurement date/environment, comparison model, pre-defined acceptance criterion, and report/document hashes.
