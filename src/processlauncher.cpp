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

std::string readBuffer(boost::asio::streambuf& streambuf, sfun::ssize_t size)
{
    return {buffers_begin(streambuf.data()), buffers_begin(streambuf.data()) + size};
}

class Process : public std::enable_shared_from_this<Process> {
public:
    static void launch(
            boost::asio::io_context& io,
            const boost::filesystem::path& cmd,
            const std::vector<std::string>& cmdArgs,
            const boost::filesystem::path& workingDir,
            const std::function<void(const ProcessResult&)>& resultHandler)
    {
        auto process =std::shared_ptr<Process>{new Process{io}};
        process->launch(cmd, cmdArgs, workingDir, resultHandler);
    }

private:
    explicit Process(boost::asio::io_context& io)
        : io_{io}
        , stdOutPipe_{io}
        , stdErrPipe_{io}
    {
    }

    void launch(
            const boost::filesystem::path& cmd,
            const std::vector<std::string>& cmdArgs,
            const boost::filesystem::path& workingDir,
            const std::function<void(const ProcessResult&)>& resultHandler)
    {
        proc::async_system(
                io_,
                [self = shared_from_this(), resultHandler](const boost::system::error_code& ec, int exitCode)
                {
                    self->onExit(ec, exitCode, resultHandler);
                },
                cmd,
                proc::args(osArgs(cmdArgs)),
                proc::start_dir = workingDir,
                proc::std_out > stdOutPipe_,
                proc::std_err > stdErrPipe_);

        readOutput<&Process::stdOut_, &Process::stdOutBuffer_, &Process::stdOutPipe_>();
        readOutput<&Process::stdErr_, &Process::stdErrBuffer_, &Process::stdErrPipe_>();
    }

    void onExit(const std::error_code& ec, int exitCode, const std::function<void(const ProcessResult&)>& resultHandler)
    {
        if (ec) {
            resultHandler({.exitCode = exitCode, .output = stdOut_ + "\n" + ec.message(), .errorOutput = stdErr_});
            return;
        }
        resultHandler({.exitCode = exitCode, .output = stdOut_, .errorOutput = stdErr_});
    }

    template<auto outputStringPtr, auto outputBufferPtr, auto outputPipePtr>
    void readOutput()
    {
        auto& outBuffer = this->*outputBufferPtr;
        auto& outPipe = this->*outputPipePtr;

        boost::asio::async_read_until(
                outPipe,
                outBuffer,
                '\n',
                [self = shared_from_this()](const boost::system::error_code& ec, std::size_t bytesTransferred)
                {
                    self->readOutput<outputStringPtr, outputBufferPtr, outputPipePtr>(ec, bytesTransferred);
                });
    }

    template<auto outputStringPtr, auto outputBufferPtr, auto outputPipePtr>
    void readOutput(const boost::system::error_code& ec, std::size_t bytesTransferred)
    {
        auto& outString = this->*outputStringPtr;
        auto& outBuffer = this->*outputBufferPtr;
        if (!ec) {
            outString += readBuffer(outBuffer, bytesTransferred);
            outBuffer.consume(bytesTransferred);
            readOutput<outputStringPtr, outputBufferPtr, outputPipePtr>();
        }
        else if (ec == boost::asio::error::eof) {
            outString += readBuffer(outBuffer, std::ssize(outBuffer));
            outBuffer.consume(std::ssize(outBuffer));
        }
    }

    boost::asio::io_context& io_;
    boost::process::async_pipe stdOutPipe_;
    boost::process::async_pipe stdErrPipe_;
    boost::asio::streambuf stdOutBuffer_;
    boost::asio::streambuf stdErrBuffer_;
    std::string stdOut_;
    std::string stdErr_;
};

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
        const std::function<void(const ProcessResult&)>& resultHandler)
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

    Process::launch(io, cmd, cmdArgs, workingDir, resultHandler);
}

} //namespace stone_skipper