#include "project/Project.h"

#include <algorithm>
#include <map>
#include <regex>
#include <set>

#include "core/Files.h"
#include "core/Json.h"

const char* const kProjectFile = "blit.project.json";
const char* const kLocalFile = "blit.local.json";

std::string Executable(const std::string& name)
{
#ifdef _WIN32
    return name + ".exe";
#else
    return name;
#endif
}

ProjectPaths PathsOf(const Project& project)
{
    ProjectPaths paths;

    paths.levels = JoinPath(project.root, "levels");
    paths.assets = JoinPath(project.root, "assets");
    paths.textures = JoinPath(paths.assets, "textures");
    paths.main = JoinPath(project.root, project.main);
    paths.scripts = ParentFolder(paths.main);
    paths.engineBuild = JoinPath(project.root, ".blit/engine");
    paths.output = JoinPath(project.root, "build");
    paths.stage = JoinPath(project.root, ".blit/stage");
    paths.archive = JoinPath(paths.output, "Engine.blfs");
    paths.compiler = JoinPath(paths.engineBuild, Executable("blitscriptc"));
    paths.packer = JoinPath(paths.engineBuild, Executable("blitfs_pack"));
    paths.levelCompiler = JoinPath(paths.engineBuild, Executable("blitlevel"));
    paths.game = JoinPath(paths.engineBuild, Executable("HomeInvasion"));
    paths.shipped = JoinPath(paths.output, Executable("HomeInvasion"));
    return paths;
}

bool IsProjectFolder(const std::string& root)
{
    return FileExists(JoinPath(root, kProjectFile));
}

void WriteLocal(const Project& project)
{
    Json local;
    std::string error;
    std::string file = JoinPath(project.root, kLocalFile);

    if (!JsonReadFile(file, local, error) || !local.IsObject())
    {
        local = JsonObject();
    }

    local["engine"] = Json(project.engine);
    local["tools"] = Json(project.tools);
    JsonWriteFile(file, local);
}

bool OpenProject(const std::string& root, const Settings& settings, Project& project, std::string& error)
{
    Json json, local;
    std::string ignored;

    if (!JsonReadFile(JoinPath(root, kProjectFile), json, error))
    {
        error = "There's no Blit project here (no " + std::string(kProjectFile) + ")";
        return false;
    }

    JsonReadFile(JoinPath(root, kLocalFile), local, ignored);

    project = Project();
    project.root = CleanPath(root);
    project.name = json.Text("name", FileName(root));
    project.main = json.Text("main", "scripts/main.bs");
    project.engine = local.Text("engine", settings.engine);
    project.tools = local.Text("tools", settings.tools);

    if (const Json* scenes = json.Find("scenes"))
    {
        for (size_t i = 0; scenes->IsArray() && i < scenes->items.size(); i++)
        {
            project.scenes.push_back(scenes->items[i].text);
        }
    }

    // a project cloned onto this computer has no local file yet
    if (!local.Find("engine") || !local.Find("tools"))
    {
        WriteLocal(project);
    }

    return true;
}

std::vector<std::string> ListLevels(const Project& project)
{
    return ListFiles(PathsOf(project).levels, { ".blvl" }, false);
}

std::vector<std::string> SceneOrder(const Project& project)
{
    std::vector<std::string> levels = ListLevels(project), ordered;

    for (const std::string& listed : project.scenes)
    {
        std::string file = CleanPath(JoinPath(project.root, listed));

        for (const std::string& level : levels)
        {
            if (CleanPath(level) == file && std::find(ordered.begin(), ordered.end(), level) == ordered.end())
            {
                ordered.push_back(level);
            }
        }
    }

    // the ones it doesn't list: main first, it's where a new game starts
    std::sort(levels.begin(), levels.end(), [](const std::string& a, const std::string& b)
    {
        bool aMain = FileName(a) == "main.blvl", bMain = FileName(b) == "main.blvl";
        return aMain != bMain ? aMain : a < b;
    });

    for (const std::string& level : levels)
    {
        if (std::find(ordered.begin(), ordered.end(), level) == ordered.end()) ordered.push_back(level);
    }

    return ordered;
}

