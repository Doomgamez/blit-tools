#pragma once

// what the editor remembers on this computer (%APPDATA%/Blit/BlitEditor/settings.json): where homeinvasionsdl and
// blit-tools are, the MinGW toolchain, and the projects opened lately

#include <string>
#include <vector>

struct Settings
{
    std::string engine;                 // the homeinvasionsdl folder
    std::string tools;                  // the blit-tools folder
    std::string toolchain;              // MinGW's bin folder, C:/msys64/mingw64/bin when empty and it's there
    std::vector<std::string> recent;    // project folders, newest first
    float gridStep = 0.25f;             // last grid size, metres
    bool loaded = false;
};

std::string SettingsFile();
Settings LoadSettings();
void SaveSettings(const Settings& settings);
// puts the project first in the recent list
void RememberProject(Settings& settings, const std::string& root);

// "" when the folder is a homeinvasionsdl / blit-tools checkout, else what's wrong with it
std::string CheckEngineFolder(const std::string& folder);
std::string CheckToolsFolder(const std::string& folder);
std::string CheckFolder(const std::string& folder, const std::vector<std::string>& markers, const std::string& describe);
// the first of markers that's missing under folder, "" when they're all there
std::string MissingMarker(const std::string& folder, const std::vector<std::string>& markers);

// the toolchain to build the engine with, "" when there's none
std::string FindToolchain(const Settings& settings);
