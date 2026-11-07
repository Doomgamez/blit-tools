#include "project/Build.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <set>

#include "core/Files.h"

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return text;
}

std::string BuildPath(const std::string& toolchain)
{
#ifdef _WIN32
    const char separator = ';';
#else
    const char separator = ':';
#endif
    std::string current = CurrentPath(), out = toolchain;

    for (size_t start = 0; start <= current.size();)
    {
        size_t end = current.find(separator, start);
        std::string folder = current.substr(start, end == std::string::npos ? std::string::npos : end - start);
        std::string lower = Lower(CleanPath(folder));
        start = end == std::string::npos ? current.size() + 1 : end + 1;

#ifdef _WIN32
        if (lower.find("msys64/usr") != std::string::npos || lower.find("git/usr") != std::string::npos ||
            lower.find("git/bin") != std::string::npos)
        {
            continue;
        }
#endif

        if (!folder.empty())
        {
            out += (out.empty() ? "" : std::string(1, separator)) + folder;
        }
    }

    return out;
}

std::string EngineStamp(const Project& project)
{
    return JoinPath(PathsOf(project).engineBuild, "engine.built");
}

bool EngineComplete(const Project& project)
{
    ProjectPaths paths = PathsOf(project);
    std::string stamp = EngineStamp(project);

    for (const std::string& program : { paths.game, paths.compiler, paths.packer, paths.levelCompiler })
    {
        if (!FileExists(program)) return false;
    }

#ifdef _WIN32
    if (!FileExists(JoinPath(paths.engineBuild, "SDL2.dll"))) return false;
#endif

    if (!FileExists(stamp)) return false;

    // the engine repository moved on since: its sources are newer than the build
    for (const char* folder : { "src", "blitscript", "blitfs", "cmake", "tools/blitlevel" })
    {
        for (const std::string& file : ListFiles(JoinPath(project.engine, folder), { ".c", ".h", ".cmake" }, true))
        {
            if (IsNewer(file, stamp)) return false;
        }
    }

    return !IsNewer(JoinPath(project.engine, "CMakeLists.txt"), stamp);
}

std::vector<BuildStep> EngineSteps(const Project& project, const std::string& toolchain)
{
    ProjectPaths paths = PathsOf(project);
    std::vector<std::string> configure = { "-S", project.engine, "-B", paths.engineBuild, "-DCMAKE_BUILD_TYPE=Release" };

#ifdef _WIN32
    configure.insert(configure.end(), { "-G", "MinGW Makefiles", "-DCMAKE_C_COMPILER=gcc" });

    if (!toolchain.empty())
    {
        configure.push_back("-DCMAKE_PREFIX_PATH=" + ParentFolder(toolchain));
    }
#endif

    return {
        { "Configuring the engine", [=](StepContext& c) { return c.Run("cmake", configure, project.root); } },
        { "Building the engine", [=](StepContext& c)
          { return c.Run("cmake", { "--build", paths.engineBuild, "-j", "8" }, project.root); } },
        { "Noting the engine build", [=](StepContext&)
          { return WriteTextFile(EngineStamp(project), std::to_string((long long)std::time(nullptr)) + "\n"); } },
    };
}

std::string CompiledLevel(const Project& project, const std::string& levelFile)
{
    return JoinPath(PathsOf(project).stage, FileStem(levelFile) + ".lvl.glb");
}

std::vector<std::string> PackList(const Project& project)
{
    ProjectPaths paths = PathsOf(project);
    std::vector<std::string> files = { JoinPath(paths.stage, "main.blit"), JoinPath(paths.stage, "manifest.txt") };
    std::set<std::string> taken;

    for (const std::string& file : ListFiles(paths.assets, {}, true))
    {
        if (Lower(FileName(file)) != "manifest.txt")
        {
            files.push_back(file);
            taken.insert(Lower(FileName(file)));
        }
    }

    // only levels that still exist: a deleted level's old .lvl.glb stays out
    for (const std::string& level : ListLevels(project))
    {
        if (FileExists(CompiledLevel(project, level))) files.push_back(CompiledLevel(project, level));
    }

    // the engine's shaders and font, unless the project has its own with that name
    std::vector<std::string> fromEngine = ListFiles(JoinPath(project.engine, "assets/shaders"), {}, false);
    fromEngine.push_back(JoinPath(project.engine, "blitfs/arial.ttf"));

    for (const std::string& file : fromEngine)
    {
        if (!taken.count(Lower(FileName(file)))) files.push_back(file);
    }

    return files;
}

