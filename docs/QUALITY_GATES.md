# Production Quality Gates

A plugin should not be considered release-ready until the following are automated:

1. Compile with zero warnings.
2. Unit and property tests pass.
3. No NaN/Inf propagation under defined stress inputs.
4. No denormal performance pathologies.
5. Output remains finite for long renders.
6. Block-size changes do not alter expected behavior.
7. Sample-rate changes do not produce unstable behavior.
8. Parameter automation has no zipper artifacts beyond defined limits.
9. Latency is measured and reported.
10. Nonlinear processors meet their aliasing target at each oversampling setting.
11. THD/IMD are measured against reference expectations.
12. State/preset round-trip is exact within defined tolerances.
13. VST3/CLAP validation passes.
14. CPU cost remains inside the plugin's declared budget.
15. Regression renders remain within numerical tolerances.