bool SaveSceneOrder(Project& project, const std::vector<std::string>& levels)
{
    std::string file = JoinPath(project.root, kProjectFile), error;
    Json json;

    if (!JsonReadFile(file, json, error))
    {
        return false;
    }

    project.scenes.clear();
    Json& scenes = json["scenes"] = JsonArray();

    for (const std::string& level : levels)
    {
        std::string relative = RelativePath(level, project.root);
        project.scenes.push_back(relative);
        scenes.items.push_back(Json(relative));
    }

    return JsonWriteFile(file, json);
}

std::string CheckProjectName(const std::string& name)
{
    if (name.empty())
    {
        return "Give it a name";
    }

    if (!std::regex_match(name, std::regex("^[A-Za-z0-9][A-Za-z0-9 _-]*$")) || name.back() == ' ')
    {
        return "Use letters, numbers, spaces, - and _";
    }

    if (std::regex_match(name, std::regex("^(con|prn|aux|nul|com[0-9]|lpt[0-9])$", std::regex::icase)))
    {
        return "Windows reserves this name";
    }

    return "";
}

std::string LevelFileFor(const Project& project, const std::string& name)
{
    if (!CheckProjectName(name).empty())
    {
        return "";
    }

    return JoinPath(PathsOf(project).levels, name + ".blvl");
}

ScriptScan ScanScripts(const Project& project)
{
    ScriptScan scan;
    std::regex declaration(R"(^\s*CLASS\s+([A-Za-z_][A-Za-z0-9_]*)(\s+EXTENDS\s+([A-Za-z_][A-Za-z0-9_]*))?)",
                           std::regex::icase);

    for (const std::string& file : ListFiles(PathsOf(project).scripts, { ".bs" }, true))
    {
        std::string text;

        scan.files.push_back(file);

        if (!ReadTextFile(file, text)) continue;

        for (size_t start = 0; start < text.size();)
        {
            size_t end = text.find('\n', start);
            std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
            std::smatch match;
            start = end == std::string::npos ? text.size() : end + 1;

            if (std::regex_search(line, match, declaration))
            {
                scan.parents[match[1]] = match[3];
                scan.fileOf[match[1]] = file;
            }
        }
    }

    return scan;
}

bool IsLevelObject(const ScriptScan& scan, const std::string& name)
{
    // follow EXTENDS up to LevelObject, a loop or an unknown class ends it
    std::set<std::string> seen;
    auto found = scan.parents.find(name);
    std::string at = found == scan.parents.end() ? "" : found->second;

    while (!at.empty() && at != "LevelObject" && !seen.count(at) && scan.parents.count(at))
    {
        seen.insert(at);
        at = scan.parents.at(at);
    }

    return at == "LevelObject" && name != "LevelObject";
}

std::vector<std::string> LevelObjectClasses(const Project& project)
{
    ScriptScan scan = ScanScripts(project);
    std::vector<std::string> out;

    for (const auto& entry : scan.parents)
    {
        if (IsLevelObject(scan, entry.first)) out.push_back(entry.first);
    }

    return out;
}

std::vector<ScriptFile> ScriptFiles(const Project& project)
{
    ScriptScan scan = ScanScripts(project);
    std::vector<ScriptFile> out;

    for (const std::string& file : scan.files)
    {
        ScriptFile script;
        script.file = RelativePath(file, PathsOf(project).scripts);

        for (const auto& entry : scan.fileOf)
        {
            if (entry.second == file && IsLevelObject(scan, entry.first)) script.classes.push_back(entry.first);
        }

        out.push_back(script);
    }

    return out;
}

std::vector<std::string> ProjectFiles(const Project& project, const std::vector<std::string>& extensions)
{
    std::vector<std::string> out;

    for (const std::string& file : ListFiles(PathsOf(project).assets, extensions, true))
    {
        out.push_back(RelativePath(file, project.root));
    }

    return out;
}

std::vector<std::string> TextureFolders(const Project& project)
{
    ProjectPaths paths = PathsOf(project);
    std::vector<std::string> folders = { paths.textures, paths.assets };

    if (!project.engine.empty())
    {
        folders.push_back(JoinPath(project.engine, "assets/textures"));
    }

    return folders;
}
