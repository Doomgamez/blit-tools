#include "render/ClassIcons.h"

#include <filesystem>

#include <SDL.h>
#include <SDL_image.h>

#include "core/Files.h"
#include "render/Textures.h"

long long FileStamp(const std::string& file)
{
    std::error_code error;
    auto time = std::filesystem::last_write_time(ToPath(file), error);
    return error ? 0 : (long long)time.time_since_epoch().count();
}

// a see-through square with a dashed grey frame and a faint cross: clearly "draw here", and clearly not a real icon
bool WriteBlankIcon(const std::string& file)
{
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kScriptIconSize, kScriptIconSize, 32, SDL_PIXELFORMAT_RGBA32);

    if (!surface)
    {
        return false;
    }

    Uint32* pixels = (Uint32*)surface->pixels;
    int pitch = surface->pitch / 4;
    Uint32 clear = SDL_MapRGBA(surface->format, 0, 0, 0, 0);
    Uint32 frame = SDL_MapRGBA(surface->format, 150, 150, 160, 255);
    Uint32 cross = SDL_MapRGBA(surface->format, 150, 150, 160, 70);

    for (int y = 0; y < kScriptIconSize; y++)
    {
        for (int x = 0; x < kScriptIconSize; x++)
        {
            bool edge = x < 2 || y < 2 || x >= kScriptIconSize - 2 || y >= kScriptIconSize - 2;
            bool dash = ((x + y) / 4) % 2 == 0;
            bool diagonal = x == y || x == kScriptIconSize - 1 - y;
            pixels[y * pitch + x] = edge && dash ? frame : diagonal ? cross : clear;
        }
    }

    MakeFolders(ParentFolder(file));
    bool ok = IMG_SavePNG(surface, file.c_str()) == 0;
    SDL_FreeSurface(surface);
    return ok;
}

bool SetScriptIconFolder(ScriptIcon& icon, const std::string& tools)
{
    FreeScriptIcon(icon);
    icon.file = tools.empty() ? "" : JoinPath(tools, "blit-editor/icons/script.png");
    return !icon.file.empty() && !FileExists(icon.file) && WriteBlankIcon(icon.file);
}

GLuint GetScriptIcon(ScriptIcon& icon, double now)
{
    if (icon.file.empty())
    {
        return 0;
    }

    // you're painting it: look at the file once a second and reload it when it was saved
    if (icon.checked >= 0.0 && now - icon.checked < 1.0)
    {
        return icon.id;
    }

    icon.checked = now;
    long long stamp = FileStamp(icon.file);

    if (stamp != icon.stamp || (!icon.id && stamp))
    {
        if (icon.id) glDeleteTextures(1, &icon.id);

        int width = 0, height = 0;
        icon.id = stamp ? LoadImageTexture(icon.file, width, height) : 0;
        icon.stamp = stamp;
    }

    return icon.id;
}

void FreeScriptIcon(ScriptIcon& icon)
{
    if (icon.id) glDeleteTextures(1, &icon.id);
    icon.id = 0;
    icon.stamp = 0;
    icon.checked = -1.0;
}
