cmake_minimum_required(VERSION 3.18)
project(test_lunchtoast)

set(SRC
    test_utils.cpp
    ../src/utils.cpp
)

SealLake_GoogleTest(
        NAME test_stone_skipper
        SOURCES ${SRC}
        COMPILE_FEATURES cxx_std_20
        PROPERTIES
            CXX_EXTENSIONS OFF
        INCLUDES
            ../src
            ${SEAL_LAKE_SOURCE_range-v3}/include
        LIBRARIES
            sfun::sfun fmt::fmt Microsoft.GSL::GSL sago::platform_folders
)
