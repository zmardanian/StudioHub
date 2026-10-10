# Golden Measurement Chain

This framework now models the physical measurement chain without pretending that software creates calibration.

## Required evidence

A measurement chain is not considered ready merely because instruments exist. The release model requires:

1. instrument identity and service status;
2. a calibration certificate/evidence record for each measurement instrument;
3. certificate document SHA-256 and validity dates;
4. explicit traceability nodes and links;
5. uncertainty references for traceability nodes;
6. a named measurement procedure;
7. a recorded fixture/path identifier;
8. an environmental observation, with controlled status where the procedure requires it;
9. an explicit readiness assessment with blockers.

The framework fails closed: missing or expired evidence blocks `ready_for_measurement`.

## What must come from the physical world

The software cannot create the certificate, calibrate the ADC/DAC, establish SI traceability, or prove laboratory competence. Those are external evidence requirements.

For a future German laboratory, DAkkS is the national accreditation body and accredits testing/calibration laboratories under DIN EN ISO/IEC 17025 within a defined scope. The scope is specific to activities, not a blanket statement about every measurement a laboratory can perform.
