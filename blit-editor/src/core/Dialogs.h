#pragma once

// the system's own folder and file pickers. an empty string when cancelled, or where there's no picker

#include <string>

std::string PickFolder(const std::string& title, const std::string& startFolder);
// filterName "Level", filterPattern "*.blvl"
std::string PickFile(const std::string& title, const std::string& startFolder, const std::string& filterName,
                     const std::string& filterPattern);

// opens a folder or file with what the system uses for it (Explorer, the default editor)
void OpenWithSystem(const std::string& path);

#ifdef _WIN32
// one IFileOpenDialog, picking folders or one kind of file
std::string RunOpenDialog(const std::string& title, const std::string& startFolder, bool folders,
                          const std::string& filterName, const std::string& filterPattern);
#endif