std::vector<std::string> RuntimeFiles(const Project& project)
{
    ProjectPaths paths = PathsOf(project);
    std::vector<std::string> files = { paths.game };

#ifdef _WIN32
    for (const std::string& dll : ListFiles(paths.engineBuild, { ".dll" }, false)) files.push_back(dll);
#endif

    return files;
}

BuildStep CompileLevelStep(const Project& project, const std::string& levelFile)
{
    ProjectPaths paths = PathsOf(project);

    return { "Compiling level " + FileName(levelFile), [=](StepContext& c)
    {
        MakeFolders(paths.stage);
        return c.Run(paths.levelCompiler, { "-i", levelFile, "-o", CompiledLevel(project, levelFile),
                                            "-t", paths.textures, "-t", paths.assets }, project.root);
    } };
}

std::vector<BuildStep> ProjectSteps(const Project& project)
{
    ProjectPaths paths = PathsOf(project);
    std::vector<BuildStep> steps;

    steps.push_back({ "Preparing the build folders", [=](StepContext&)
    {
        // builds from before .blit/stage left these next to the game, it doesn't read them
        for (const std::string& old : ListFiles(paths.output, { ".blit", ".glb" }, false)) RemoveFile(old);
        RemoveFile(JoinPath(paths.output, "manifest.txt"));
        return MakeFolders(paths.output) && MakeFolders(paths.stage);
    } });

    // INCLUDE "engine.bs" and "level.bs" find them next to the scripts
    for (const char* library : { "engine.bs", "level.bs", "leveltext.bs", "level2d.bs", "levelsprite.bs", "levelnodes2d.bs" })
    {
        std::string from = JoinPath(project.engine, std::string("blitscript/") + library);
        steps.push_back({ std::string("Updating ") + library, [=](StepContext&)
                          { return !FileExists(from) || CopyFileTo(from, JoinPath(paths.scripts, library)); } });
    }

    steps.push_back({ "Compiling scripts", [=](StepContext& c)
                      { return c.Run(paths.compiler, { "-i", paths.main, "-o", JoinPath(paths.stage, "main.blit") }, paths.scripts); } });

    for (const std::string& level : ListLevels(project))
    {
        steps.push_back(CompileLevelStep(project, level));
    }

    steps.push_back({ "Writing the manifest", [=](StepContext&)
                      { return WriteTextFile(JoinPath(paths.stage, "manifest.txt"), "Main-Script: main.blit\n"); } });
    steps.push_back({ "Packing Engine.blfs", [=](StepContext& c)
    {
        RemoveFile(paths.archive);

        for (const std::string& file : PackList(project))
        {
            if (!c.Run(paths.packer, { "-m", "0", "-i", file, "-o", paths.archive }, paths.stage))
            {
                LogAdd(*c.log, LogKind::Error, "could not pack " + file);
                return false;
            }
        }

        return true;
    } });
    steps.push_back({ "Copying the game and its DLLs", [=](StepContext&)
    {
        for (const std::string& file : RuntimeFiles(project))
        {
            if (!CopyFileTo(file, JoinPath(paths.output, FileName(file)))) return false;
        }

        return true;
    } });

    return steps;
}

BuildStep RunGameStep(const Project& project, const std::string& levelFile)
{
    ProjectPaths paths = PathsOf(project);
    std::vector<std::string> args;

    if (!levelFile.empty())
    {
        // the compiled level stays in .blit/stage, --level reads it by its full path and names it by its file name
        args = { "--level", CompiledLevel(project, levelFile), "--assets", paths.assets };
    }

    return { "Running the game", [=](StepContext& c)
    {
        LogAdd(*c.log, LogKind::Info, levelFile.empty() ? "the game starts on its own level"
                                                        : "the script loads this level with LaunchLevelOr (level.bs)");
        c.Run(paths.shipped, args, paths.output);
        return true; // closing the game window is how a preview ends, whatever its exit code
    } };
}

std::vector<BuildStep> BuildSteps(const Project& project, const std::string& toolchain, bool forceEngine)
{
    std::vector<BuildStep> steps;

    if (forceEngine || !EngineComplete(project))
    {
        steps = EngineSteps(project, toolchain);
    }

    for (BuildStep& step : ProjectSteps(project)) steps.push_back(std::move(step));
    return steps;
}

std::vector<BuildStep> PlaySteps(const Project& project, const std::string& levelFile, const std::string& toolchain)
{
    std::vector<BuildStep> steps = BuildSteps(project, toolchain, false);
    steps.push_back(RunGameStep(project, levelFile));
    return steps;
}
