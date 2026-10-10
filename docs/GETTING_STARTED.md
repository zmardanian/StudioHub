# Getting Started

## Requirements

- Linux with a working graphical session (KDE Plasma is the primary desktop target).
- CMake, Ninja, a C++20 compiler, Git, and Python 3.
- Qt 6 development modules used by Qt Quick, QML, and Qt's process APIs.
- The pinned VST3 SDK in `third_party/vst3sdk` (already present in this checkout).
- Codex CLI for the real App Server integration. Sign in using Codex's supported login flow; StudioHub does not request or store API tokens.
- For plugin validation: Steinberg validator from the SDK, pluginval, and Bubblewrap for isolated validator execution.

On immutable Fedora/Bazzite, install development packages in a Fedora Toolbox rather than changing the host image. Provide Qt runtime libraries when launching the app from the host, or launch the app through that Toolbox.

## Build and launch Desktop

From the repository root:

```sh
cmake -S desktop -B build/desktop-app -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop-app -j2
./desktop/launch-studiohub-desktop
```

The launcher uses the available Qt runtime. On a Toolbox-based setup, create a Toolbox named `studiohub-qt-dev` with the Qt 6 development/runtime packages and Codex CLI available, then launch from that environment. The launcher reports failures and writes its log under `${XDG_STATE_HOME:-$HOME/.local/state}/studiohub/`.

## Build a plugin directly

Each project has its own manifest and CMake build directory. In StudioHub Desktop, select a project and use **Build**. For direct reproducible project builds:

```sh
cmake -S . -B build/vst3 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSTUDIOHUB_ENABLE_VST3=ON -DSTUDIOHUB_ENABLE_VSTGUI=ON \
  -DSMTG_CREATE_PLUGIN_LINK=OFF \
  -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF \
  -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=OFF
cmake --build build/vst3 -j2
```

Consult the selected project's `studiohub-project.json` for its target names, definitions, tests, and artifact path. Keep plugin-link creation disabled so development builds do not alter global plugin locations.

## Toolchain profile

`desktop/toolchain-profile.json` provides local executable and SDK settings. Keep machine-specific overrides outside source control. CMake and CTest resolve from `PATH`. The checked-in profile leaves pluginval unset so the app can still open and build on machines where it is not installed; pluginval actions show BLOCKED until you install it and set its executable name or path in the profile. StudioHub rejects paths naming protected audio runtimes and requires the pinned SDK location.

## Codex connection

Install Codex CLI and authenticate with its supported account flow, then connect from StudioHub Desktop. The application uses the installed `codex app-server` protocol and the user's Codex configuration. It reports effective model, reasoning, sandbox, and approval information returned by the server. It does not silently substitute a model or widen permissions.

The default model is GPT-6 Luna with the server's default reasoning option. Availability depends on the signed-in account and installed Codex version.

## Isolated validation

Use StudioHub's validation controls, which run validators through the Bubblewrap adapter. The tested plugin bundle is mounted read-only and the regular home directory is excluded. Missing isolation support blocks validation. Do not add broad system or home plugin directories to automated host tests. Manual DAW evaluation is separate and should use a deliberate user-controlled setup.

## Generated data

Build products, validation receipts, measurement output, and local state are generated locally. They are excluded from source control. Check available disk space before creating large audio renders or SDK builds.
