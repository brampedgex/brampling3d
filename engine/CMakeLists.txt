project(
    engine-main
    LANGUAGES CXX ASM ASM_NASM
)

set(GENERATED_SOURCE_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_SOURCE_DIR}) 
file(MAKE_DIRECTORY "${GENERATED_SOURCE_DIR}/include/engine-generated")

file(GLOB_RECURSE ENGINE_MAIN_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(${PROJECT_NAME} ${ENGINE_MAIN_SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE ENGINE_SYSTEM_PROCESSOR="${CMAKE_HOST_SYSTEM_PROCESSOR}")

# Include generated source files.
target_include_directories(${PROJECT_NAME} PRIVATE "${GENERATED_SOURCE_DIR}/include")

target_link_libraries(${PROJECT_NAME} 
    SDL3::SDL3
    Vulkan::Vulkan
    spdlog::spdlog
    glm::glm
    imgui::imgui
    stb::stb
)

if (APPLE)
    target_link_libraries(${PROJECT_NAME}
        Vulkan::MoltenVK
    )
endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE "src/pch.hpp")

add_subdirectory(assets)
