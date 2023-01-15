#pragma once
#include "path_utils.h"
#include "paths.h"
#include <figcone/config.h>
#include <sfun/string_utils.h>
#include <filesystem>
#include <string>
#include <vector>

namespace stone_skipper {

inline std::string defaultShellCommand()
{
#ifdef _WIN32
    return "cmd.exe /c";
#else
    return "sh -c -e";
#endif
}

struct StartsWithSlash {
    void operator()(const std::string& str)
    {
        if (!sfun::startsWith(str, "/"))
            throw figcone::ValidationError{"must start with '/'"};
    }
};

struct TaskIsValid {
    template<typename TTaskCfg>
    void operator()(const TTaskCfg& task)
    {
        if (task.command.empty() && task.process.empty())
            throw figcone::ValidationError{"a task must have 'command' or 'process' parameter set"};
        if (!task.command.empty() && !task.process.empty())
            throw figcone::ValidationError{"a task can't have both 'command' and 'process' parameters set"};
    }
};

struct AllTasksAreValid {
    template<typename TTaskCfg>
    void operator()(const std::vector<TTaskCfg>& taskList)
    {
        std::for_each(taskList.begin(), taskList.end(), TaskIsValid{});
    }
};

struct TaskConfig : figcone::Config {
    FIGCONE_PARAM(route, std::string).ensure<StartsWithSlash>();
    FIGCONE_PARAM(command, std::string)();
    FIGCONE_PARAM(process, std::string)();
    FIGCONE_PARAM(workingDir, std::filesystem::path)(homePath());
};

struct Config : figcone::Config {
    FIGCONE_PARAM(shell, std::string)(defaultShellCommand());
    FIGCONE_NODELIST(tasks, std::vector<TaskConfig>).ensure<AllTasksAreValid>();
};
} //namespace stone_skipper
