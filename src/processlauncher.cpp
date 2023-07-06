#include "processlauncher.h"
#include "errors.h"
#include "utils.h"
#include <fmt/format.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>
#include <sfun/path.h>
#include <sfun/string_utils.h>
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/process/extend.hpp>
#include <string_view>
#include <utility>
#include <vector>

namespace proc = boost::process;
namespace views = ranges::views;
namespace fs = std::filesystem;

namespace stone_skipper {

namespace {

auto osArgs(const std::vector<std::string>& args)
{
#ifndef _WIN32
    return args;
#else
    const auto toWString = [](const std::string& arg)
    {
        return sfun::to_wstring(arg);
    };
    return args | views::transform(toWString) | ranges::to<std::vector>;
#endif
}

struct ProcessHandler : boost::process::extend::async_handler {
    ProcessHandler(
            std::shared_ptr<std::future<std::string>> stdOut,
            std::shared_ptr<std::future<std::string>> stdErr,
            std::function<void(const ProcessResult&)> resultHandler)
        : stdOut_{std::move(stdOut)}
        , stdErr_{std::move(stdErr)}
        , resultHandler_{std::move(resultHandler)}
    {
    }

    template<typename ExecutorType>
    std::function<void(int, std::error_code const&)> on_exit_handler(ExecutorType&)
    {
        return [out = std::move(stdOut_),
                err = std::move(stdErr_),
                resultHandler = std::move(resultHandler_)](int exitCode, std::error_code const& ec)
        {
            if (ec) {
                resultHandler({.exitCode = exitCode, .output = ec.message(), .errorOutput = ""});
                return;
            }
            resultHandler({.exitCode = exitCode, .output = out->get(), .errorOutput = err->get()});
        };
    }
    std::shared_ptr<std::future<std::string>> stdOut_;
    std::shared_ptr<std::future<std::string>> stdErr_;
    std::function<void(const ProcessResult&)> resultHandler_;
};

void startProcess(
        boost::asio::io_context& io,
        const boost::filesystem::path& cmd,
        const std::vector<std::string>& cmdArgs,
        const boost::filesystem::path& workingDir,
        std::function<void(const ProcessResult&)> resultHandler)
{
    auto stdoutData = std::make_shared<std::future<std::string>>();
    auto stderrData = std::make_shared<std::future<std::string>>();
    auto processHandler = ProcessHandler{stdoutData, stderrData, std::move(resultHandler)};
    auto process = proc::child{
            cmd,
            proc::args(osArgs(cmdArgs)),
            proc::start_dir = workingDir,
            proc::std_out > (*stdoutData),
            proc::std_err > (*stderrData),
            std::move(processHandler),
            io};
    process.detach();
}

std::tuple<std::string, std::vector<std::string>> parseShellCommand(
        const std::string& shellCommand,
        const std::string& command)
{
    if (shellCommand.find('\n') != std::string::npos)
        throw Error{fmt::format("Can't launch a command with a newline character: {}", shellCommand)};

    const auto shellCmdParts = splitCommand(shellCommand);
    if (shellCmdParts.empty())
        throw Error{"Can't launch the process with an empty command"};
    if (command.find('\n') != std::string::npos)
        throw Error{fmt::format("Can't launch a command with a newline character: {}", command)};

    const auto& shellExec = shellCmdParts.front();
    const auto shellArgs = shellCmdParts | views::drop(1);
    const auto args = views::concat(shellArgs, views::single(command)) | ranges::to<std::vector>;
    return std::tuple{shellExec, args};
}

std::tuple<std::string, std::vector<std::string>> parseCommand(const std::string& command)
{
    if (command.find('\n') != std::string::npos)
        throw Error{fmt::format("Can't launch a command with a newline character: {}", command)};

    auto cmdParts = splitCommand(command);
    if (cmdParts.empty())
        throw Error{"Can't launch the process with an empty command"};

    const auto& processExec = cmdParts.front();
    const auto args = cmdParts | views::drop(1) | ranges::to<std::vector>;
    return std::tuple{processExec, args};
}

} //namespace

void launchProcess(
        boost::asio::io_context& io,
        const ProcessCfg& processCfg,
        std::function<void(const ProcessResult&)> resultHandler)
{
    const auto [cmdName, cmdArgs] = processCfg.shellCommand.has_value()
            ? parseShellCommand(processCfg.shellCommand.value(), processCfg.command)
            : parseCommand(processCfg.command);

    const auto currentPath = boost::this_process::path();
    const auto workingDir = processCfg.workingDir.has_value()
            ? boost::filesystem::path(processCfg.workingDir.value().native())
            : boost::filesystem::path{sfun::make_path(".").native()};
    const auto path = views::concat(currentPath, views::single(workingDir)) | ranges::to<std::vector>();

    const auto cmd = proc::search_path(std::string{cmdName}, path);
    if (cmd.empty())
        throw Error{fmt::format("Couldn't find the executable of the command '{}'", cmdName)};

    startProcess(io, cmd, cmdArgs, workingDir, resultHandler);
}

} //namespace stone_skipper