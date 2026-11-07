#include "project/Runner.h"

#include <chrono>
#include <cstdio>

void LogAdd(BuildLog& log, LogKind kind, const std::string& text)
{
    std::lock_guard<std::mutex> lock(log.mutex);
    log.lines.push_back({ kind, text });

    // a game that logs every frame shouldn't eat all the memory
    if (log.lines.size() > 20000)
    {
        log.lines.erase(log.lines.begin(), log.lines.begin() + 5000);
    }

    log.version++;
}

void LogClear(BuildLog& log)
{
    std::lock_guard<std::mutex> lock(log.mutex);
    log.lines.clear();
    log.version++;
}

std::vector<LogLine> LogCopy(BuildLog& log, int& version)
{
    std::lock_guard<std::mutex> lock(log.mutex);
    version = log.version;
    return log.lines;
}

bool StepContext::Run(const std::string& program, const std::vector<std::string>& args, const std::string& folder)
{
    ProcessRequest request;
    request.program = program;
    request.args = args;
    request.folder = folder;
    request.path = path;

    int code = RunProcess(request, [&](const std::string& line)
    {
        bool bad = line.find("error") != std::string::npos || line.find("Error") != std::string::npos;
        LogAdd(*log, bad ? LogKind::Error : LogKind::Output, line);
    }, process);

    if (code != 0 && !(process && process->stopped))
    {
        LogAdd(*log, LogKind::Error, program + " stopped with exit code " + std::to_string(code));
    }

    return code == 0;
}

void SetStatus(Runner& runner, const std::string& status)
{
    std::lock_guard<std::mutex> lock(runner.statusMutex);
    runner.status = status;
}

std::string RunnerStatus(Runner& runner)
{
    std::lock_guard<std::mutex> lock(runner.statusMutex);
    return runner.status;
}

void JoinRunner(Runner& runner)
{
    if (runner.worker.joinable())
    {
        runner.worker.join();
    }
}

bool StartSteps(Runner& runner, const std::string& title, std::vector<BuildStep> steps, const std::string& path)
{
    if (runner.busy)
    {
        return false;
    }

    JoinRunner(runner);
    runner.busy = true;
    runner.succeeded = false;
    runner.process.stopped = false;
    runner.title = title;
    LogAdd(runner.log, LogKind::Step, "== " + title);

    runner.worker = std::thread([&runner, steps = std::move(steps), path]()
    {
        auto started = std::chrono::steady_clock::now();
        StepContext context;
        context.log = &runner.log;
        context.path = path;
        context.process = &runner.process;
        bool ok = true;

        for (const BuildStep& step : steps)
        {
            if (runner.process.stopped)
            {
                ok = false;
                LogAdd(runner.log, LogKind::Error, "Stopped");
                break;
            }

            SetStatus(runner, step.title);
            LogAdd(runner.log, LogKind::Step, step.title);

            if (!step.run(context))
            {
                ok = false;

                if (!runner.process.stopped)
                {
                    LogAdd(runner.log, LogKind::Error, step.title + " failed");
                }

                break;
            }
        }

        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        char took[64];
        std::snprintf(took, sizeof(took), " (%.1f s)", seconds);
        LogAdd(runner.log, ok ? LogKind::Success : LogKind::Error, (ok ? "Done" : "Failed") + std::string(took));

        SetStatus(runner, "");
        runner.succeeded = ok;
        runner.finished++;
        runner.busy = false;
    });

    return true;
}

void StopSteps(Runner& runner)
{
    StopProcess(runner.process);
}
