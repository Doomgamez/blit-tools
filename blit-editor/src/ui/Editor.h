#pragma once

// everything the editor window holds, and what its panels share
//
//   EditorActions.cpp   projects, levels, building and playing
//   EditorFrame.cpp     the dockspace, menus, toolbar, shortcuts and dialogs
//   Hub.cpp             first-run setup and choosing / creating a project
//   Hierarchy.cpp       the node tree       Inspector.cpp, ElementEditors.cpp   the selected node or face
//   Viewport.cpp        drawing and camera  ViewportTools.cpp                   picking, handles, placing
//   Assets.cpp          textures, models, levels and script classes             Console.cpp   the build log

#include <functional>
#include <string>
#include <vector>

#include "editor/Camera.h"
#include "editor/Document.h"
#include "editor/Factory.h"
#include "project/Create.h"
#include "project/Project.h"
#include "project/Runner.h"
#include "project/Settings.h"
#include "render/ClassIcons.h"
#include "render/Renderer.h"
#include "render/Scene.h"

enum class Screen { Setup, Hub, Editing };
enum class Tool { Select, Move, Rotate, Scale, Resize };

struct HubState
{
    NewProject form;
    std::string error;
    bool creating = false;
    std::string setupEngine, setupTools, setupToolchain;
};

// a brush resize in progress: the brushes as they were when the handle was grabbed
struct ResizeDrag
{
    bool active = false;
    int node = 0;
    Mat4 matrix;
    float bounds[6] = { 0, 0, 0, 0, 0, 0 };
    std::vector<Element> original;
};

struct Editor
{
    Screen screen = Screen::Setup;
    Settings settings;
    Project project;
    HubState hub;

    Document doc;
    Camera camera;
    TextureCache textures;
    ModelCache models;
    Renderer renderer;
    Framebuffer viewport;
    SceneMesh scene;
    SceneOptions sceneOptions;
    std::vector<LineVertex> overlay;

    Tool tool = Tool::Move;
    bool worldSpace = false;
    bool snap = true;
    float gridStep = 0.25f;         // metres, 3D
    float gridStep2D = 16.0f;       // pixels, 2D
    float rotateStep = 15.0f;
    bool faceMode = false;
    bool lighting = true;
    std::string currentTexture = "checker";
    ClassIcons classIcons;          // a picture per level object class, editor/classes/<Class>.png
    ResizeDrag resize;

    Runner runner;
    int seenRuns = 0;

    // refreshed when a project opens or the user asks
    std::vector<std::string> levels, classes, modelFiles, soundFiles;

    std::string status;
    bool quit = false;
    bool showDemo = false;
    bool viewportHovered = false;
    bool viewportFocused = false;

    // "save the changes?" before an action that would lose them
    std::function<void()> afterSave;
    bool askSave = false;
    bool askNewLevel = false;
    std::string newLevelName = "level";
    bool newLevel2D = false;

    // changes to the tree asked for while it's being drawn (menus, drops), run at the end of the frame
    std::vector<std::function<void()>> deferred;
};

// runs it at the end of the frame, after every panel is drawn
void Defer(Editor& editor, std::function<void()> action);
void RunDeferred(Editor& editor);

// the grid step for the level being edited
float GridStep(const Editor& editor);
SceneContext MakeSceneContext(Editor& editor);

// main.cpp: the editor's colours and spacing
void ApplyStyle();

// EditorActions.cpp
void InitEditor(Editor& editor);
void ShutdownEditor(Editor& editor);
bool OpenProjectFolder(Editor& editor, const std::string& root);
void CloseProject(Editor& editor);
void RefreshLists(Editor& editor);
bool OpenLevelFile(Editor& editor, const std::string& file);
bool CreateLevel(Editor& editor, const std::string& name, bool is2D);
bool SaveCurrentLevel(Editor& editor);
// runs action now, or after asking whether to save unsaved changes
void WhenSaved(Editor& editor, std::function<void()> action);
// nothing else running, an engine folder, and the level saved
bool ReadyToBuild(Editor& editor);
void PlayLevel(Editor& editor);
void PlayGame(Editor& editor);
void BuildProject(Editor& editor, bool forceEngine);
void CompileCurrentLevel(Editor& editor);
std::string WindowTitle(const Editor& editor);
// after a build: picks up new files
void PollRunner(Editor& editor);

// EditorFrame.cpp
void DrawEditor(Editor& editor);
void DrawMenuBar(Editor& editor);
void DrawToolbar(Editor& editor);
void DrawDialogs(Editor& editor);
void HandleShortcuts(Editor& editor);
// Shortcuts.cpp: a box around everything selected, false when nothing is
bool SelectionBounds(Editor& editor, Vec3& lo, Vec3& hi);
void FocusSelection(Editor& editor);
// halves or doubles the grid
void StepGrid(Editor& editor, bool up);
void ToolButton(Editor& editor, const char* label, Tool tool, const char* tip);
void DrawAddMenu(Editor& editor, int parent);
// a node of this kind at the middle of the view (or under parent), selected
void AddNodeOfKind(Editor& editor, NodeKind kind, int parent);

// the panels
void DrawHub(Editor& editor);
void DrawSetup(Editor& editor);
// Hub.cpp's pieces
bool BeginPage(const char* id, const char* title);
bool FolderField(const char* label, std::string& folder, const char* pickerTitle);
void Verdict(const std::string& problem, const char* good);
void DrawRecent(Editor& editor);
void DrawNewProject(Editor& editor);
void DrawHierarchy(Editor& editor);
std::string NodeSummary(const Node& node, bool is2D);
void NodeContextMenu(Editor& editor, Node& node);
void DrawNodeRow(Editor& editor, Node& node);
void DeferNodeChange(Editor& editor, int id, std::function<void(Node&)> change);
void DrawInspector(Editor& editor);
void DrawViewport(Editor& editor);
void DrawAssets(Editor& editor);
void DrawTextureGrid(Editor& editor);
void DrawModelList(Editor& editor);
void DrawLevelList(Editor& editor);
void DrawClassList(Editor& editor);
void DrawConsole(Editor& editor);
