# StudioHub Desktop

Qt 6 / Qt Quick frontend for manifest-driven StudioHub projects. The application uses the installed Codex App Server and runs builds, tests, validators, and measurements asynchronously.

## Build

From the repository root:

```sh
cmake -S desktop -B build/desktop-app -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop-app -j2
```

## Launch

```sh
./desktop/launch-studiohub-desktop
```

The launcher uses the `studiohub-qt-dev` Fedora Toolbox when it exists, otherwise it runs with the current environment's Qt runtime. Its log is stored under `${XDG_STATE_HOME:-$HOME/.local/state}/studiohub/`. The executable is `build/desktop-app/StudioHubDesktop`.

## Behavior

- Project manifests are described in [`project.schema.json`](project.schema.json); current examples are in the root project and `projects/stereo-utility`.
- The default Codex model is GPT-6 Luna with the server's default reasoning option. Effective availability and permissions come from the real App Server response.
- Validators use `run-isolated-validator.py` and Bubblewrap. Missing isolation support blocks the job.
- Full-GUI pluginval results require observed editor lifecycle completion markers; non-GUI validation is labeled separately.
- Local job evidence and generated build files are not checked into source control.

See the root [Getting Started](../docs/GETTING_STARTED.md) and [Validation](../docs/VALIDATION.md) guides.
