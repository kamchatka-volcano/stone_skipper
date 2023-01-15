#pragma once
#include "processlauncher.h"
#include <asyncgi/asyncgi.h>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace stone_skipper {

struct TaskConfig;

struct Task {
    explicit Task(const TaskConfig&, const std::string& shellCmd);
    asyncgi::rx routeRegexp;
    std::vector<std::string> routeParams;
    ProcessCfg process;
    std::vector<std::string> commandParams;
};

} //namespace stone_skipper