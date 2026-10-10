# StudioHub

**A native Linux workspace for building and validating audio plugins.**

StudioHub brings project management, Codex-assisted development, reproducible builds, plugin validation, and DSP measurement into one desktop application. It is built for Linux audio engineers who want a visible, evidence-backed path from a plugin idea to a VST3 they can load in their DAW.

We believe StudioHub can soon become a game changer for Linux audio engineers. Today it is a functional release candidate: useful and exercised with real projects, while still under active hardening.

## What is included

- **StudioHub Desktop** — Qt 6 / Qt Quick interface for projects, Codex conversations, builds, validation, measurements, and evidence.
- **StudioComp FET** — a native VST3 dynamics processor and the original reference project.
- **StudioHub Stereo Utility** — a separate native VST3 project demonstrating the reusable project workflow.
- **Project manifests and scaffolding** — a versioned description of build targets, artifacts, tests, and measurement capabilities.
- **Deterministic toolchain** — CMake/Ninja builds, CTest, Steinberg validation, isolated pluginval runs, and artifact provenance.
- **DSP laboratory** — repeatable source-level measurements and listening-comparison preparation tools.
- **Pinned Steinberg VST3 SDK** — included with its upstream license and notices.

## Quick start

See [Getting Started](docs/GETTING_STARTED.md) for Fedora/Bazzite dependencies, Toolbox setup, Codex authentication, build commands, and the KDE launch option. The short version:

```sh
cmake -S desktop -B build/desktop-app -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop-app -j2
./desktop/launch-studiohub-desktop
```

The desktop executable is `build/desktop-app/StudioHubDesktop`. Qt development/runtime dependencies may require a Fedora Toolbox on immutable Fedora systems.

## Project status

This is a **1.0 release candidate**, not a declaration of commercial or production maturity. Both included projects have been built and exercised through StudioHub's verification workflow. The validated plugin artifacts are generated locally and are not included in this repository. Run the current verification on your own machine before relying on a build.

The compressor's sonic character remains an open creative evaluation. Automated measurements establish repeatability and behavior; they do not establish musical preference or parity with commercial processors. Human listening remains necessary.

See [Validation and evidence](docs/VALIDATION.md), the [verification snapshot](docs/VERIFICATION-SNAPSHOT.md), [Architecture](docs/ARCHITECTURE.md), [Roadmap](docs/ROADMAP.md), and [release notes](RELEASE_NOTES.md).

## Licensing

No project-wide license has been declared for StudioHub-authored code. Publication does not grant permission to reuse that code. Third-party components retain their own licenses and notices; see [Third-party notices](THIRD_PARTY.md) and the license files shipped with each dependency. A project-wide license decision is still needed before inviting code reuse or contributions under an open-source license.

## Contributing

Read [Contributing](CONTRIBUTING.md) before submitting changes. Please include the exact project revision, artifact identity, test commands, and relevant output when reporting a validation result.
