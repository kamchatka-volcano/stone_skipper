#pragma once
#include "path_utils.h"
#include <cmdlime/config.h>
#include <sfun/path.h>
#include <sfun/string_utils.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace stone_skipper {

struct TcpHost {
    std::string ipAddress;
    uint16_t port{};
};
struct UnixDomainHost {
    std::filesystem::path path;
};

using FcgiHost = std::variant<TcpHost, UnixDomainHost>;

const auto defaultConfigPath = configPath() / "stone_skipper/stone_skipper.cfg";

inline std::string defaultShellCommand()
{
#ifdef _WIN32
    return "cmd.exe /c";
#else
    return "bash -ceo pipefail";
#endif
}


// clang-format off
struct CommandLine : cmdlime::Config {
    CMDLIME_PARAM(log, cmdlime::optional<std::filesystem::path>)    << "log file path";
    CMDLIME_PARAM(config, std::filesystem::path)(defaultConfigPath) << "config file path";
    CMDLIME_PARAM(fcgiAddress, FcgiHost)                            << "socket for FastCGI connection (either a file path, or 'ipAddress:port' string)";
    CMDLIME_PARAM(shell, std::string)(defaultShellCommand())        << "shell command";
    CMDLIME_PARAM(threads, int)(1)                                  << "number of threads"
        << [](std::optional<int> value)
        {
            if (value && *value <= 0)
                throw cmdlime::ValidationError{"threads number must be positive"};
        };
};
// clang-format on

} //namespace stone_skipper

namespace cmdlime {
template<>
struct StringConverter<stone_skipper::FcgiHost> {
    static std::optional<stone_skipper::FcgiHost> fromString(const std::string& str)
    {
        using namespace stone_skipper;
        if (str.find(':') != std::string::npos) {
            auto parts = ::sfun::split(str, ":");
            if (parts.size() != 2) {
                throw cmdlime::ValidationError{"TCP host parameter must be in the format 'ipAddress:port'"};
            }
            return TcpHost{std::string{parts[0]}, static_cast<uint16_t>(std::stoi(std::string{parts[1]}))};
        }
        else
            return UnixDomainHost{str};
    }

    static std::optional<std::string> toString(const stone_skipper::FcgiHost& socket)
    {
        using namespace stone_skipper;
        if (std::holds_alternative<TcpHost>(socket))
            return std::get<TcpHost>(socket).ipAddress + ":" + std::to_string(std::get<TcpHost>(socket).port);
        else
            return std::get<UnixDomainHost>(socket).path.string();
    }
};
} //namespace cmdlime