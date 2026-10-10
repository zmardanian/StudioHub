# Uncertainty Policy

A reported uncertainty must have identifiable components. The default v1.0 combination is:

`u_c = sqrt(sum((c_i * u_i)^2))`

where `u_i` is a standard uncertainty component and `c_i` its sensitivity coefficient.

Expanded uncertainty is:

`U = k * u_c`

The default coverage factor in the library is `k=2`, but this is metadata, not a guarantee of a 95% coverage probability for every distribution or degrees-of-freedom situation.

Do not use a fictitious uncertainty merely to make a result look precise. When correlation between components matters, a covariance-aware extension is required.
