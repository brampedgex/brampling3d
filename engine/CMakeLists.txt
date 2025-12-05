project(
    engine-main
    LANGUAGES CXX ASM_NASM
)

set(GENERATED_SOURCE_DIR "${CMAKE_BINARY_DIR}/generated/engine-generated")
file(MAKE_DIRECTORY ${GENERATED_SOURCE_DIR}) 

file(GLOB_RECURSE ENGINE_MAIN_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(${PROJECT_NAME} ${ENGINE_MAIN_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_BINARY_DIR}/generated")

target_link_libraries(${PROJECT_NAME} 
    SDL3::SDL3
    Vulkan::Vulkan
    Vulkan::volk
    spdlog::spdlog
    glm::glm
)

target_precompile_headers(${PROJECT_NAME} PRIVATE "src/pch.hpp")

add_subdirectory(assets)
