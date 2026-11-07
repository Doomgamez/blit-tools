#include "core/Files.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <SDL.h>

namespace fs = std::filesystem;

fs::path ToPath(const std::string& path)
{
    return fs::u8path(path);
}

std::string FromPath(const fs::path& path)
{
    std::string text = path.u8string();
    std::replace(text.begin(), text.end(), '\\', '/');
    return text;
}

bool ReadTextFile(const std::string& path, std::string& out)
{
    std::ifstream file(ToPath(path), std::ios::binary);

    if (!file)
    {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

bool WriteTextFile(const std::string& path, const std::string& text)
{
    std::string temporary = path + ".tmp";
    std::error_code error;

    {
        std::ofstream file(ToPath(temporary), std::ios::binary | std::ios::trunc);

        if (!file)
        {
            return false;
        }

        file << text;

        if (!file)
        {
            return false;
        }
    }

    fs::rename(ToPath(temporary), ToPath(path), error);
    return !error;
}

bool CopyFileTo(const std::string& from, const std::string& to)
{
    std::error_code error;
    MakeFolders(ParentFolder(to));
    fs::copy_file(ToPath(from), ToPath(to), fs::copy_options::overwrite_existing, error);
    return !error;
}

bool RemoveFile(const std::string& path)
{
    std::error_code error;
    fs::remove(ToPath(path), error);
    return !error;
}

bool FileExists(const std::string& path)
{
    std::error_code error;
    return !path.empty() && fs::is_regular_file(ToPath(path), error);
}

bool FolderExists(const std::string& path)
{
    std::error_code error;
    return !path.empty() && fs::is_directory(ToPath(path), error);
}

bool MakeFolders(const std::string& path)
{
    std::error_code error;

    if (path.empty() || FolderExists(path))
    {
        return true;
    }

    fs::create_directories(ToPath(path), error);
    return !error;
}

bool IsNewer(const std::string& path, const std::string& than)
{
    std::error_code a, b;
    auto first = fs::last_write_time(ToPath(path), a);
    auto second = fs::last_write_time(ToPath(than), b);
    return !a && !b && first > second;
}

std::string JoinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    return CleanPath(a) + "/" + b;
}

std::string FileName(const std::string& path)
{
    std::string clean = CleanPath(path);
    size_t slash = clean.find_last_of('/');
    return slash == std::string::npos ? clean : clean.substr(slash + 1);
}

std::string FileStem(const std::string& path)
{
    std::string name = FileName(path);
    size_t dot = name.find('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

std::string FileExtension(const std::string& path)
{
    std::string name = FileName(path);
    size_t dot = name.find_last_of('.');
    std::string extension = dot == std::string::npos ? "" : name.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return extension;
}

std::string ParentFolder(const std::string& path)
{
    std::string clean = CleanPath(path);
    size_t slash = clean.find_last_of('/');
    return slash == std::string::npos ? "" : clean.substr(0, slash);
}

std::string CleanPath(const std::string& path)
{
    std::string out = path;
    std::replace(out.begin(), out.end(), '\\', '/');

    while (out.size() > 1 && out.back() == '/' && !(out.size() == 3 && out[1] == ':'))
    {
        out.pop_back();
    }

    return out;
}

std::string RelativePath(const std::string& path, const std::string& root)
{
    std::string p = CleanPath(path), r = CleanPath(root) + "/";

    if (p.size() > r.size() && std::equal(r.begin(), r.end(), p.begin(),
        [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); }))
    {
        return p.substr(r.size());
    }

    return p;
}

std::vector<std::string> ListFiles(const std::string& folder, const std::vector<std::string>& extensions, bool recursive)
{
    std::vector<std::string> out;
    std::error_code error;

    if (!FolderExists(folder))
    {
        return out;
    }

    auto take = [&](const fs::directory_entry& entry)
    {
        if (!entry.is_regular_file(error))
        {
            return;
        }

        std::string path = FromPath(entry.path());

        if (extensions.empty() || std::find(extensions.begin(), extensions.end(), FileExtension(path)) != extensions.end())
        {
            out.push_back(path);
        }
    };

    if (recursive)
    {
        for (const auto& entry : fs::recursive_directory_iterator(ToPath(folder), error)) take(entry);
    }
    else
    {
        for (const auto& entry : fs::directory_iterator(ToPath(folder), error)) take(entry);
    }

    std::sort(out.begin(), out.end());
    return out;
}

std::string SettingsFolder()
{
    char* base = SDL_GetPrefPath("Blit", "BlitEditor");
    std::string folder = base ? CleanPath(base) : ".";
    SDL_free(base);
    MakeFolders(folder);
    return folder;
}

std::string ProgramFolder()
{
    char* base = SDL_GetBasePath();
    std::string folder = base ? CleanPath(base) : ".";
    SDL_free(base);
    return folder;
}
