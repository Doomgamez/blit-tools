#include "ui/Editor.h"

#include "core/Files.h"
#include "geometry/Brush.h"
#include "level/LevelFile.h"
#include "project/Build.h"

float GridStep(const Editor& editor)
{
    return editor.doc.level.is2D ? editor.gridStep2D : editor.gridStep;
}

SceneContext MakeSceneContext(Editor& editor)
{
    SceneContext context;
    context.textures = &editor.textures;
    context.models = &editor.models;
    context.projectRoot = editor.project.root;
    return context;
}

void InitEditor(Editor& editor)
{
    editor.settings = LoadSettings();
    editor.gridStep = editor.settings.gridStep;
    InitTextures(editor.textures);

    if (!InitRenderer(editor.renderer))
    {
        LogAdd(editor.runner.log, LogKind::Error, "the viewport's shaders didn't compile: " + editor.renderer.error);
    }

    // the folders are known and still there: straight to the projects
    bool ready = CheckEngineFolder(editor.settings.engine).empty() && CheckToolsFolder(editor.settings.tools).empty();
    editor.screen = ready ? Screen::Hub : Screen::Setup;

    if (ready && SetScriptIconFolder(editor.scriptIcon, editor.settings.tools))
    {
        LogAdd(editor.runner.log, LogKind::Info, "made " + editor.scriptIcon.file + ", paint it: every script object shows it");
    }
    editor.hub.setupEngine = editor.settings.engine;
    editor.hub.setupTools = editor.settings.tools;
    editor.hub.setupToolchain = editor.settings.toolchain;

    // a first run from inside blit-tools: guess both folders
    if (editor.hub.setupTools.empty())
    {
        for (std::string at = ProgramFolder(); !at.empty() && at.find('/') != std::string::npos; at = ParentFolder(at))
        {
            if (CheckToolsFolder(at).empty())
            {
                editor.hub.setupTools = at;
                std::string sibling = JoinPath(ParentFolder(at), "homeinvasionsdl");
                editor.hub.setupEngine = CheckEngineFolder(sibling).empty() ? sibling : editor.hub.setupEngine;
                break;
            }
        }
    }
}

void ShutdownEditor(Editor& editor)
{
    StopSteps(editor.runner);
    JoinRunner(editor.runner);
    editor.settings.gridStep = editor.gridStep;
    SaveSettings(editor.settings);
    FreeModels(editor.models);
    FreeScriptIcon(editor.scriptIcon);
    FreeTextures(editor.textures);
    DestroyFramebuffer(editor.viewport);
    FreeRenderer(editor.renderer);
}

void RefreshLists(Editor& editor)
{
    editor.levels = SceneOrder(editor.project);
    editor.classes = LevelObjectClasses(editor.project);
    editor.scripts = ScriptFiles(editor.project);
    editor.modelFiles = ProjectFiles(editor.project, { ".glb", ".gltf" });
    editor.soundFiles = ProjectFiles(editor.project, { ".wav", ".ogg", ".mp3", ".flac" });
    RefreshTextureNames(editor.textures);
}

bool OpenProjectFolder(Editor& editor, const std::string& root)
{
    Project project;
    std::string error;

    if (!OpenProject(root, editor.settings, project, error))
    {
        editor.hub.error = error;
        return false;
    }

    editor.project = project;
    RememberProject(editor.settings, project.root);
    FreeModels(editor.models);
    SetTextureFolders(editor.textures, TextureFolders(project));
    RefreshLists(editor);
    editor.screen = Screen::Editing;
    editor.status = "Opened " + project.name;

    if (!CheckEngineFolder(project.engine).empty())
    {
        LogAdd(editor.runner.log, LogKind::Error, "this project's homeinvasionsdl folder (" + project.engine +
               ") isn't there, set it in Project > Folders");
    }

    // the main level, else the first one, else a new one
    std::string main = JoinPath(PathsOf(project).levels, "main.blvl");
    std::string first = FileExists(main) ? main : (editor.levels.empty() ? "" : editor.levels.front());

    if (first.empty() || !OpenLevelFile(editor, first))
    {
        editor.doc.New(false);
        ResetCamera(editor.camera, false);
    }

    return true;
}

bool OpenFromCommandLine(Editor& editor, const std::string& file)
{
    std::string level = CleanPath(file);

    if (FileExtension(level) != ".blvl" || !FileExists(level))
    {
        LogAdd(editor.runner.log, LogKind::Error, "can't open " + file + ": not a .blvl level");
        return false;
    }

    // the project is the nearest folder above the level with a blit.project.json
    for (std::string at = ParentFolder(level); !at.empty(); at = ParentFolder(at))
    {
        if (IsProjectFolder(at))
        {
            return OpenProjectFolder(editor, at) && OpenLevelFile(editor, level);
        }

        if (ParentFolder(at) == at) break;
    }

    editor.hub.error = file + " isn't inside a Blit project";
    return false;
}

