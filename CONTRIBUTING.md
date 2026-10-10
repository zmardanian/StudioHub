# Contributing

StudioHub is under active development. Before contributing, note that StudioHub-authored source currently has no project-wide reuse license; please contact the project owner before redistributing or submitting patches intended for public reuse.

For engineering changes:

1. Keep DSP processing bounded and allocation-free in the audio callback.
2. Preserve project independence through manifests and capability-specific tests.
3. Tie validation claims to the exact source revision and artifact fingerprint.
4. Run focused tests for the changed behavior and record exact commands/results.
5. Do not include personal audio, credentials, machine-specific paths, build output, or local evidence receipts in a source contribution.
6. Do not run broad plugin scans or use a normal DAW configuration for automated tests.

Include a concise summary, changed behavior, verification commands, and any tests that are blocked or not run.
