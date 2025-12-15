cmake_minimum_required(VERSION 3.30)
project(engine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fixes gcc build on macOS (we build for macOS with apple clang now so this isn't needed but i'll keep it here because it doesnt seem to do anything)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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
CPMAddPackage("gh:BasedInc/libhat@0.9.0")

CPMAddPackage(
    NAME stb
    GIT_REPOSITORY https://github.com/nothings/stb
    GIT_TAG f1c79c02822848a9bed4315b12c8c8f3761e1296
    DOWNLOAD_ONLY YES
)

if (stb_ADDED)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE $<BUILD_INTERFACE:${stb_SOURCE_DIR}>)
    
    add_library(stb::stb ALIAS stb)
endif()

CPMAddPackage(
    NAME imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    VERSION 1.92.5
    DOWNLOAD_ONLY YES
)

if (imgui_ADDED)
    file(GLOB imgui_sources ${imgui_SOURCE_DIR}/*.cpp)
    add_library(imgui STATIC 
        ${imgui_sources}
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    )
    target_include_directories(imgui PUBLIC 
        $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
        $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/backends>
    )
    target_link_libraries(imgui PRIVATE SDL3::SDL3)

    add_library(imgui::imgui ALIAS imgui)
endif()

# Portable macro for debug builds.
add_compile_definitions($<$<CONFIG:Debug>:DEBUG_BUILD>)

add_subdirectory(engine)
