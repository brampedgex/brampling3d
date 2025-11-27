project(
    engine-main
    LANGUAGES CXX
)

file(GLOB_RECURSE ENGINE_MAIN_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(${PROJECT_NAME} ${ENGINE_MAIN_SOURCES})

target_link_libraries(${PROJECT_NAME} 
    SDL3::SDL3
    Vulkan::Vulkan
    Vulkan::volk
    spdlog::spdlog
    glm::glm
)

add_subdirectory(assets)
