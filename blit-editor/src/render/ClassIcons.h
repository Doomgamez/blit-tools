#pragma once

// a picture for each level object class, to paint yourself: <project>/editor/classes/<ClassName>.png. the editor makes
// a blank one for every class that has none, and picks up changes to the file while it runs. editor/ isn't packed into
// the game, only assets/ is

#include <map>
#include <string>
#include <vector>

#include "render/Gpu.h"

constexpr int kClassIconSize = 64;

struct ClassIcon
{
    GLuint id = 0;
    long long stamp = 0;        // the file's last write time when it was loaded
    double checked = -1.0;      // when the file was last looked at, so it isn't every frame
};

struct ClassIcons
{
    std::string folder;
    std::map<std::string, ClassIcon> loaded;
};

void SetClassIconFolder(ClassIcons& icons, const std::string& folder);
// the file for a class's picture
std::string ClassIconFile(const ClassIcons& icons, const std::string& className);
// writes a blank picture for every class without one, the ones it made
std::vector<std::string> MakeMissingClassIcons(ClassIcons& icons, const std::vector<std::string>& classes);
// the class's picture, reloaded when the file changed (looked at once a second). 0 when there's no file
GLuint GetClassIcon(ClassIcons& icons, const std::string& className, double now);
void FreeClassIcons(ClassIcons& icons);

// the pieces
bool WriteBlankIcon(const std::string& file);
long long FileStamp(const std::string& file);
