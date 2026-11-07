#pragma once

// a Blit project, the same layout the VS Code extension makes and builds (blitscript-vscode/src/project):
//
//   blit.project.json   name, entry script and the order of the scenes, in git
//   blit.local.json     this computer's homeinvasionsdl and blit-tools folders, not in git
//   scripts/main.bs     the game, INCLUDE "level.bs" for level objects
//   levels/*.blvl       levels from this editor
//   assets/             textures (assets/textures), models, sounds, packed into Engine.blfs by file name
//   .blit/engine        the project's own build of the engine and its tools
//   build/              the game: HomeInvasion.exe, Engine.blfs, compiled levels

#include <map>
#include <string>
#include <vector>

#include "project/Settings.h"

struct Project
{
    std::string root;
    std::string name;
    std::string main = "scripts/main.bs";
    std::string engine;
    std::string tools;
    std::vector<std::string> scenes;    // levels in scene order, relative to the root: "levels/main.blvl"
};

// the files and folders of a project
struct ProjectPaths
{
    std::string levels, assets, textures, scripts, main;
    std::string engineBuild, output, archive;
    std::string stage;              // .blit/stage: main.blit, manifest.txt and compiled levels, packed but not shipped
    std::string compiler, packer, levelCompiler, game, shipped;
};

ProjectPaths PathsOf(const Project& project);
std::string Executable(const std::string& name);    // "blitlevel" -> "blitlevel.exe" on Windows

bool IsProjectFolder(const std::string& root);
// reads it, the engine and tools folders come from blit.local.json, or the settings when it has none (and are saved)
bool OpenProject(const std::string& root, const Settings& settings, Project& project, std::string& error);
void WriteLocal(const Project& project);

// levels/*.blvl, sorted
std::vector<std::string> ListLevels(const Project& project);

// the levels in scene order, what ChangeScene's index counts in: the ones the project lists first, in its order, then
// any it doesn't list yet (main.blvl before the rest), so every level has an index. full paths
std::vector<std::string> SceneOrder(const Project& project);
// a new order into blit.project.json, levels are full paths
bool SaveSceneOrder(Project& project, const std::vector<std::string>& levels);
// a new level's file for this name, "" when the name won't do
std::string LevelFileFor(const Project& project, const std::string& name);
// the classes a script element can name: CLASS X EXTENDS LevelObject in scripts/, also through other classes
std::vector<std::string> LevelObjectClasses(const Project& project);

// a .bs file in scripts/ and the level object classes it defines
struct ScriptFile
{
    std::string file;                   // relative to scripts/, "main.bs", "enemies/zombie.bs"
    std::vector<std::string> classes;
};

std::vector<ScriptFile> ScriptFiles(const Project& project);

// every CLASS in scripts/: what it extends and which file it's in
struct ScriptScan
{
    std::vector<std::string> files;
    std::map<std::string, std::string> parents;
    std::map<std::string, std::string> fileOf;
};

ScriptScan ScanScripts(const Project& project);
bool IsLevelObject(const ScriptScan& scan, const std::string& name);
// models (.glb, .gltf) and sounds (.wav, .ogg, .mp3) under assets/, relative to the project's root
std::vector<std::string> ProjectFiles(const Project& project, const std::vector<std::string>& extensions);
// the folders textures are looked for in: the project's, then the engine's
std::vector<std::string> TextureFolders(const Project& project);

// "" when the name is a usable folder name, else why not
std::string CheckProjectName(const std::string& name);
