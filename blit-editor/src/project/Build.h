#pragma once

// the build, step for step what the VS Code extension does (blitscript-vscode/src/project/build.js), so a project
// built here and there comes out the same:
//
//   engine   cmake the engine into .blit/engine (HomeInvasion, blitscriptc, blitlevel, blitfs_pack), when it's
//            missing or older than the engine's sources
//   project  scripts -> .blit/stage/main.blit, levels/*.blvl -> .blit/stage/*.lvl.glb, all of it packed into
//            build/Engine.blfs, the game and its DLLs copied into build/. build/ is only what ships
//   play     the game in build/ with --level <name>.lvl.glb --assets <project>/assets

#include <string>
#include <vector>

#include "project/Project.h"
#include "project/Runner.h"

// PATH for the tools: the toolchain first, without MSYS's and Git's Unix tools, which stop cmake's MinGW generator
std::string BuildPath(const std::string& toolchain);

// the engine build is there and newer than every engine source
bool EngineComplete(const Project& project);

std::vector<BuildStep> EngineSteps(const Project& project, const std::string& toolchain);
std::vector<BuildStep> ProjectSteps(const Project& project);
// blitlevel on one level, into build/
BuildStep CompileLevelStep(const Project& project, const std::string& levelFile);
// the game on a level, the window's output goes to the log
BuildStep RunGameStep(const Project& project, const std::string& levelFile);

// everything Play needs: the engine if it's out of date, the project, then the game
std::vector<BuildStep> PlaySteps(const Project& project, const std::string& levelFile, const std::string& toolchain);
// everything Build needs, without running the game
std::vector<BuildStep> BuildSteps(const Project& project, const std::string& toolchain, bool forceEngine);

// .blit/stage/<name>.lvl.glb for levels/<name>.blvl
std::string CompiledLevel(const Project& project, const std::string& levelFile);
// what goes into Engine.blfs, the first file with a name wins
std::vector<std::string> PackList(const Project& project);
// when the engine was last built: .blit/engine/engine.built
std::string EngineStamp(const Project& project);
// the game and the DLLs next to it, what build/ needs to run without the toolchain
std::vector<std::string> RuntimeFiles(const Project& project);
std::string Lower(std::string text);
