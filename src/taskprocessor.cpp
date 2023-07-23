#include "taskprocessor.h"
#include "processlauncher.h"
#include <fmt/format.h>
#include <sfun/contract.h>
#include <sfun/functional.h>
#include <sfun/string_utils.h>
#include <sfun/utility.h>
#include <spdlog/spdlog.h>
#include <optional>
#include <utility>
#include <variant>

namespace stone_skipper {
template<TaskLaunchMode launchMode>
TaskProcessor<launchMode>::TaskProcessor(Task task)
    : task_{std::move(task)}
{
}

namespace {

auto makeProcessHandler(const ProcessCfg& taskProcess, asyncgi::Response& response, const asyncgi::TaskContext& ctx)
{
    return [taskProcess, response, ctx](const ProcessResult& result) mutable
    {
        if (result.exitCode == 0) {
            spdlog::info("The command '{}' was completed succesfully", taskProcess.command);
            response.send(asyncgi::http::ResponseStatus::_200_Ok, result.output);
        }
        else {
            spdlog::info("The command '{}' exited with an error code {}", taskProcess.command, result.exitCode);
            response.send(asyncgi::http::ResponseStatus::_200_Ok, result.output + "\n" + result.errorOutput);
        }
    };
}

auto makeLogProcessHandler(const ProcessCfg& taskProcess)
{
    return [taskProcess](const ProcessResult& result) mutable
    {
        if (result.exitCode == 0) {
            spdlog::info("The command '{}' was completed succesfully", taskProcess.command);
        }
        else {
            spdlog::info("The command '{}' exited with an error code {}", taskProcess.command, result.exitCode);
        }
    };
}

void processTaskLaunch(const ProcessCfg& taskProcess, asyncgi::Response& response)
{
    spdlog::info("Launching the command '{}'", taskProcess.command);

    auto disp = asyncgi::AsioDispatcher{response};
    disp.postTask(
            [taskProcess, response](const asyncgi::TaskContext& ctx) mutable
            {
                try {
                    launchProcess(ctx.io(), taskProcess, makeProcessHandler(taskProcess, response, ctx));
                }
                catch (const std::runtime_error& err) {
                    spdlog::error("{}", err.what());
                    response.send(asyncgi::http::ResponseStatus::_424_Failed_Dependency, std::string{err.what()});
                }
            });
}

void processTaskLaunchDetached(const ProcessCfg& taskProcess, asyncgi::Response& response)
{
    auto disp = asyncgi::AsioDispatcher{response};
    disp.postTask(
            [taskProcess, response](const asyncgi::TaskContext& ctx) mutable
            {
                try {
                    launchProcess(ctx.io(), taskProcess, makeLogProcessHandler(taskProcess));
                    const auto infoMessage = fmt::format("The command '{}' was launched and detached.",taskProcess.command);
                    spdlog::info(infoMessage);
                    response.send(infoMessage);
                }
                catch (const std::runtime_error& err) {
                    spdlog::error("{}", err.what());
                    response.send(asyncgi::http::ResponseStatus::_424_Failed_Dependency, std::string{err.what()});
                }
            });
}

std::optional<std::string> paramFromRoute(
        const std::string& commandParamName,
        const std::vector<std::string>& regexRouteParams,
        asyncgi::RouteParameters<> routeParams)
{
    const auto it = std::ranges::find(regexRouteParams, commandParamName);
    if (it == regexRouteParams.end())
        return std::nullopt;
    const auto routeParamIndex = std::distance(regexRouteParams.begin(), it);
    sfun_contract_check(routeParamIndex < std::ssize(routeParams.value));
    return routeParams.value.at(routeParamIndex);
}

std::optional<std::string> paramFromQueries(const std::string& commandParamName, const asyncgi::Request& request)
{
    if (!request.hasQuery(commandParamName))
        return std::nullopt;
    return std::string{request.query(commandParamName)};
}

class ProcessCfgParametrizationError : public std::runtime_error {
public:
    ProcessCfgParametrizationError(std::string param)
        : std::runtime_error{"ProcessCfgParametrizationError"}
        , param_{std::move(param)}
    {
    }

    std::string message(std::string_view command) const
    {
        return fmt::format(
                "Couldn't launch the command '{}'. Request doesn't contain a parameter '{}'",
                command,
                param_);
    }

private:
    std::string param_;
};

ProcessCfg makeProcessCfg(
        const ProcessCfg& templateProcessCfg,
        const std::vector<std::string>& regexRouteParams,
        const asyncgi::RouteParameters<>& routeParams,
        const asyncgi::Request& request)
{
    auto processCfg = templateProcessCfg;
    for (const auto& commandParam : processCfg.commandParams) {
        if (auto routeParam = paramFromRoute(commandParam, regexRouteParams, routeParams))
            processCfg.command = sfun::replace(processCfg.command, "{{" + commandParam + "}}", routeParam.value());
        else if (auto queryParam = paramFromQueries(commandParam, request))
            processCfg.command = sfun::replace(processCfg.command, "{{" + commandParam + "}}", queryParam.value());
        else
            throw ProcessCfgParametrizationError{commandParam};
    }
    return processCfg;
}
} //namespace

template<TaskLaunchMode launchMode>
void TaskProcessor<launchMode>::operator()(
        const asyncgi::RouteParameters<>& routeParams,
        const asyncgi::Request& request,
        asyncgi::Response& response) const
{
    try {
        const auto taskProcess = makeProcessCfg(task_.get().process, task_.get().routeParams, routeParams, request);
        if constexpr (launchMode == TaskLaunchMode::WaitingForResult)
            processTaskLaunch(taskProcess, response);
        else
            processTaskLaunchDetached(taskProcess, response);
    }
    catch (const ProcessCfgParametrizationError& error) {
        const auto errorMessage = error.message(task_.get().process.command);
        spdlog::error(errorMessage);
        response.send(asyncgi::http::ResponseStatus::_422_Unprocessable_Entity, errorMessage);
    }
}

template struct TaskProcessor<TaskLaunchMode::Detached>;
template struct TaskProcessor<TaskLaunchMode::WaitingForResult>;


} //namespace stone_skipper
