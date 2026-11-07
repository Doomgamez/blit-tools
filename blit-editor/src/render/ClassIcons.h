#pragma once

// the one picture every script object shows, to paint yourself: blit-tools/blit-editor/icons/script.png. the editor
// writes a blank one when it's missing, and picks up changes to the file while it runs

#include <string>

#include "render/Gpu.h"

constexpr int kScriptIconSize = 64;

struct ScriptIcon
{
    std::string file;
    GLuint id = 0;
    long long stamp = 0;        // the file's last write time when it was loaded
    double checked = -1.0;      // when the file was last looked at, so it isn't every frame
};

// where the picture is, from the blit-tools folder. writes the blank one if there's none, true when one was made
bool SetScriptIconFolder(ScriptIcon& icon, const std::string& tools);
// the picture, reloaded when the file changed (looked at once a second). 0 when there's no file
GLuint GetScriptIcon(ScriptIcon& icon, double now);
void FreeScriptIcon(ScriptIcon& icon);

// the pieces
bool WriteBlankIcon(const std::string& file);
long long FileStamp(const std::string& file);
