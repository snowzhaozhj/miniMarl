cmake_minimum_required(VERSION 3.16)
project(miniMarl)

set(CMAKE_CXX_STANDARD 17)

set(MINIMARL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(MINIMARL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(MINIMARL_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

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
            "${MINIMARL_SOURCE_DIR}/memory.cpp"
        PUBLIC
            "${MINIMARL_INCLUDE_DIR}/marl/export.hpp"
            "${MINIMARL_INCLUDE_DIR}/marl/deprecated.hpp"
            "${MINIMARL_INCLUDE_DIR}/marl/tsa.hpp"
            "${MINIMARL_INCLUDE_DIR}/marl/debug.hpp"
            "${MINIMARL_INCLUDE_DIR}/marl/memory.hpp"
            "${MINIMARL_INCLUDE_DIR}/marl/containers.hpp"
            "${MINIMARL_INCLUDE_DIR}/marl/mutex.hpp"
            "${MINIMARL_INCLUDE_DIR}/marl/task.hpp"
        )
target_include_directories(miniMarl
        PUBLIC ${MINIMARL_INCLUDE_DIR}
        )
target_compile_options(miniMarl
        PRIVATE "-fvisibility=hidden")

add_executable(miniMarlTests "")
target_sources(miniMarlTests
        PRIVATE
            "${MINIMARL_TEST_DIR}/marl_test.hpp"
            "${MINIMARL_TEST_DIR}/marl_test.cpp"
            "${MINIMARL_TEST_DIR}/memory_test.cpp"
            "${MINIMARL_TEST_DIR}/containers_test.cpp"
            "${MINIMARL_TEST_DIR}/mutex_test.cpp"
            "${MINIMARL_TEST_DIR}/task_test.cpp"
        )
target_link_libraries(miniMarlTests miniMarl gmock gtest)
add_test(NAME miniMarlTests COMMAND miniMarlTests)

