cmake_minimum_required(VERSION 3.18)
project(stone_skipper VERSION 1.1.0)

find_package(Boost 1.78 REQUIRED COMPONENTS system filesystem)
include(GNUInstallDirs)
include(external/seal_lake)

SealLake_Import(sfun 4.0.1
        GIT_REPOSITORY https://github.com/kamchatka-volcano/sfun
        GIT_TAG v4.0.1
)

set(ASYNCGI_USE_BOOST_ASIO ON)
SealLake_Import(asyncgi 0.2.0
        GIT_REPOSITORY https://github.com/kamchatka-volcano/asyncgi
        GIT_TAG v0.2.0
)
SealLake_Import(cmdlime 2.5.0
        GIT_REPOSITORY https://github.com/kamchatka-volcano/cmdlime
        GIT_TAG v2.5.0
)
set(FIGCONE_USE_SHOAL ON)
SealLake_Import(figcone 2.4.8
        GIT_REPOSITORY https://github.com/kamchatka-volcano/figcone
        GIT_TAG v2.4.9
)

SealLake_Import(fmt 9.1.0
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG 9.1.0
)
set(SPDLOG_FMT_EXTERNAL ON)
SealLake_Import(spdlog 1.11.0
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.11.0
)

SealLake_Import(GSL 4.0.0
        GIT_REPOSITORY https://github.com/microsoft/GSL.git
        GIT_TAG        v4.0.0
)

SealLake_Import(platform_folders 4.2.0
        GIT_REPOSITORY https://github.com/sago007/PlatformFolders.git
        GIT_TAG        4.2.0
)
SealLake_Bundle(
        NAME range-v3
        SKIP_LOAD
        GIT_REPOSITORY https://github.com/ericniebler/range-v3
        GIT_TAG        0.12.0
)

set(SRC
    src/main.cpp
    src/task.cpp
    src/taskprocessor.cpp
    src/processlauncher.cpp
    src/utils.cpp
)

SealLake_Executable(
        SOURCES ${SRC}
        COMPILE_FEATURES cxx_std_20
        PROPERTIES
            CXX_EXTENSIONS OFF
        INCLUDES ${SEAL_LAKE_SOURCE_range-v3}/include
        LIBRARIES
            Boost::boost
            Boost::filesystem
            spdlog::spdlog
            sfun::sfun
            asyncgi::asyncgi
            cmdlime::cmdlime
            figcone::figcone
            fmt::fmt
            Microsoft.GSL::GSL
            sago::platform_folders
            Threads::Threads
)

SealLake_OptionalBuildSteps(tests)
