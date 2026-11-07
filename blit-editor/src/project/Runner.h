#pragma once

// runs build steps on a thread of their own, so the editor keeps drawing while the engine builds or the game runs.
// what they print goes into a log the console panel shows

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/Process.h"

enum class LogKind { Info, Step, Error, Success, Output };

struct LogLine
{
    LogKind kind = LogKind::Info;
    std::string text;
};

struct BuildLog
{
    std::mutex mutex;
    std::vector<LogLine> lines;
    int version = 0;            // goes up with every line, so the console knows to scroll
};

void LogAdd(BuildLog& log, LogKind kind, const std::string& text);
void LogClear(BuildLog& log);
// a copy to draw from, without holding the lock while drawing
std::vector<LogLine> LogCopy(BuildLog& log, int& version);

// what a step gets: where to write and how to run programs
struct StepContext
{
    BuildLog* log = nullptr;
    std::string path;           // PATH for the tools
    ProcessHandle* process = nullptr;

    // runs a program, every line it prints goes to the log, true when it exits with 0
    bool Run(const std::string& program, const std::vector<std::string>& args, const std::string& folder);
};

struct BuildStep
{
    std::string title;
    std::function<bool(StepContext&)> run;
};

struct Runner
{
    BuildLog log;
    std::thread worker;
    std::atomic<bool> busy{ false };
    std::atomic<bool> succeeded{ false };
    std::atomic<int> finished{ 0 };     // goes up when a run ends, so the editor can react (reload, refresh)
    std::string title;                  // what's running, set before it starts
    std::mutex statusMutex;
    std::string status;                 // the current step's title
    ProcessHandle process;
};

// false when something is still running
bool StartSteps(Runner& runner, const std::string& title, std::vector<BuildStep> steps, const std::string& path);
void StopSteps(Runner& runner);
std::string RunnerStatus(Runner& runner);
void SetStatus(Runner& runner, const std::string& status);
// waits for the thread, when the editor closes
void JoinRunner(Runner& runner);
