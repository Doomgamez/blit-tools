#pragma once

// running the engine's tools (cmake, blitscriptc, blitlevel, blitfs_pack) and the game, with their output read
// line by line. on Windows nothing opens a console window

#include <atomic>
#include <functional>
#include <string>
#include <vector>

struct ProcessRequest
{
    std::string program;                // a full path, or a name found on PATH
    std::vector<std::string> args;
    std::string folder;                 // working folder, the editor's when empty
    std::string path;                   // PATH for the program, the editor's when empty
};

// set while the process runs so another thread can stop it with StopProcess
struct ProcessHandle
{
    std::atomic<void*> native{ nullptr };
    std::atomic<bool> stopped{ false };
};

// waits for it to finish, onLine gets stdout and stderr as they come. the exit code, -1 if it couldn't start
// (and why in onLine)
int RunProcess(const ProcessRequest& request, const std::function<void(const std::string&)>& onLine,
               ProcessHandle* handle = nullptr);

void StopProcess(ProcessHandle& handle);

// the PATH variable of the editor itself
std::string CurrentPath();

// one argument quoted the way the Windows C runtime splits a command line
std::string QuoteArgument(const std::string& arg);
std::string CommandLine(const ProcessRequest& request);

#ifdef _WIN32
// UTF-8 to UTF-16 for the Windows API
std::wstring Widen(const std::string& text);
std::string Narrow(const std::wstring& text);
// the editor's environment with PATH replaced, as a block for CreateProcessW
std::wstring EnvironmentWithPath(const std::string& path);
#endif
