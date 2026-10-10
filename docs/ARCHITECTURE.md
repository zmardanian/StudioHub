# Architecture

StudioHub keeps three responsibilities explicit:

1. **Desktop application:** Qt Quick UI, project selection, development state, Codex transport, build and validation controls, and evidence inspection.
2. **Codex:** software design and implementation through the installed official App Server. StudioHub does not implement a second coding agent.
3. **Deterministic engineering tools:** project scaffolding, manifest validation, CMake/CTest, plugin validators, DSP measurements, artifact fingerprints, and job receipts.

## Projects

Each plugin project is rooted by `studiohub-project.json`. The manifest identifies the project, build target/configuration, expected bundle and module, tests, evidence directory, dependency fingerprint inputs, and measurement capability. `desktop/project.schema.json` describes the shared format. Project paths are canonicalized and checked before operations.

The root project contains StudioComp FET. `projects/stereo-utility` is an independent project with its own manifest, source, tests, and dependency lock. The Desktop backend uses the manifest and capability selection rather than adding project-specific branches for the second project.

## Jobs and evidence

Build, test, validation, and measurement jobs capture their project snapshot, source identity, dependency/toolchain identity, command, status, timestamps, artifact hashes, and log/evidence locations. Bundle identity includes bundle resources as well as the executable module. Results are tied to the exact built artifact; changed sources or artifacts make old verification stale.

Validators run through the isolated adapter. Their subprocesses receive only the selected test artifact and explicitly prepared paths. A successful command is not sufficient when required evidence is missing or malformed.

## Codex transport

The Desktop process communicates with `codex app-server --listen stdio://` using framed JSON messages. Turn terminal states are distinct from message streaming and tool activity. User-visible approval handling is restricted to recognized request types; unsupported requests fail closed. Threads are associated with the selected project directory, and interrupted state is surfaced for explicit recovery.

## DSP tools

Measurement analyzers are capability-specific. Dynamics reports cover StudioComp FET; stereo-utility reports cover channel behavior, gain, balance, bypass, smoothing, and state. These reports are engineering measurements, not subjective quality scores.
