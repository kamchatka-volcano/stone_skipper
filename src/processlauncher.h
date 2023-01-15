#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace stone_skipper {

struct ProcessCfg {
    std::string command;
    std::optional<std::string> shellCommand;
    std::optional<std::filesystem::path> workingDir;
};

struct ProcessResult {
    int exitCode;
    std::string output;
};

std::optional<ProcessResult> launchProcess(const ProcessCfg&);
bool launchProcessDetached(const ProcessCfg&);

} //namespace stone_skipper
