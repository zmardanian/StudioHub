# Verification snapshot

This is a scrubbed summary of an actual StudioHub Desktop acceptance run on 10 October 2026. Local logs, screenshots, user audio, and absolute workstation paths are intentionally kept out of this public source snapshot.

## Desktop acceptance

The Qt Desktop completed the full Verify Project workflow against both included projects:

| Project | Build | DSP tests | Metadata | Steinberg validator | pluginval strictness 5 full GUI | Measurements |
|---|---:|---:|---:|---:|---:|---:|
| StudioComp FET | PASS | PASS | PASS | PASS | PASS | PASS |
| StudioHub Stereo Utility | PASS | PASS | PASS | PASS | PASS | PASS |

The GUI pluginval logs recorded completion of **Editor**, **Editor Automation**, and **Open editor whilst processing** for each project. These were actual full-GUI results, not `--skip-gui-tests` runs. The application stored per-job source/dependency/tool fingerprints, artifact identities, commands, exit statuses, timestamps, and logs.

The Desktop regression suite passed **8/8** tests, covering manifest validation, controller/protocol/recovery behavior, Bubblewrap isolation, provenance, activity records, toolchain validation, QML startup, and App Server framing. Stereo Utility's standalone regression suite passed **4/4** tests, including state exchange, DSP, native VST3 integration, and measurements.

Project switching and restart restored the selected project and evidence history. A resource-only bundle change correctly marked earlier verification stale. Controlled missing-evidence and failed-command cases remained failures. Codex's real App Server connection and account/model catalog were verified; no authentication token is part of this repository.

## Protected historical FET artifacts

The known original module SHA-256 was `6e04b40aca9be209da4bb7bacd1f678b398fa70ab5fe5e50d1749f94f9c9994c`. The Cairo-corrected reference module SHA-256 was `af14aa5c86c3868a74cc0665002b80adc3abf98e6576111c400f92c565b9048a`. These hashes document the historical local acceptance evidence; generated plugin binaries are not shipped here, and the values do not identify a build produced from this public checkout.

## Limitations

- ThreadSanitizer could not run in the development Toolbox because its runtime library was unavailable; this is not a TSAN pass.
- Linux host/graphics stacks vary. Repeat full-GUI plugin validation on the target system and keep exact failure evidence.
- Automated DSP measurements do not establish subjective quality. Human listening is still required for musical character and comparative evaluation.
- The run demonstrates the tested configurations on one development workstation, not universal Linux compatibility or a commercial-readiness claim.
