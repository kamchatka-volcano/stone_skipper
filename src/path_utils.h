#pragma once
#include <platform_folders.h>
#include <filesystem>

namespace stone_skipper {

inline std::filesystem::path homePath()
{
    return std::filesystem::path{sago::getDesktopFolder()}.parent_path();
}

inline std::filesystem::path configPath()
{
    return std::filesystem::path{sago::getConfigHome()};
}

} //namespace stone_skipper