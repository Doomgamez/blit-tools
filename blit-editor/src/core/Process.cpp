#include "core/Process.h"

#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

std::string QuoteArgument(const std::string& arg)
{
    if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos)
    {
        return arg;
    }

    std::string out = "\"";
    size_t slashes = 0;

    for (char c : arg)
    {
        if (c == '\\')
        {
            slashes++;
            continue;
        }

        // backslashes before a quote are doubled, the quote escaped
        out.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
        slashes = 0;
        out += c;
    }

    out.append(slashes * 2, '\\');
    return out + "\"";
}

std::string CommandLine(const ProcessRequest& request)
{
    std::string line = QuoteArgument(request.program);

    for (const std::string& arg : request.args)
    {
        line += " " + QuoteArgument(arg);
    }

    return line;
}

std::string CurrentPath()
{
    const char* path = std::getenv("PATH");
    return path ? path : "";
}

#ifdef _WIN32

std::wstring Widen(const std::string& text)
{
    if (text.empty())
    {
        return L"";
    }

    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    std::wstring out((size_t)length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &out[0], length);
    return out;
}

std::string Narrow(const std::wstring& text)
{
    if (text.empty())
    {
        return "";
    }

    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &out[0], length, nullptr, nullptr);
    return out;
}

std::wstring EnvironmentWithPath(const std::string& path)
{
    std::wstring block;
    wchar_t* current = GetEnvironmentStringsW();

    for (const wchar_t* entry = current; *entry; entry += wcslen(entry) + 1)
    {
        if (_wcsnicmp(entry, L"PATH=", 5) != 0)
        {
            block += entry;
            block += L'\0';
        }
    }

    FreeEnvironmentStringsW(current);
    block += L"PATH=" + Widen(path);
    block += L'\0';
    block += L'\0';
    return block;
}

int RunProcess(const ProcessRequest& request, const std::function<void(const std::string&)>& onLine, ProcessHandle* handle)
{
    SECURITY_ATTRIBUTES security = { sizeof(security), nullptr, TRUE };
    HANDLE readPipe = nullptr, writePipe = nullptr;

    if (!CreatePipe(&readPipe, &writePipe, &security, 0))
    {
        onLine("couldn't make a pipe for " + request.program);
        return -1;
    }

    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION process = {};
    std::wstring command = Widen(CommandLine(request));
    std::wstring folder = Widen(request.folder);
    std::wstring environment = request.path.empty() ? L"" : EnvironmentWithPath(request.path);

    // CreateProcessW looks the program up on the editor's own PATH, so a new PATH for it goes in first
    std::string oldPath = CurrentPath();
    if (!request.path.empty())
    {
        SetEnvironmentVariableW(L"PATH", Widen(request.path).c_str());
    }

    BOOL started = CreateProcessW(nullptr, &command[0], nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                                  environment.empty() ? nullptr : (void*)environment.c_str(),
                                  folder.empty() ? nullptr : folder.c_str(), &startup, &process);

    if (!request.path.empty())
    {
        SetEnvironmentVariableW(L"PATH", Widen(oldPath).c_str());
    }

    CloseHandle(writePipe);

    if (!started)
    {
        CloseHandle(readPipe);
        onLine("couldn't start " + request.program + " (error " + std::to_string(GetLastError()) + ")");
        return -1;
    }

    if (handle)
    {
        handle->native = process.hProcess;
    }

    std::string pending;
    char buffer[4096];
    DWORD got = 0;

    while (ReadFile(readPipe, buffer, sizeof(buffer), &got, nullptr) && got > 0)
    {
        pending.append(buffer, got);
        size_t newline;

        while ((newline = pending.find('\n')) != std::string::npos)
        {
            std::string line = pending.substr(0, newline);

            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            onLine(line);
            pending.erase(0, newline + 1);
        }
    }

    if (!pending.empty())
    {
        onLine(pending);
    }

    WaitForSingleObject(process.hProcess, INFINITE);

    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);

    if (handle)
    {
        handle->native = nullptr;
    }

    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    CloseHandle(readPipe);
    return (int)code;
}

void StopProcess(ProcessHandle& handle)
{
    void* native = handle.native.load();
    handle.stopped = true;

    if (native)
    {
        TerminateProcess((HANDLE)native, 1);
    }
}

#else

#include <cstdio>

int RunProcess(const ProcessRequest& request, const std::function<void(const std::string&)>& onLine, ProcessHandle*)
{
    std::string command = (request.folder.empty() ? "" : "cd " + QuoteArgument(request.folder) + " && ") +
                          (request.path.empty() ? "" : "PATH=" + QuoteArgument(request.path) + " ") +
                          CommandLine(request) + " 2>&1";
    FILE* pipe = popen(command.c_str(), "r");

    if (!pipe)
    {
        onLine("couldn't start " + request.program);
        return -1;
    }

    char line[4096];

    while (fgets(line, sizeof(line), pipe))
    {
        std::string text = line;

        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        {
            text.pop_back();
        }

        onLine(text);
    }

    int status = pclose(pipe);
    return status;
}

void StopProcess(ProcessHandle& handle)
{
    handle.stopped = true;
}

#endif