void CloseProject(Editor& editor)
{
    editor.doc.New(false);
    editor.project = Project();
    editor.screen = Screen::Hub;
}

bool OpenLevelFile(Editor& editor, const std::string& file)
{
    std::string error;

    if (!editor.doc.Load(file, error))
    {
        LogAdd(editor.runner.log, LogKind::Error, error);
        return false;
    }

    ResetCamera(editor.camera, editor.doc.level.is2D);
    editor.faceMode = false;
    editor.status = "Opened " + FileName(file);
    return true;
}

bool CreateLevel(Editor& editor, const std::string& name, bool is2D)
{
    std::string file = LevelFileFor(editor.project, name);

    if (file.empty() || FileExists(file))
    {
        editor.status = file.empty() ? "That name won't do for a file" : name + ".blvl already exists";
        return false;
    }

    MakeFolders(ParentFolder(file));

    if (!SaveLevel(file, is2D ? StarterLevel2D() : StarterLevel3D()))
    {
        editor.status = "Couldn't write " + file;
        return false;
    }

    RefreshLists(editor);
    return OpenLevelFile(editor, file);
}

bool SaveCurrentLevel(Editor& editor)
{
    if (editor.doc.path.empty())
    {
        editor.askNewLevel = true; // an unnamed level gets a name first
        return false;
    }

    if (!editor.doc.Save())
    {
        LogAdd(editor.runner.log, LogKind::Error, "couldn't save " + editor.doc.path);
        return false;
    }

    editor.status = "Saved " + FileName(editor.doc.path);
    return true;
}

void WhenSaved(Editor& editor, std::function<void()> action)
{
    if (!editor.doc.dirty)
    {
        action();
        return;
    }

    editor.afterSave = std::move(action);
    editor.askSave = true;
}

bool ReadyToBuild(Editor& editor)
{
    if (editor.runner.busy)
    {
        editor.status = "Something is still running, stop it first";
        return false;
    }

    std::string problem = CheckEngineFolder(editor.project.engine);

    if (!problem.empty())
    {
        LogAdd(editor.runner.log, LogKind::Error, "homeinvasionsdl: " + problem);
        return false;
    }

    if (editor.doc.dirty && !editor.doc.path.empty())
    {
        SaveCurrentLevel(editor);
    }

    return true;
}

void PlayLevel(Editor& editor)
{
    if (editor.doc.path.empty())
    {
        editor.askNewLevel = true;
        return;
    }

    if (!ReadyToBuild(editor)) return;

    std::string toolchain = FindToolchain(editor.settings);
    StartSteps(editor.runner, "Play " + FileName(editor.doc.path), PlaySteps(editor.project, editor.doc.path, toolchain),
               BuildPath(toolchain));
}

void PlayGame(Editor& editor)
{
    if (!ReadyToBuild(editor)) return;

    std::string toolchain = FindToolchain(editor.settings);
    std::vector<BuildStep> steps = BuildSteps(editor.project, toolchain, false);
    steps.push_back(RunGameStep(editor.project, ""));
    StartSteps(editor.runner, "Play " + editor.project.name, steps, BuildPath(toolchain));
}

void BuildProject(Editor& editor, bool forceEngine)
{
    if (!ReadyToBuild(editor)) return;

    std::string toolchain = FindToolchain(editor.settings);
    StartSteps(editor.runner, forceEngine ? "Rebuild the engine and project" : "Build " + editor.project.name,
               BuildSteps(editor.project, toolchain, forceEngine), BuildPath(toolchain));
}

void CompileCurrentLevel(Editor& editor)
{
    if (editor.doc.path.empty() || !ReadyToBuild(editor)) return;

    std::string toolchain = FindToolchain(editor.settings);
    std::vector<BuildStep> steps;

    // blitlevel comes with the engine build
    if (!FileExists(PathsOf(editor.project).levelCompiler) || !EngineComplete(editor.project))
    {
        steps = EngineSteps(editor.project, toolchain);
    }

    steps.push_back(CompileLevelStep(editor.project, editor.doc.path));
    StartSteps(editor.runner, "Compile " + FileName(editor.doc.path), steps, BuildPath(toolchain));
}

void PollRunner(Editor& editor)
{
    if (editor.runner.finished != editor.seenRuns && !editor.runner.busy)
    {
        editor.seenRuns = editor.runner.finished;
        editor.status = editor.runner.title + (editor.runner.succeeded ? ": done" : ": failed, see the console");
        RefreshLists(editor);
    }
}

std::string WindowTitle(const Editor& editor)
{
    if (editor.screen != Screen::Editing)
    {
        return "Blit Level Editor";
    }

    return editor.doc.Title() + " - " + editor.project.name + " - Blit Level Editor";
}
