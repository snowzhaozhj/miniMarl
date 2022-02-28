cmake_minimum_required(VERSION 3.16)
project(miniMarl)

set(CMAKE_CXX_STANDARD 17)

set(MINIMARL_INCLUDE_DIR include)
set(MINIMARL_SOURCE_DIR src)

include_directories(${MINIMARL_INCLUDE_DIR})

add_library(miniMarl
        ${MINIMARL_SOURCE_DIR}/debug.cpp)
