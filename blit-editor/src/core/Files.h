#pragma once

// files and folders, paths are UTF-8 with either slash

#include <filesystem>
#include <string>
#include <vector>

// UTF-8 text to std::filesystem and back (forward slashes)
std::filesystem::path ToPath(const std::string& path);
std::string FromPath(const std::filesystem::path& path);

bool ReadTextFile(const std::string& path, std::string& out);
// writes to path.tmp first and renames it, so a crash never leaves half a file
bool WriteTextFile(const std::string& path, const std::string& text);
bool CopyFileTo(const std::string& from, const std::string& to);
bool RemoveFile(const std::string& path);

bool FileExists(const std::string& path);
bool FolderExists(const std::string& path);
bool MakeFolders(const std::string& path);
// newer than: false when either is missing
bool IsNewer(const std::string& path, const std::string& than);

std::string JoinPath(const std::string& a, const std::string& b);
std::string FileName(const std::string& path);          // "levels/house.blvl" -> "house.blvl"
std::string FileStem(const std::string& path);          // "levels/house.blvl" -> "house"
std::string FileExtension(const std::string& path);     // ".blvl", lower case
std::string ParentFolder(const std::string& path);
// forward slashes, no trailing slash
std::string CleanPath(const std::string& path);
// path relative to root with forward slashes, path itself when it isn't inside root
std::string RelativePath(const std::string& path, const std::string& root);

// every file under folder (or only in it), sorted, whose extension is one of extensions (".png"), all when empty
std::vector<std::string> ListFiles(const std::string& folder, const std::vector<std::string>& extensions,
                                   bool recursive);

// %APPDATA%/BlitEditor, made if missing
std::string SettingsFolder();
// the folder blit-editor.exe is in
std::string ProgramFolder();
