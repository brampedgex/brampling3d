cmake_minimum_required(VERSION 4.2)
project(engine)

set(CMAKE_CXX_STANDARD 23)

set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cpm")
include(cmake/CPM.cmake)

# Packages provided by the host system.
find_package(SDL3 REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS volk)
find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)

# slangc is required for compiling shaders.
find_program(slangc_EXECUTABLE NAMES slangc)
if (slangc_EXECUTABLE STREQUAL "slangc_EXECUTABLE-NOTFOUND")
    message(FATAL_ERROR "slangc not found! This is needed for compiling shaders.")
endif()

# Vendored packages
set(SPDLOG_FMT_EXTERNAL ON)
CPMAddPackage("gh:fmtlib/fmt#12.1.0")
CPMAddPackage("gh:gabime/spdlog@1.16.0")
CPMAddPackage("gh:g-truc/glm#1.0.2")

add_compile_definitions($<$<CONFIG:Debug>:DEBUG_BUILD>)

add_subdirectory(engine)
