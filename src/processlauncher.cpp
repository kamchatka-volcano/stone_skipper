#include "processlauncher.h"
#include <sfun/string_utils.h>
#include <boost/process.hpp>
#include <string_view>
#include <utility>
#include <vector>

namespace proc = boost::process;
namespace fs = std::filesystem;

namespace stone_skipper {

namespace {

std::vector<std::string_view> splitCommand(std::string_view str)
{
    if (str.empty())
        return std::vector<std::string_view>{str};

    auto result = std::vector<std::string_view>{};
    auto pos = std::size_t{0};
    auto partPos = std::string_view::npos;
    auto addCommandPart = [&]()
    {
        result.emplace_back(std::string_view{std::next(str.data(), partPos), pos - partPos});
        partPos = std::string_view::npos;
    };

    auto insideString = false;
    for (; pos < str.size(); ++pos) {
        if (!insideString && sfun::isspace(str.at(pos))) {
            if (partPos != std::string_view::npos)
                addCommandPart();
            continue;
        }
        if (str.at(pos) == '"') {
            if (insideString)
                addCommandPart();
            insideString = !insideString;
            continue;
        }
        if (!sfun::isspace(str.at(pos)) && partPos == std::string_view::npos)
            partPos = pos;
    }
    if (insideString)
        return {};

    if (partPos != std::string_view::npos)
        addCommandPart();

    return result;
}

template<typename TCommand, typename TCommandArgs, typename... TProcessArgs>
auto makeProcess(
        const TCommand& cmd,
        const TCommandArgs& cmdArgs,
        std::optional<std::filesystem::path> workingDir,
        TProcessArgs&&... processArgs)
{
    if (workingDir.has_value())
        return proc::child{
                cmd,
                proc::args(cmdArgs),
                proc::start_dir = workingDir.value().string(),
                std::forward<TProcessArgs>(processArgs)...};

    return proc::child{cmd, proc::args(cmdArgs), std::forward<TProcessArgs>(processArgs)...};
}
} //namespace

bool launchProcessDetached(const ProcessCfg& processCfg)
{
    auto env = boost::this_process::environment();
    if (processCfg.workingDir)
        env["PATH"] += processCfg.workingDir.value().string();

    auto cmdParts = processCfg.shellCommand ? splitCommand(processCfg.shellCommand.value())
                                            : splitCommand(processCfg.command);
    auto cmd = proc::search_path(std::string{cmdParts[0]});
    if (cmd.empty())
        return false;

    cmdParts.erase(cmdParts.begin());
    if (processCfg.shellCommand)
        cmdParts.push_back(processCfg.command);

    auto process = makeProcess(
            cmd,
            cmdParts,
            processCfg.workingDir,
            env,
            proc::std_out > proc::null,
            proc::std_err > proc::null);
    process.detach();
    return true;
}

std::optional<ProcessResult> launchProcess(const ProcessCfg& processCfg)
{
    auto env = boost::this_process::environment();
    if (processCfg.workingDir)
        env["PATH"] += processCfg.workingDir.value().string();

    auto cmdParts = processCfg.shellCommand ? splitCommand(processCfg.shellCommand.value())
                                            : splitCommand(processCfg.command);
    auto cmd = proc::search_path(std::string{cmdParts[0]});
    if (cmd.empty())
        return std::nullopt;

    cmdParts.erase(cmdParts.begin());
    if (processCfg.shellCommand)
        cmdParts.push_back(processCfg.command);

    auto stream = proc::ipstream{};
    auto process =
            makeProcess(cmd, cmdParts, processCfg.workingDir, env, proc::std_out > stream, proc::std_err > proc::null);

    auto processOutput = std::string{};
    auto line = std::string{};
    while (process.running() && std::getline(stream, line) && !line.empty())
        processOutput += line;
    process.wait();

    return ProcessResult{process.exit_code(), std::move(processOutput)};
}

} //namespace stone_skipper