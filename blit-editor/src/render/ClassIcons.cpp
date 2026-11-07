#include "render/ClassIcons.h"

#include <filesystem>

#include <SDL.h>
#include <SDL_image.h>

#include "core/Files.h"
#include "render/Textures.h"

void SetClassIconFolder(ClassIcons& icons, const std::string& folder)
{
    FreeClassIcons(icons);
    icons.folder = folder;
}

std::string ClassIconFile(const ClassIcons& icons, const std::string& className)
{
    return JoinPath(icons.folder, className + ".png");
}

long long FileStamp(const std::string& file)
{
    std::error_code error;
    auto time = std::filesystem::last_write_time(ToPath(file), error);
    return error ? 0 : (long long)time.time_since_epoch().count();
}

// a see-through square with a dashed grey frame and a faint cross: clearly "draw here", and clearly not a real icon
bool WriteBlankIcon(const std::string& file)
{
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kClassIconSize, kClassIconSize, 32, SDL_PIXELFORMAT_RGBA32);

    if (!surface)
    {
        return false;
    }

    Uint32* pixels = (Uint32*)surface->pixels;
    int pitch = surface->pitch / 4;
    Uint32 clear = SDL_MapRGBA(surface->format, 0, 0, 0, 0);
    Uint32 frame = SDL_MapRGBA(surface->format, 150, 150, 160, 255);
    Uint32 cross = SDL_MapRGBA(surface->format, 150, 150, 160, 70);

    for (int y = 0; y < kClassIconSize; y++)
    {
        for (int x = 0; x < kClassIconSize; x++)
        {
            bool edge = x < 2 || y < 2 || x >= kClassIconSize - 2 || y >= kClassIconSize - 2;
            bool dash = ((x + y) / 4) % 2 == 0;
            bool diagonal = x == y || x == kClassIconSize - 1 - y;
            pixels[y * pitch + x] = edge && dash ? frame : diagonal ? cross : clear;
        }
    }

    MakeFolders(ParentFolder(file));
    bool ok = IMG_SavePNG(surface, file.c_str()) == 0;
    SDL_FreeSurface(surface);
    return ok;
}

std::vector<std::string> MakeMissingClassIcons(ClassIcons& icons, const std::vector<std::string>& classes)
{
    std::vector<std::string> made;

    if (icons.folder.empty())
    {
        return made;
    }

    for (const std::string& name : classes)
    {
        std::string file = ClassIconFile(icons, name);

        if (!FileExists(file) && WriteBlankIcon(file))
        {
            made.push_back(file);
        }
    }

    return made;
}

GLuint GetClassIcon(ClassIcons& icons, const std::string& className, double now)
{
    if (className.empty() || icons.folder.empty())
    {
        return 0;
    }

    ClassIcon& icon = icons.loaded[className];

    // you're painting it: look at the file once a second and reload it when it was saved
    if (icon.checked >= 0.0 && now - icon.checked < 1.0)
    {
        return icon.id;
    }

    icon.checked = now;
    std::string file = ClassIconFile(icons, className);
    long long stamp = FileStamp(file);

    if (stamp != icon.stamp || (!icon.id && stamp))
    {
        if (icon.id) glDeleteTextures(1, &icon.id);

        int width = 0, height = 0;
        icon.id = stamp ? LoadImageTexture(file, width, height) : 0;
        icon.stamp = stamp;
    }

    return icon.id;
}

void FreeClassIcons(ClassIcons& icons)
{
    for (auto& entry : icons.loaded)
    {
        if (entry.second.id) glDeleteTextures(1, &entry.second.id);
    }

    icons.loaded.clear();
}
