# blit-editor

The Blit engine's level editor: Dear ImGui (docking) on SDL2 and OpenGL 2.1, ImGuizmo for the handles. It edits
`levels/*.blvl` in a Blit project, 3D and 2D, and builds and plays them with the project's engine. The level format
and what each element compiles to are in `homeinvasionsdl/docs/LevelEditor.md`.

## Build

MSYS2 MinGW64 with SDL2 and SDL2_image (the engine's toolchain), and CMake:

```
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build -j 8
build/blit-editor.exe
```

The DLLs it needs are copied next to it. `third_party/` holds Dear ImGui 1.91.9b (docking), ImGuizmo, cgltf and the
engine's glad.

## Using it

- **First run** asks for the `homeinvasionsdl` and `blit-tools` folders (and MinGW's `bin`, `C:/msys64/mingw64/bin`
  by default). They're kept in `%APPDATA%/Blit/BlitEditor/settings.json`.
- **Projects**: open a project folder or make one from a template (sample room, empty 3D, 2D). They're the same
  projects the VS Code extension makes, so either tool opens them.
- **Nodes**: everything in a level is a node in the Hierarchy, with elements in the Inspector: a brush, a point light,
  a model, a player start, a script (a BlitScript class that `EXTENDS LevelObject`), properties... Drag nodes onto
  each other to nest them.
- **Viewport**: right mouse + WASD/QE flies (wheel sets the speed), middle mouse pans. The view menu has top, front
  and side views, 2D levels get a 2D view. Q select, W move, E rotate, R scale, T resize brushes by their sides,
  F focuses, `[` and `]` change the grid, G toggles face mode for texturing.
- **Textures and models**: drag them from the Assets panel onto a brush (a face in face mode) or into the level.
- **Play** (F5) saves, builds the engine if it's missing or out of date, builds the project and runs the game on this
  level. The script loads it with `LaunchLevelOr` from `level.bs`. F7 only compiles the level.

## Code

| Folder | |
|---|---|
| `src/core` | JSON, maths, files, processes, the folder picker |
| `src/level` | the level's nodes and elements, `.blvl` reading and writing |
| `src/geometry` | brushes to polygons and UVs, the same as the engine's `blitlevel` |
| `src/editor` | the document (selection, undo), the camera, the Add menu's node kinds |
| `src/render` | framebuffer, shaders, textures, glTF models, the scene's meshes |
| `src/project` | settings, projects, templates, the build steps and the thread they run on |
| `src/ui` | the hub and the panels: hierarchy, inspector, viewport, assets, console |
