#include "project/Settings.h"

#include <algorithm>

#include "core/Files.h"
#include "core/Json.h"

// the same markers the VS Code extension checks (blitscript-vscode/src/project/layout.js)
const std::vector<std::string> kEngineMarkers = { "CMakeLists.txt", "src/Engine/Engine.h", "blitscript/blitscriptc.c",
                                                  "blitscript/engine.bs", "blitfs/arial.ttf", "tools/blitlevel/blitlevel.c" };
const std::vector<std::string> kToolsMarkers = { "gltf-inspector/main.py", "blitfs-viewer/main.py", "blit-editor/CMakeLists.txt" };
const char* const kDefaultToolchain = "C:/msys64/mingw64/bin";
constexpr size_t kRecentLimit = 12;

std::string SettingsFile()
{
    return JoinPath(SettingsFolder(), "settings.json");
}

Settings LoadSettings()
{
    Settings settings;
    Json json;
    std::string error;

    if (!JsonReadFile(SettingsFile(), json, error))
    {
        return settings;
    }

    settings.engine = json.Text("engine");
    settings.tools = json.Text("tools");
    settings.toolchain = json.Text("toolchain");
    settings.gridStep = (float)json.Number("gridStep", settings.gridStep);
    settings.loaded = true;

    if (const Json* recent = json.Find("recent"))
    {
        for (const Json& item : recent->items)
        {
            if (item.type == JsonType::String) settings.recent.push_back(item.text);
        }
    }

    return settings;
}

void SaveSettings(const Settings& settings)
{
    Json json = JsonObject();
    json["engine"] = Json(settings.engine);
    json["tools"] = Json(settings.tools);
    json["toolchain"] = Json(settings.toolchain);
    json["gridStep"] = Json(settings.gridStep);

    Json& recent = json["recent"] = JsonArray();
    for (const std::string& root : settings.recent) recent.items.push_back(Json(root));

    JsonWriteFile(SettingsFile(), json);
}

void RememberProject(Settings& settings, const std::string& root)
{
    std::string clean = CleanPath(root);

    settings.recent.erase(std::remove(settings.recent.begin(), settings.recent.end(), clean), settings.recent.end());
    settings.recent.insert(settings.recent.begin(), clean);

    if (settings.recent.size() > kRecentLimit)
    {
        settings.recent.resize(kRecentLimit);
    }

    SaveSettings(settings);
}

std::string MissingMarker(const std::string& folder, const std::vector<std::string>& markers)
{
    for (const std::string& marker : markers)
    {
        if (!FileExists(JoinPath(folder, marker)))
        {
            return marker;
        }
    }

    return "";
}

std::string CheckFolder(const std::string& folder, const std::vector<std::string>& markers, const std::string& describe)
{
    if (folder.empty())
    {
        return "Choose the " + describe + " folder";
    }

    if (!FolderExists(folder))
    {
        return "This folder doesn't exist";
    }

    std::string missing = MissingMarker(folder, markers);
    return missing.empty() ? "" : "This isn't the " + describe + " folder: there's no " + missing;
}

std::string CheckEngineFolder(const std::string& folder)
{
    return CheckFolder(folder, kEngineMarkers, "homeinvasionsdl");
}

std::string CheckToolsFolder(const std::string& folder)
{
    return CheckFolder(folder, kToolsMarkers, "blit-tools");
}

std::string FindToolchain(const Settings& settings)
{
    if (!settings.toolchain.empty())
    {
        return settings.toolchain;
    }

    return FolderExists(kDefaultToolchain) ? kDefaultToolchain : "";
}
