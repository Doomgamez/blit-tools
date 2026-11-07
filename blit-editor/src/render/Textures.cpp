#include "render/Textures.h"

#include <algorithm>
#include <cstring>
#include <set>

#include <SDL.h>
#include <SDL_image.h>

#include "core/Files.h"

GLuint UploadSurface(SDL_Surface* surface, int& width, int& height)
{
    if (!surface)
    {
        return 0;
    }

    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(surface);

    if (!rgba)
    {
        return 0;
    }

    width = rgba->w;
    height = rgba->h;

    // tightly packed rows for glTexImage2D
    std::vector<unsigned char> pixels((size_t)width * height * 4);
    for (int y = 0; y < height; y++)
    {
        memcpy(&pixels[(size_t)y * width * 4], (unsigned char*)rgba->pixels + (size_t)y * rgba->pitch, (size_t)width * 4);
    }

    SDL_FreeSurface(rgba);
    return MakeTexture(pixels.data(), width, height, false);
}

GLuint LoadImageTexture(const std::string& path, int& width, int& height)
{
    return UploadSurface(IMG_Load(path.c_str()), width, height);
}

GLuint LoadImageMemory(const void* data, size_t size, int& width, int& height)
{
    return UploadSurface(IMG_Load_RW(SDL_RWFromConstMem(data, (int)size), 1), width, height);
}

void InitTextures(TextureCache& cache)
{
    unsigned char white[4] = { 255, 255, 255, 255 };
    unsigned char missing[8 * 8 * 4];

    for (int i = 0; i < 64; i++)
    {
        bool dark = ((i % 8) / 4 + (i / 8) / 4) % 2 == 0;
        missing[i * 4 + 0] = dark ? 60 : 255;
        missing[i * 4 + 1] = dark ? 0 : 0;
        missing[i * 4 + 2] = dark ? 60 : 255;
        missing[i * 4 + 3] = 255;
    }

    cache.white = MakeTexture(white, 1, 1, false);
    cache.missing = MakeTexture(missing, 8, 8, false);
}

void FreeLoaded(TextureCache& cache)
{
    for (auto& entry : cache.loaded)
    {
        if (entry.second.found && entry.second.id)
        {
            glDeleteTextures(1, &entry.second.id);
        }
    }

    cache.loaded.clear();
}

void SetTextureFolders(TextureCache& cache, const std::vector<std::string>& folders)
{
    FreeLoaded(cache);
    cache.folders = folders;
    RefreshTextureNames(cache);
}

bool IsDetailMap(const std::string& stem)
{
    auto ends = [&](const char* suffix)
    {
        size_t length = strlen(suffix);
        return stem.size() > length && stem.compare(stem.size() - length, length, suffix) == 0;
    };

    return ends("_normal") || ends("_rough");
}

void RefreshTextureNames(TextureCache& cache)
{
    std::set<std::string> names;

    for (const std::string& folder : cache.folders)
    {
        for (const std::string& file : ListFiles(folder, { ".png" }, false))
        {
            std::string stem = FileStem(file);

            if (!IsDetailMap(stem))
            {
                names.insert(stem);
            }
        }
    }

    cache.names.assign(names.begin(), names.end());
}

std::string FindTextureFile(const TextureCache& cache, const std::string& name)
{
    for (const std::string& folder : cache.folders)
    {
        std::string file = JoinPath(folder, name + ".png");

        if (FileExists(file))
        {
            return file;
        }
    }

    return "";
}

const TextureInfo& GetTexture(TextureCache& cache, const std::string& name)
{
    auto found = cache.loaded.find(name);

    if (found != cache.loaded.end())
    {
        return found->second;
    }

    TextureInfo info;
    std::string file = FindTextureFile(cache, name);

    if (!file.empty())
    {
        info.id = LoadImageTexture(file, info.width, info.height);
        info.found = info.id != 0;
    }

    if (!info.found)
    {
        info.id = cache.missing;
        info.width = info.height = 64;
    }

    return cache.loaded[name] = info;
}

void FreeTextures(TextureCache& cache)
{
    FreeLoaded(cache);
    glDeleteTextures(1, &cache.white);
    glDeleteTextures(1, &cache.missing);
}
