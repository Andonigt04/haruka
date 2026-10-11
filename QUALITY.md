# Code and testing standards

The editor follows the engine's general standards
(**[Haruka Engine QUALITY.md](https://github.com/Andonigt04/haruka-cpp/blob/develop/QUALITY.md)**):
RAII, `enum class`, includes at the top, own header first, tests with a counter-test, and saying
what was not verified. This file adds what is specific to the editor.

## The core is engine-free

- Nothing under the core (`src/` outside `src/backends/`) may include or name an engine header.
  The `check_core_engine_free` target runs on every build and fails it otherwise.
- The core compiles as **C++17**. The Haruka backend compiles as C++26, because the engine
  requires it publicly. Do not raise the core's standard just to match a backend.
- Anything that depends on one engine goes behind `IEngineBackend` (`src/backend/engine_backend.h`)
  or, for scenes, `ISceneModel`.

## Contracts are versioned

- **Backend contract:** `IEngineBackend` is a C++ ABI (a vtable). Any change to it (a new method,
  a reordering, a changed signature) must bump `HARUKA_EDITOR_BACKEND_API_VERSION` and say what
  changed in its comment. An old backend is then refused instead of crashing.
- **Plugin contract:** same rule for `haruka_editor_plugin.h` and `HARUKA_EDITOR_PLUGIN_API_VERSION`.
  Plugins declare their version with `HARUKA_EDITOR_PLUGIN_DECLARE_API()`. A plain `const int`
  inside `extern "C"` has internal linkage and is not exported.
- **Remote protocol:** messages in `remote_protocol.h` are shared with engines in other processes.
  Add fields in a backward-compatible way, and update `tools/mock_engine` in the same change.

## One ImGui context

- The executable exports its symbols, so the engine, backends and plugins use the editor's Dear
  ImGui. Never add `-fno-semantic-interposition`, `-Bsymbolic` or hidden visibility to the engine
  or the editor: that splits the ImGui context and breaks panels silently.
- The vendored ImGui must be exactly the engine's (see `third_party/VENDORED.md`). Update both
  together.

## Naming

The editor's own files are still `snake_case`, while the engine has moved to PascalCase. Until the
editor is migrated, follow the naming already used in the folder you are working in.

## Testing

- Test a change by running the editor headless (`SDL_VIDEODRIVER=offscreen`,
  `HARUKA_EDITOR_QUIT_AFTER`, a temporary `XDG_CONFIG_HOME`), and check the generated `imgui.ini`
  for the windows that were drawn.
- Test remote-backend changes against `tools/mock_engine`. Socket and threading code should also
  run clean under AddressSanitizer and ThreadSanitizer.
