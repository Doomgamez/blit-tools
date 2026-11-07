# Roadmap: Godot's features in Blit

The aim is for blit-editor and the engine (homeinvasionsdl) to do what Godot's editor and engine do, in Blit's own
way: C99 engine, BlitScript, `.blvl` levels made of nodes and elements, Dear ImGui editor. The behaviour is taken
from Godot's source (MIT), the code is written fresh to fit Blit.

Godot is many years of work by hundreds of people, so this goes in phases. Each phase is something you can use on
its own when it's done. The order puts 2D games first, then what both 2D and 3D need, then 3D.

Status: **done**, **doing**, or blank for not started.

Two rules for every phase:

- **Scripts reach everything.** Whatever the editor puts in a level (a sprite, a tile map, an animation, a shape) can
  be read and changed from BlitScript while the game runs. A feature isn't done until its script side is.
- **2D first, then 3D.** Phases 1 to 8 finish 2D, phase 9 is 3D.

## Phase 0 - Sprites (Sprite2D, TextureRect, NinePatchRect) - done

- [x] Size from the node's box or the frame, the node's scale resizes it (Sprite2D has no size either)
- [x] Scaled, tiled by count, repeat at the frame's own size (TextureRect stretch modes)
- [x] Flip H / V, region, sprite sheets: columns, rows, frame, `SetFrame` to animate
- [x] Scripts: `FindLevelSprite`, then `SetTexture`, `SetSize`, `SetFlip`, `SetTiles`, `SetRegion`, `SetSheet`, `SetFrame`,
      `SetTint`, `SetAngle`, `SetPosition`, `SetVisible`. A sprite follows its node or its box
- [ ] Nine-patch: borders that don't stretch, per-axis stretch / tile / tile-fit (NinePatchRect)

## Phase 1 - The 2D canvas (CanvasItemEditor) - done, waiting to be tried in the editor

- [x] A rectangle with 8 handles on the selection: Shift keeps the shape, Alt resizes from the middle
- [x] Drag anything to move it, Shift locks one axis, a drag on empty space box-selects
- [x] Smart snapping: to other nodes' sides and middles, the parent, guides, the grid, with a line showing what
- [x] Arrow keys nudge a pixel, Shift a grid step
- [x] Alt + right click lists everything under the mouse
- [x] Rulers along the view, guides dragged out of them and saved with the level
- [x] Scripts: what the canvas edits is what scripts move: `FindLevelBox`, then `MoveBy`, `SetPosition`, `SetAngle`,
      `SetSize`, `Refresh`. The engine gained `GetNodeParent`

## Cameras - done

- [x] `LevelCamera` from the level's Camera node, 2D (zoom, turn) and 3D (fov), scripts move it or its node
- [x] What's on top comes from the camera: the farthest is drawn first (`SortByDepth`). In 2D a higher z is nearer
- [x] The 2D view shows each camera's frame, the game window's size over its zoom
- [ ] Follow, limits and smoothing (phase 6)

## Nodes and text - done

- [x] Text element (Godot's Label): in the level in 2D and 3D, or on the screen as a HUD. `LevelText`, `FindLevelText`,
      `SetText`... The examples' HUDs are text nodes now, no `DrawText`
- [x] Switch any node off and on from scripts (`SetLevelNodeEnabled`, `SetNodeEnabled`): not drawn, no collision, no
      triggers, its light dark, its sprite, box, text and script object skipped
- [x] Nodes made while running (`CreateNode`, `CreateLevelText`, `CreateLevelSprite`, `CreateLevelBox`)
- [ ] Freeing nodes made while running, and making a node from another level (prefabs, phase 5)

## Scripting - doing

- [x] Expressions (`x = (a + b) * 2`, calls and methods inside them), `+= -= ++`, calls as lines, `RETURN a + b`
- [x] `IF` / `ELSEIF` / `ELSE` / `ENDIF` with `AND OR NOT`, `WHILE` / `ENDWHILE`; the VS Code linter knows them
- [ ] Vectors in expressions (`position + velocity * dt`)
- [ ] Rewrite the libraries (level2d.bs...) with them

## Phase 2 - Tile maps (TileMapLayer, TileSet)

- [ ] A tile set from a sheet: tile size, separation, per-tile collision shapes
- [ ] A tile map element: paint, erase, rectangle, bucket fill, pick, several layers
- [ ] Terrains (autotiling): edges and corners chosen from the neighbours
- [ ] The engine draws them in one batch and collides with them

## Phase 3 - Animation (AnimatedSprite2D, AnimationPlayer)

- [ ] Animations by name on a sprite: frames, speed, loop, played from scripts
- [ ] An animation panel: a timeline with keys on any element property, easing, tracks that call script methods
- [ ] Animation trees later: blends and state machines

## Phase 4 - 2D physics (CollisionShape2D, bodies, Area2D)

- [ ] Shapes as elements: rectangle, circle, capsule, polygon, segment, drawn and edited in the canvas
- [ ] Static, kinematic (`MoveAndSlide` with floors, walls and slopes) and rigid bodies in the engine
- [ ] Areas: enter / exit events for scripts, layers and masks
- [ ] Raycasts and shape queries from scripts

## Phase 5 - Scenes and signals (PackedScene, signals)

- [ ] A level placed inside another as a node, with per-instance overrides (prefabs)
- [ ] Signals: a node says what it sends, the inspector connects them to script methods
- [ ] Groups: tag nodes, find them by tag from scripts

## Phase 6 - More 2D nodes

- [ ] Camera 2D: follow, limits, smoothing, zoom, shake
- [ ] Parallax layers
- [ ] Particles: emitter, amount, lifetime, velocity, gravity, colour and size over life
- [ ] Lights and shadows in 2D: point and directional lights, occluders
- [ ] Paths with followers, lines, polygons, text labels
- [ ] Positional audio, audio buses with volume and effects

## Phase 7 - UI (Control)

- [ ] Anchors and margins, containers (box, grid, margin, scroll), buttons, labels, sliders, text fields
- [ ] Themes, edited in the canvas like 2D nodes

## Phase 8 - Workflow

- [ ] A remote scene tree: see and change nodes in the running game
- [ ] Debugger: breakpoints and stepping in BlitScript, a profiler
- [ ] Export: one folder or zip per platform, icons, a project settings panel
- [ ] Input map: named actions bound to keys and buttons

## Phase 9 - 3D

- [ ] Grid maps (tile maps in 3D) from model libraries
- [ ] 3D physics bodies and shapes, character controller
- [ ] Navigation meshes and pathfinding (2D too)
- [ ] Environment: sky, fog, tonemapping, glow. Reflection probes and light baking need more than OpenGL 2.1
