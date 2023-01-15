#include "config.h"
#include "processlauncher.h"
#include "task.h"
#include "taskprocessor.h"
#include <asyncgi/asyncgi.h>
#include <cmdlime/commandlinereader.h>
#include <figcone/configreader.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <string>

struct CommandLine : cmdlime::Config {
    CMDLIME_PARAM(log, cmdlime::optional<std::filesystem::path>);
    CMDLIME_PARAM(config, std::filesystem::path);
};

using namespace stone_skipper;

int mainApp(const CommandLine& commandLine)
{
    using namespace asyncgi::http;

    auto configReader = figcone::ConfigReader{};
    auto config = configReader.readShoalFile<Config>(std::filesystem::canonical(commandLine.config));

    auto app = asyncgi::makeApp();
    auto router = asyncgi::makeRouter();
    for (const auto& taskCfg : config.tasks) {
        const auto task = Task{taskCfg, config.shell};
        router.route(task.routeRegexp, RequestMethod::GET)
                .process<TaskProcessor<TaskLaunchMode::WaitingForResult>>(task);
        router.route(task.routeRegexp, RequestMethod::POST) //
                .process<TaskProcessor<TaskLaunchMode::Detached>>(task);
    }
    router.route().set(asyncgi::http::ResponseStatus::Code_404_Not_Found, "Page not found");

    auto server = app->makeServer(router);
    server->listen("/tmp/fcgi.sock");

    spdlog::info("stone_skipper has started");
    app->exec();
    spdlog::info("stone_skipper has stopped");
    return 0;
}

int main(int argc, char** argv)
{
    auto cmdLineReader = cmdlime::CommandLineReader<cmdlime::Format::Simple>{"stone_skipper", "v1.0.0"};
    return cmdLineReader.exec<CommandLine>(argc, argv, mainApp);
}
