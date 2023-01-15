#include "taskprocessor.h"
#include "processlauncher.h"
#include <fmt/format.h>
#include <sfun/contract.h>
#include <sfun/string_utils.h>
#include <sfun/utility.h>
#include <spdlog/spdlog.h>
#include <optional>
#include <utility>

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
            "Command '{}' was launched and detached. The state of the command is unknown.",
            taskProcess.command);
    spdlog::info(infoMessage);
    response.send(asyncgi::http::ResponseStatus::Code_200_Ok, infoMessage);
}

void processTaskLaunch(const ProcessCfg& taskProcess, asyncgi::Response& response)
{
    auto result = launchProcess(taskProcess);
    if (!result.has_value()) {
        const auto errorMessage =
                fmt::format("Couldn't find the executable of the command '{}' to launch it", taskProcess.command);
        spdlog::error(errorMessage);
        response.send(asyncgi::http::ResponseStatus::Code_424_Failed_Dependency, errorMessage);
        return;
    }
    if (result.value().exitCode == 0) {
        spdlog::info("Command '{}' was completed succesfully", taskProcess.command);
        response.send(asyncgi::http::ResponseStatus::Code_200_Ok, result.value().output);
    }
    else {
        spdlog::info("Command '{}' exited with an error code {}", taskProcess.command, result.value().exitCode);
        response.send(asyncgi::http::ResponseStatus::Code_200_Ok, result.value().output);
    }
}

} //namespace

template<TaskLaunchMode taskLaunchMode>
void TaskProcessor<taskLaunchMode>::operator()(
        const asyncgi::RouteParameters<>& routeParams,
        const asyncgi::Request& request,
        asyncgi::Response& response)
{
    auto taskProcessResult = makeProcessCfg(routeParams, request, response);
    if (!taskProcessResult)
        return;
    auto& taskProcess = taskProcessResult.value();

    if constexpr (taskLaunchMode == TaskLaunchMode::Detached)
        processDetachedTaskLaunch(taskProcess, response);
    else
        processTaskLaunch(taskProcess, response);
}

namespace {
std::optional<std::string> paramFromRoute(
        const std::string& commandParamName,
        const Task& task,
        asyncgi::RouteParameters<> routeParams)
{
    auto it = std::find(task.routeParams.begin(), task.routeParams.end(), commandParamName);
    if (it == task.routeParams.end())
        return std::nullopt;
    auto routeParamIndex = std::distance(task.routeParams.begin(), it);
    sfunContractCheck(routeParamIndex < sfun::ssize(routeParams.value));
    return routeParams.value.at(routeParamIndex);
}

std::optional<std::string> paramFromQueries(const std::string& commandParamName, const asyncgi::Request& request)
{
    if (!request.hasQuery(commandParamName))
        return std::nullopt;
    return std::string{request.query(commandParamName)};
}
} //namespace

template<TaskLaunchMode taskLaunchMode>
std::optional<ProcessCfg> TaskProcessor<taskLaunchMode>::makeProcessCfg(
        const asyncgi::RouteParameters<>& routeParams,
        const asyncgi::Request& request,
        asyncgi::Response& response)
{
    auto processCfg = task_.process;
    for (const auto& commandParam : task_.commandParams) {
        if (auto routeParam = paramFromRoute(commandParam, task_, routeParams))
            processCfg.command = sfun::replace(processCfg.command, "{{" + commandParam + "}}", routeParam.value());
        else if (auto queryParam = paramFromQueries(commandParam, request))
            processCfg.command = sfun::replace(processCfg.command, "{{" + commandParam + "}}", queryParam.value());
        else {
            const auto errorMessage = fmt::format(
                    "Couldn't launch the command '{}'. Request doesn't contain a parameter '{}'",
                    task_.process.command,
                    commandParam);
            spdlog::error(errorMessage);
            response.send(asyncgi::http::ResponseStatus::Code_422_Unprocessable_Entity, errorMessage);
            return std::nullopt;
        }
    }
    return processCfg;
}

template struct TaskProcessor<TaskLaunchMode::Detached>;
template struct TaskProcessor<TaskLaunchMode::WaitingForResult>;

} //namespace stone_skipper
