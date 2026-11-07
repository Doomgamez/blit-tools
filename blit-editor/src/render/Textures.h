#pragma once

// the project's textures by name, the way the engine finds them: "brick" is brick.png in assets/textures, then
// anywhere under assets/, then the engine's own assets/textures. loaded once, on first use

#include <map>
#include <string>
#include <vector>

#include "render/Gpu.h"

struct TextureInfo
{
    GLuint id = 0;
    int width = 64;
    int height = 64;
    bool found = false;     // false: the checker stand-in, the build will warn about it
};

struct TextureCache
{
    std::vector<std::string> folders;           // searched in order
    std::map<std::string, TextureInfo> loaded;
    GLuint white = 0;                           // for untextured things
    GLuint missing = 0;                         // magenta checker

    // what the texture browser lists, without the detail maps (_normal, _rough)
    std::vector<std::string> names;
};

// empty folders and textures, makes the white and missing textures
void InitTextures(TextureCache& cache);
// new folders (another project): forgets what was loaded
void SetTextureFolders(TextureCache& cache, const std::vector<std::string>& folders);
// rescans the folders for names (after files were added)
void RefreshTextureNames(TextureCache& cache);
const TextureInfo& GetTexture(TextureCache& cache, const std::string& name);
// the file the name stands for, "" when there's none
std::string FindTextureFile(const TextureCache& cache, const std::string& name);
// any image file (a sprite, a model's image) as a texture, 0 when it can't be read
GLuint LoadImageTexture(const std::string& path, int& width, int& height);
GLuint LoadImageMemory(const void* data, size_t size, int& width, int& height);
void FreeTextures(TextureCache& cache);

struct SDL_Surface;
// any surface as an RGBA texture, the surface is freed
GLuint UploadSurface(SDL_Surface* surface, int& width, int& height);
void FreeLoaded(TextureCache& cache);
// brick_normal and brick_rough go with brick, the browser doesn't list them
bool IsDetailMap(const std::string& stem);
