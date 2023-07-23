#include "commandline.h"
#include "config.h"
#include "task.h"
#include "taskprocessor.h"
#include <asyncgi/asyncgi.h>
#include <cmdlime/commandlinereader.h>
#include <figcone/configreader.h>
#include <sfun/functional.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <filesystem>

using namespace stone_skipper;
namespace fs = std::filesystem;
namespace http = asyncgi::http;

void createDefaultLogger(const fs::path& logPath);
void createDefaultConfig();

int mainApp(const CommandLine& commandLine)
{
    createDefaultConfig();
    if (commandLine.log.has_value())
        createDefaultLogger(commandLine.log.value());

    auto configReader = figcone::ConfigReader{};
    auto config = configReader.readShoalFile<Config>(commandLine.config);
    spdlog::info("Configuration was read from {}", sfun::path_string(commandLine.config));

    auto io = asyncgi::IO{commandLine.threads};
    auto router = asyncgi::Router{};
    for (const auto& taskCfg : config.tasks){
        const auto task = Task{taskCfg, commandLine.shell};
        router.route(task.routeRegexp, http::RequestMethod::Get).process<TaskProcessor<TaskLaunchMode::WaitingForResult>>(task);
        router.route(task.routeRegexp, http::RequestMethod::Post).process<TaskProcessor<TaskLaunchMode::Detached>>(task);
    }
    router.route().set(http::ResponseStatus::_404_Not_Found, "Unknown task");
    if (config.tasks.empty())
        spdlog::warn("No tasks were found in the config");

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

    spdlog::info("stone_skipper task server has started");
    io.run();
    spdlog::info("stone_skipper task server has stopped");
    return 0;
}

int main(int argc, char** argv)
{
    auto cmdLineReader = cmdlime::CommandLineReader<cmdlime::Format::Simple>{"stone_skipper", "v1.1.0"};
    try {
        return cmdLineReader.exec<CommandLine>(argc, argv, mainApp);
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what();
        return 1;
    }
}

void createDefaultLogger(const fs::path& logPath)
{
    auto sinks = std::vector<spdlog::sink_ptr>{
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(sfun::path_string(logPath))};
    auto logger = std::make_shared<spdlog::logger>("default", std::begin(sinks), std::end(sinks));

    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger);
}

void createDefaultConfig()
{
    namespace fs = std::filesystem;

    auto skipError = std::error_code{};
    if (fs::exists(defaultConfigPath, skipError))
        return;

    try {
        fs::create_directories(defaultConfigPath.parent_path());
        auto stream = std::ofstream{defaultConfigPath};
        stream.exceptions(std::ofstream::failbit);
        stream << "#tasks:\n###";
    }
    catch(const std::runtime_error& error){
        spdlog::error("Couldn't create default config {}, error: {}", sfun::path_string(defaultConfigPath), error.what());
    }
    spdlog::info("Default config {} was created", sfun::path_string(defaultConfigPath));
}