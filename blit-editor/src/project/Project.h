#pragma once

// a Blit project, the same layout the VS Code extension makes and builds (blitscript-vscode/src/project):
//
//   blit.project.json   name and entry script, in git
//   blit.local.json     this computer's homeinvasionsdl and blit-tools folders, not in git
//   scripts/main.bs     the game, INCLUDE "level.bs" for level objects
//   levels/*.blvl       levels from this editor
//   assets/             textures (assets/textures), models, sounds, packed into Engine.blfs by file name
//   .blit/engine        the project's own build of the engine and its tools
//   build/              the game: HomeInvasion.exe, Engine.blfs, compiled levels

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
// a new level's file for this name, "" when the name won't do
std::string LevelFileFor(const Project& project, const std::string& name);
// the classes a script element can name: CLASS X EXTENDS LevelObject in scripts/, also through other classes
std::vector<std::string> LevelObjectClasses(const Project& project);
// models (.glb, .gltf) and sounds (.wav, .ogg, .mp3) under assets/, relative to the project's root
std::vector<std::string> ProjectFiles(const Project& project, const std::vector<std::string>& extensions);
// the folders textures are looked for in: the project's, then the engine's
std::vector<std::string> TextureFolders(const Project& project);

// "" when the name is a usable folder name, else why not
std::string CheckProjectName(const std::string& name);
