cmake_minimum_required(VERSION 4.2)
project(engine)

set(CMAKE_CXX_STANDARD 23)

set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cpm")
include(cmake/CPM.cmake)

find_package(SDL3 REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS volk)

set(SPDLOG_FMT_EXTERNAL ON)
CPMAddPackage("gh:fmtlib/fmt#12.1.0")
CPMAddPackage("gh:gabime/spdlog@1.16.0")

add_subdirectory(engine)
