cmake_minimum_required(VERSION 4.0)
project(engine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fixes gcc build on macOS
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cpm")
include(cmake/CPM.cmake)

# Packages provided by the host system.

set(VULKAN_COMPONENTS)

if (APPLE)
    # Use MoltenVK for Vulkan support on Apple
    set(VULKAN_COMPONENTS ${VULKAN_COMPONENTS} MoltenVK)
endif()

find_package(Vulkan REQUIRED COMPONENTS ${VULKAN_COMPONENTS})
find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)

find_program(slangc_EXECUTABLE NAMES slangc)
if (slangc_EXECUTABLE STREQUAL "slangc_EXECUTABLE-NOTFOUND")
    message(FATAL_ERROR "slangc not found! This is needed for compiling shaders.")
endif()

# Vendored packages
set(SPDLOG_FMT_EXTERNAL ON)
CPMAddPackage("gh:fmtlib/fmt#12.1.0")
CPMAddPackage("gh:gabime/spdlog@1.16.0")
CPMAddPackage("gh:g-truc/glm#1.0.2")
CPMAddPackage("gh:libsdl-org/SDL#release-3.2.28")

# Portable macro for debug builds.
add_compile_definitions($<$<CONFIG:Debug>:DEBUG_BUILD>)

add_subdirectory(engine)
