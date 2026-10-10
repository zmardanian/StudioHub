# StudioHub project manifest v1

Every project contains `studiohub-project.json`. See the reference at the repository root.
`schema_version` is 1; `root` is `.` relative to the manifest; `format` is `VST3`.
Identifiers and CMake/test target names use ASCII letters, digits, underscores and hyphens.
Names, category, GUI declaration, build configuration, dependency identity and fingerprint files
are required. Paths are project-relative, cannot traverse `..`, and cannot resolve through a
symlink outside the canonical project root. Build output and module must lie inside the declared
build directory/bundle. CMake definitions are scalar strings, not shell fragments; CMake control
variables are rejected. Global plugin links and SDK examples are always disabled.

Measurements declare `dynamics`, `stereo-utility` or `none`. A capability describes the analyzer,
not a PASS result. Jobs use direct process argument vectors. Open Project reads a manifest;
it does not execute code until the user invokes a build or development task.

Projects and selected project are persisted in StudioHub's Qt settings. Evidence is local to
each manifest. An old history entry is eligible only for the same project, revision, source
fingerprint and exact module SHA-256. Existing mission evidence is retained separately.
