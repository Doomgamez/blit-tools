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

std::vector<std::string> LevelObjectClasses(const Project& project)
{
    std::map<std::string, std::string> parents;     // class -> what it extends
    std::regex declaration(R"(^\s*CLASS\s+([A-Za-z_][A-Za-z0-9_]*)(\s+EXTENDS\s+([A-Za-z_][A-Za-z0-9_]*))?)",
                           std::regex::icase);

    for (const std::string& file : ListFiles(PathsOf(project).scripts, { ".bs" }, true))
    {
        std::string text, line;

        if (!ReadTextFile(file, text)) continue;

        for (size_t start = 0; start < text.size();)
        {
            size_t end = text.find('\n', start);
            line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
            start = end == std::string::npos ? text.size() : end + 1;

            std::smatch match;
            if (std::regex_search(line, match, declaration))
            {
                parents[match[1]] = match[3];
            }
        }
    }

    std::vector<std::string> out;

    for (const auto& entry : parents)
    {
        // follow EXTENDS up to LevelObject, a loop or an unknown class ends it
        std::set<std::string> seen;
        std::string at = entry.second;

        while (!at.empty() && at != "LevelObject" && !seen.count(at) && parents.count(at))
        {
            seen.insert(at);
            at = parents[at];
        }

        if (at == "LevelObject" && entry.first != "LevelObject")
        {
            out.push_back(entry.first);
        }
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
