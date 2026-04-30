# Haruka Editor

ImGui-based editor for the Haruka Engine. Loads `libHarukaEngine.so` from a
versioned build directory and provides scene authoring, asset management, and
play-mode testing.

---

## Architecture

Editor panels (`src/panels/`):

- `viewport` — 3D scene view with camera and gizmo controls
- `scene_hierarchy` — entity tree, selection, rename
- `inspector` — component properties, transform, material
- `project_browser` — asset file browser
- `material_editor` — PBR material authoring
- `console` — log output
- `stats` — performance counters
- `asset_importer` — model and texture import
- `settings` — editor preferences
- `ui_builder` — in-engine UI layout tool
- `planet_terrain_editor` — procedural terrain parameters
- `search_panel` — cross-asset search
- `export_panel` — game export

---

## Dependencies

```bash
sudo dnf install cmake gcc-c++ assimp-devel openssl-devel openal-soft-devel \
    gtk3-devel pkgconf-pkg-config sdl3-devel
```

Also requires `glad`, `glm`, `stb`, `imgui`, `ImGuizmo`, and `nativefiledialog`
under `third_party/`.

The **Haruka Engine** (`haruka-cpp`) must be built and installed first — see
[haruka-cpp/README.md](../haruka-cpp/README.md).

---

## Build

**Step 1 — Build and install the engine:**

```bash
cmake -B haruka-cpp/build haruka-cpp/ -DENGINE_VERSION=1.0.0
cmake --build haruka-cpp/build -j$(nproc)
cmake --install haruka-cpp/build --prefix haruka/build
```

**Step 2 — Build the editor:**

```bash
cmake -B haruka/build haruka/
cmake --build haruka/build -j$(nproc)
```

CMake auto-detects the newest version installed under `build/`. To pin a version:

```bash
cmake -B haruka/build haruka/ -DENGINE_VERSION=1.0.0
```

Output:

```
build/
└── 1.0.0/
    └── bin/
        ├── HarukaEditor
        ├── libHarukaEngine.so
        └── shaders/*.spv
```

Run:

```bash
./build/1.0.0/bin/HarukaEditor
```

---

## Rendering Features

- Deferred shading (G-buffer geometry + lighting passes)
- PBR material workflow (metallic/roughness)
- Directional and point-light shadows, cascaded shadow maps
- SSAO, IBL, HDR, bloom, tone mapping
- Compute-shader post-processing path

Core shader assets are in [haruka-cpp/shaders/](../haruka-cpp/shaders/).

---

## Project Layout

- [src/](src/) — editor source
- [src/panels/](src/panels/) — ImGui panel implementations
- [third_party/](third_party/) — vendored libraries
- [template/](template/) — new project skeleton
- [assets/](assets/) — editor assets

---

## Notes

- `Release` is the default build type.
- `SDL_GetBasePath()` is used at runtime to locate shaders relative to the binary.
- The engine `.so` is copied next to the binary at post-build; no `LD_LIBRARY_PATH` needed.
- Multiple engine versions can coexist under `build/` — switch with `-DENGINE_VERSION`.
