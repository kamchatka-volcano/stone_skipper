#pragma once
#include "path_utils.h"
#include <cmdlime/config.h>
#include <sfun/string_utils.h>
#include <filesystem>
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

struct CommandLine : cmdlime::Config {
    CMDLIME_PARAM(log, cmdlime::optional<std::filesystem::path>);
    CMDLIME_PARAM(config, std::filesystem::path)(configPath() / "stone_skipper/stone_skipper.cfg");
    CMDLIME_PARAM(fcgiAddress, FcgiHost);
    CMDLIME_PARAM(threads, std::optional<int>) << [](std::optional<int> value)
    {
        if (value && *value <= 0)
            throw cmdlime::ValidationError{"threads number must be positive"};
    };
};

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