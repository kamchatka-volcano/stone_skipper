#include "commandline.h"
#include "config.h"
#include "task.h"
#include "taskprocessor.h"
#include <asyncgi/asyncgi.h>
#include <cmdlime/commandlinereader.h>
#include <figcone/configreader.h>
#include <sfun/functional.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <filesystem>

using namespace stone_skipper;

int mainApp(const CommandLine& commandLine)
{
    using namespace asyncgi::http;

    auto configReader = figcone::ConfigReader{};
    auto config = configReader.readShoalFile<Config>(std::filesystem::canonical(commandLine.config));
    auto io = [&]
    {
        if (commandLine.threads.has_value()) {
            return asyncgi::IO{commandLine.threads.value()};
        }
        else
            return asyncgi::IO{};
    }();
    if (commandLine.threads.has_value() && commandLine.threads.value() > 1) {
        auto mtLogger = spdlog::stdout_color_mt("stone_skipper");
        spdlog::set_default_logger(mtLogger);
    }
    auto router = asyncgi::Router{};
    for (const auto& taskCfg : config.tasks) {
        const auto task = Task{taskCfg, config.shell};
        router.route(task.routeRegexp, RequestMethod::Get).process<TaskProcessor>(task);
    }
    router.route().set(asyncgi::http::ResponseStatus::_404_Not_Found, "Page not found");

    auto server = asyncgi::Server{io, router};
    std::visit(
            sfun::overloaded{
                    [&](const TcpHost& host)
                    {
                        server.listen(host.ipAddress, host.port);
                    },
                    [&](const UnixDomainHost& host)
                    {
                        server.listen(host.path);
                    }},
            commandLine.fcgiAddress);

    spdlog::info("stone_skipper has started");
    io.run();
    spdlog::info("stone_skipper has stopped");
    return 0;
}

int main(int argc, char** argv)
{
    auto cmdLineReader = cmdlime::CommandLineReader<cmdlime::Format::Simple>{"stone_skipper", "v1.0.0"};
    return cmdLineReader.exec<CommandLine>(argc, argv, mainApp);
}
