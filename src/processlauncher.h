#pragma once
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace boost::asio {
class io_context;
}

namespace stone_skipper {

struct ProcessCfg {
    std::string command;
    std::vector<std::string> commandParams;
    std::optional<std::string> shellCommand;
    std::optional<std::filesystem::path> workingDir;
};

struct ProcessResult {
    int exitCode;
    std::string output;
    std::string errorOutput;
};

void launchProcess(
        boost::asio::io_context&,
        const ProcessCfg&,
        const std::function<void(const ProcessResult&)>& resultHandler);
void launchProcessDetached(const ProcessCfg&);

} //namespace stone_skipper
