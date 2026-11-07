#include "project/Create.h"

#include "core/Files.h"
#include "core/Json.h"
#include "level/LevelFile.h"

const char* const kGitignore =
    "build/\n"
    ".blit/\n"
    "blit.local.json\n"
    "scripts/engine.bs\n"
    "scripts/level.bs\n"
    "scripts/level2d.bs\n"
    "scripts/levelsprite.bs\n"
    "scripts/leveltext.bs\n"
    "scripts/levelnodes2d.bs\n";

const std::vector<ProjectTemplate> kTemplates = {
    { "room", "Sample room (3D)", "The engine's test level: lights, a trigger that opens a door, plated walls and a house prop, run by BlitScript classes.", false },
    { "empty3d", "Empty 3D level", "A floor, a player start and a light, with a first-person walker.", false },
    { "empty2d", "2D level", "Platforms, a player start and a sprite, with a script that moves a box around them.", true },
};

const std::vector<ProjectTemplate>& ProjectTemplates()
{
    return kTemplates;
}

std::string ReplaceAll(std::string text, const std::string& from, const std::string& to)
{
    for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
    {
        text.replace(at, from.size(), to);
    }

    return text;
}

std::string CheckNewProject(const NewProject& values)
{
    std::string problem = CheckProjectName(values.name);

    if (!problem.empty()) return problem;
    if (!FolderExists(values.location)) return "Choose an existing folder to put it in";
    if (FolderExists(JoinPath(values.location, values.name))) return values.name + " already exists there";

    problem = CheckEngineFolder(values.engine);
    if (!problem.empty()) return problem;

    return CheckToolsFolder(values.tools);
}

bool TemplateScript(const NewProject& values, bool is2D, std::string& script, std::string& error)
{
    std::string example = JoinPath(values.engine, is2D ? "blitscript/examples/platformer.bs" : "blitscript/examples/scripted.bs");

    if (!ReadTextFile(example, script))
    {
        error = "This template needs " + RelativePath(example, values.engine) + " from a newer engine";
        return false;
    }

    // the examples sit one folder below the libraries, a project keeps them next to main.bs
    script = ReplaceAll(script, "INCLUDE \"../level.bs\"", "INCLUDE \"level.bs\"");
    script = ReplaceAll(script, "INCLUDE \"../level2d.bs\"", "INCLUDE \"level2d.bs\"");
    script = ReplaceAll(script, "INCLUDE \"../engine.bs\"", "INCLUDE \"engine.bs\"");
    script = ReplaceAll(script, "\"testmap.blvl\"", "\"main.blvl\"");
    script = ReplaceAll(script, "\"level2d.blvl\"", "\"main.blvl\"");
    return true;
}

bool CopyAll(const std::string& engine, const std::string& root, const std::vector<std::pair<std::string, std::string>>& files,
             std::string& error)
{
    for (const auto& file : files)
    {
        if (!CopyFileTo(JoinPath(engine, file.first), JoinPath(root, file.second)))
        {
            error = "Couldn't copy " + file.first + " from the engine";
            return false;
        }
    }

    return true;
}

bool CreateProject(const NewProject& values, Project& project, std::string& error)
{
    const ProjectTemplate* chosen = nullptr;

    for (const ProjectTemplate& t : ProjectTemplates())
    {
        if (values.templateId == t.id) chosen = &t;
    }

    error = CheckNewProject(values);

    if (!error.empty()) return false;
    if (!chosen) { error = "Choose a template"; return false; }

    std::string script;
    if (!TemplateScript(values, chosen->is2D, script, error)) return false;

    std::string root = JoinPath(values.location, values.name);
    std::vector<std::pair<std::string, std::string>> copies = {
        { "blitscript/engine.bs", "scripts/engine.bs" },
        { "blitscript/level.bs", "scripts/level.bs" },
        { "blitscript/level2d.bs", "scripts/level2d.bs" },
        { "blitscript/levelsprite.bs", "scripts/levelsprite.bs" },
        { "blitscript/leveltext.bs", "scripts/leveltext.bs" },
        { "blitscript/levelnodes2d.bs", "scripts/levelnodes2d.bs" },
        { "assets/textures/checker.png", "assets/textures/checker.png" },
    };

    if (values.templateId == "room")
    {
        copies.push_back({ "levels/testmap.blvl", "levels/main.blvl" });
        copies.push_back({ "assets/models/house.glb", "assets/models/house.glb" });

        for (const char* name : { "panel", "panel_normal", "panel_rough", "clip", "nodraw", "skip", "trigger" })
        {
            copies.push_back({ std::string("assets/textures/") + name + ".png", std::string("assets/textures/") + name + ".png" });
        }
    }

    if (!MakeFolders(JoinPath(root, "scripts")) || !MakeFolders(JoinPath(root, "levels")) ||
        !MakeFolders(JoinPath(root, "assets/textures")))
    {
        error = "Couldn't make the project's folders in " + values.location;
        return false;
    }

    Json manifest = JsonObject();
    manifest["name"] = Json(values.name);
    manifest["main"] = Json("scripts/main.bs");

    project = Project();
    project.root = CleanPath(root);
    project.name = values.name;
    project.engine = values.engine;
    project.tools = values.tools;

    if (!JsonWriteFile(JoinPath(root, "blit.project.json"), manifest) || !WriteTextFile(JoinPath(root, ".gitignore"), kGitignore) ||
        !WriteTextFile(JoinPath(root, "scripts/main.bs"), script) || !CopyAll(values.engine, root, copies, error))
    {
        if (error.empty()) error = "Couldn't write the project's files";
        return false;
    }

    WriteLocal(project);

    if (values.templateId != "room")
    {
        SaveLevel(JoinPath(root, "levels/main.blvl"), chosen->is2D ? StarterLevel2D() : StarterLevel3D());
    }

    return true;
}
