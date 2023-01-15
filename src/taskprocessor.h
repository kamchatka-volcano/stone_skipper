#pragma once
#include "task.h"
#include <asyncgi/asyncgi.h>
#include <utility>

namespace stone_skipper {
struct Task;
class ProcessLauncher;

enum class TaskLaunchMode {
    WaitingForResult,
    Detached
};

template<TaskLaunchMode>
struct TaskProcessor {
    explicit TaskProcessor(Task);
    void operator()(const asyncgi::RouteParameters<>&, const asyncgi::Request&, asyncgi::Response&);

private:
    std::optional<ProcessCfg> makeProcessCfg(
            const asyncgi::RouteParameters<>&,
            const asyncgi::Request&,
            asyncgi::Response&);

private:
    Task task_;
};

} //namespace stone_skipper