cmake_minimum_required(VERSION 3.16)
project(miniMarl)

set(CMAKE_CXX_STANDARD 17)

set(MINIMARL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(MINIMARL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

option(MINIMARL_BUILD_TESTS "" ON)

set(MINIMARL_GTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest)

if(MINIMARL_BUILD_TESTS)
    if(NOT EXISTS ${MINIMARL_GTEST_DIR}/.git)
        message(WARNING "third_party/googletest submodule missing.")
        message(WARNING "Run: `git submodule update --init` to build tests.")
        set(MINIMARL_BUILD_TESTS OFF)
    endif()
endif()

if(MINIMARL_BUILD_TESTS)
    add_subdirectory(${MINIMARL_GTEST_DIR})
    enable_testing()
endif()

add_library(miniMarl "")
target_sources(miniMarl
        PRIVATE
            "${MINIMARL_SOURCE_DIR}/debug.cpp"
        PUBLIC
            "${MINIMARL_INCLUDE_DIR}/marl/export.hpp"
            "${MINIMARL_INCLUDE_DIR}/marl/debug.hpp"
        )
target_include_directories(miniMarl
        PUBLIC ${MINIMARL_INCLUDE_DIR}
        )
target_compile_options(miniMarl
        PRIVATE "-fvisibility=hidden")

