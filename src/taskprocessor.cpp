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

template<TaskLaunchMode taskLaunchMode>
TaskProcessor<taskLaunchMode>::TaskProcessor(Task task)
    : task_{std::move(task)}
{
}

namespace {

void processDetachedTaskLaunch(const ProcessCfg& taskProcess, asyncgi::Response& response)
{
    if (!launchProcessDetached(taskProcess)) {
        const auto errorMessage =
                fmt::format("Couldn't find the executable of the command '{}' to launch it", taskProcess.command);
        spdlog::error(errorMessage);
        response.send(asyncgi::http::ResponseStatus::Code_424_Failed_Dependency, errorMessage);
        return;
    }
    const auto infoMessage = fmt::format(
            "The command '{}' was launched and detached. The state of the command is unknown.",
            taskProcess.command);
    spdlog::info(infoMessage);
    response.send(asyncgi::http::ResponseStatus::Code_200_Ok, infoMessage);
}

void processTaskLaunch(const ProcessCfg& taskProcess, asyncgi::Response& response)
{
    spdlog::info("Launching the command '{}'", taskProcess.command);
    auto result = launchProcess(taskProcess);
    if (!result.has_value()) {
        const auto errorMessage =
                fmt::format("Couldn't find the executable of the command '{}' to launch it", taskProcess.command);
        spdlog::error(errorMessage);
        response.send(asyncgi::http::ResponseStatus::Code_424_Failed_Dependency, errorMessage);
        return;
    }
    if (result.value().exitCode == 0) {
        spdlog::info("The command '{}' was completed succesfully", taskProcess.command);
        response.send(asyncgi::http::ResponseStatus::Code_200_Ok, result.value().output);
    }
    else {
        spdlog::info("The command '{}' exited with an error code {}", taskProcess.command, result.value().exitCode);
        response.send(asyncgi::http::ResponseStatus::Code_200_Ok, result.value().output);
    }
}

std::optional<std::string> paramFromRoute(
        const std::string& commandParamName,
        const std::vector<std::string>& regexRouteParams,
        asyncgi::RouteParameters<> routeParams)
{
    auto it = std::find(regexRouteParams.begin(), regexRouteParams.end(), commandParamName);
    if (it == regexRouteParams.end())
        return std::nullopt;
    auto routeParamIndex = std::distance(regexRouteParams.begin(), it);
    sfunContractCheck(routeParamIndex < sfun::ssize(routeParams.value));
    return routeParams.value.at(routeParamIndex);
}

std::optional<std::string> paramFromQueries(const std::string& commandParamName, const asyncgi::Request& request)
{
    if (!request.hasQuery(commandParamName))
        return std::nullopt;
    return std::string{request.query(commandParamName)};
}

struct ProcessCfgParametrizationError {
    std::string missingParam;
};

std::variant<ProcessCfg, ProcessCfgParametrizationError> makeProcessCfg(
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
            return ProcessCfgParametrizationError{commandParam};
    }
    return processCfg;
}
} //namespace

template<TaskLaunchMode taskLaunchMode>
void TaskProcessor<taskLaunchMode>::operator()(
        const asyncgi::RouteParameters<>& routeParams,
        const asyncgi::Request& request,
        asyncgi::Response& response) const
{
    auto taskProcessResult = makeProcessCfg(task_.process, task_.routeParams, routeParams, request);
    auto taskProcessResultVisitor = sfun::overloaded{
            [&](const ProcessCfgParametrizationError& error)
            {
                const auto errorMessage = fmt::format(
                        "Couldn't launch the command '{}'. Request doesn't contain a parameter '{}'",
                        task_.process.command,
                        error.missingParam);
                spdlog::error(errorMessage);
                response.send(asyncgi::http::ResponseStatus::Code_422_Unprocessable_Entity, errorMessage);
            },
            [&](const ProcessCfg& taskProcess)
            {
                if constexpr (taskLaunchMode == TaskLaunchMode::Detached)
                    processDetachedTaskLaunch(taskProcess, response);
                else
                    processTaskLaunch(taskProcess, response);
            }};
    std::visit(taskProcessResultVisitor, taskProcessResult);
}

template struct TaskProcessor<TaskLaunchMode::Detached>;
template struct TaskProcessor<TaskLaunchMode::WaitingForResult>;

} //namespace stone_skipper
