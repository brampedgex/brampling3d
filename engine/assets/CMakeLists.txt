set(ASSETS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(ASSETS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(ASSETS_STAGING_DIR "${ASSETS_BINARY_DIR}/_staging")
file(MAKE_DIRECTORY ${ASSETS_STAGING_DIR})

function(EmbedAsset OUT_VAR SRC DEST)
    set(FULL_DEST "${ASSETS_STAGING_DIR}/${DEST}")
    add_custom_command(
        OUTPUT ${FULL_DEST}
        DEPENDS ${SRC}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${FULL_DEST}
        COMMENT "Copying asset ${SRC} to ${FULL_DEST}"
    )

    # Update OUT_VAR with the new asset file
    set(${OUT_VAR} ${${OUT_VAR}} ${DEST} PARENT_SCOPE)
endfunction()

function(EmbedFile SRC)
    EmbedAsset(ENGINE_ASSETS ${SRC} ${SRC})
    # propagate variables
    set(ENGINE_ASSETS ${ENGINE_ASSETS} PARENT_SCOPE)
endfunction()

set(SHADER_BINARY_DIR "${PROJECT_BINARY_DIR}/shaders")
set(ENGINE_ASSETS "")

function(CompileSlangShader SRC STAGE)
    get_filename_component(SRC_NAME ${SRC} NAME_WLE)
    get_filename_component(SRC_DIR ${SRC} DIRECTORY)

    # don't put a slash if SRC_DIR is empty, otherwise we'd get `/shader.spv`
    if (NOT SRC_DIR STREQUAL "")
        set(SRC_DIR "${SRC_DIR}/")
    endif()

    set(INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${SRC}")
    set(OUTPUT_SPV_FILE "${SHADER_BINARY_DIR}/${SRC_DIR}/${SRC_NAME}.${STAGE}.spv")

    add_custom_command(
        OUTPUT ${OUTPUT_SPV_FILE}
        COMMAND ${slangc_EXECUTABLE} -stage ${STAGE} -entry "${STAGE}_main" -o ${OUTPUT_SPV_FILE} ${INPUT_FILE}
        DEPENDS ${INPUT_FILE}
        COMMENT "Compiling ${STAGE} shader ${OUTPUT_SPV_FILE}"
    )

    EmbedAsset(ENGINE_ASSETS ${OUTPUT_SPV_FILE} "${SRC_DIR}${SRC_NAME}.${STAGE}.spv")
    # propagate variables
    set(ENGINE_ASSETS ${ENGINE_ASSETS} PARENT_SCOPE)
endfunction()

function(CompileSlangShaders SRC)
    CompileSlangShader("${SRC}" vertex vertex_main)
    CompileSlangShader("${SRC}" fragment fragment_main)

    # propagate variables
    set(ENGINE_ASSETS ${ENGINE_ASSETS} PARENT_SCOPE)
endfunction()


CompileSlangShaders("shaders/color3d.slang")
EmbedFile("images/soggy.png")


if (MSVC OR APPLE)
    # Prefer NASM on MSVC and Apple.
    if (NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(USE_NASM TRUE)
    else()
        # NASM doesn't support ARM.
        set(USE_NASM FALSE)
    endif()
else()
    set(USE_NASM FALSE)
endif()

if (USE_NASM)
    enable_language(ASM_NASM)
    set(ENGINE_ASSETS_ASM "${GENERATED_SOURCE_DIR}/engine_assets.asm")
else()
    enable_language(ASM)
    set(ENGINE_ASSETS_ASM "${GENERATED_SOURCE_DIR}/engine_assets.S")
endif()

set(ENGINE_ASSETS_HPP "${GENERATED_SOURCE_DIR}/include/engine-generated/engine_assets.hpp")

# Generate a .asm file embedding and exporting the binaries, and a header file that declares them in C++ for convenience.
set(ASSET_BUILD_ARGS -o ${ENGINE_ASSETS_ASM} --header ${ENGINE_ASSETS_HPP})
if (USE_NASM)
    set(ASSET_BUILD_ARGS ${ASSET_BUILD_ARGS} "--nasm")
endif()
if (APPLE)
    set(ASSET_BUILD_ARGS ${ASSET_BUILD_ARGS} "--apple")
endif()
set(ASSET_BUILD_ARGS ${ASSET_BUILD_ARGS} --files ${ENGINE_ASSETS})
execute_process(
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/asset_build.py" ${ASSET_BUILD_ARGS}
    WORKING_DIRECTORY ${ASSETS_STAGING_DIR}
    COMMAND_ERROR_IS_FATAL ANY
)

# Ensure we re-run nasm when the asset files change.
set(ENGINE_ASSETS_FULL ${ENGINE_ASSETS})
list(TRANSFORM ENGINE_ASSETS_FULL PREPEND "${ASSETS_STAGING_DIR}/")
add_custom_target(EngineAssets ALL DEPENDS ${ENGINE_ASSETS_FULL})
add_dependencies(${PROJECT_NAME} EngineAssets)

set_source_files_properties(${ENGINE_ASSETS_ASM} PROPERTIES LANGUAGE ASM)
target_sources(${PROJECT_NAME} PRIVATE ${ENGINE_ASSETS_ASM})
