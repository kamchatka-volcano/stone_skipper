#include "task.h"
#include "config.h"
#include <regex>

namespace stone_skipper {

namespace {
asyncgi::rx readRouteRegex(std::string input)
{
    const auto paramRegex = std::regex(R"(\{\{.+\}\})");
    return asyncgi::rx{std::regex_replace(input, paramRegex, "(.+)")};
}

std::string withoutBrackets(const std::string& str)
{
    return str.substr(2, str.size() - 4);
}

std::vector<std::string> readParams(std::string input)
{
    const auto paramRegex = std::regex(R"(\{\{.+\}\})");
    auto match = std::smatch{};
    auto params = std::vector<std::string>{};
    while (std::regex_search(input, match, paramRegex)) {
        params.emplace_back(withoutBrackets(match.str()));
        input = match.suffix();
    }
    return params;
}

ProcessCfg makeProcessCfg(const TaskConfig& cfg, const std::string& shellCmd)
{
    auto result = ProcessCfg{};
    result.workingDir = cfg.workingDir;
    if (!cfg.command.empty()) {
        result.command = cfg.command;
        result.shellCommand = shellCmd;
    }
    else {
        result.command = cfg.process;
    }
    result.commandParams = readParams(result.command);
    return result;
}

} //namespace

Task::Task(const TaskConfig& cfg, const std::string& shellCmd)
    : routeRegexp{readRouteRegex(cfg.route)}
    , routeParams{readParams(cfg.route)}
    , process{makeProcessCfg(cfg, shellCmd)}
{
}

} //namespace stone_skipper