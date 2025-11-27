set(ASSETS_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(ASSETS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ASSETS_STAGING_DIR "${ASSETS_BINARY_DIR}/_staging")

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

set(SHADER_BINARY_DIR "${PROJECT_BINARY_DIR}/shaders")
set(ENGINE_ASSETS "")

function(CompileShader SRC TYPE WORKING_DIR)
    message(INFO "CompileShader: ${SRC}, ${TYPE}, ${WORKING_DIR}")

    get_filename_component(SRC_NAME ${SRC} NAME_WLE)
    get_filename_component(SRC_DIR ${SRC} DIRECTORY)

    # don't put a slash if SRC_DIR is empty, otherwise we'd get `/shader.spv`
    if (NOT SRC_DIR STREQUAL "")
        set(SRC_DIR "${SRC_DIR}/")
    endif()

    set(INPUT_FILE "${WORKING_DIR}/${SRC}")
    set(OUTPUT_SPV_FILE "${SHADER_BINARY_DIR}/${SRC_DIR}/${SRC_NAME}.spv")

    add_custom_command(
        OUTPUT ${OUTPUT_SPV_FILE}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=${TYPE} -o ${OUTPUT_SPV_FILE} ${INPUT_FILE}
        DEPENDS ${INPUT_FILE}
        COMMENT "Compiling shader ${OUTPUT_SPV_FILE}"
    )

    EmbedAsset(ENGINE_ASSETS ${OUTPUT_SPV_FILE} "${SRC_DIR}${SRC_NAME}.spv")
    # propagate variables
    set(ENGINE_ASSETS ${ENGINE_ASSETS} PARENT_SCOPE)
endfunction()

function(CompileVertexShader SRC WORKING_DIR)
    CompileShader("${SRC}" vertex "${WORKING_DIR}")
    # propagate variables
    set(ENGINE_ASSETS ${ENGINE_ASSETS} PARENT_SCOPE)
endfunction()

function(CompileFragmentShader SRC WORKING_DIR)
    CompileShader("${SRC}" fragment "${WORKING_DIR}")
    # propagate variables
    set(ENGINE_ASSETS ${ENGINE_ASSETS} PARENT_SCOPE)
endfunction()


CompileFragmentShader("shader_frag.glsl" "${ASSETS_SOURCE_DIR}")
CompileVertexShader("shader_vert.glsl" "${ASSETS_SOURCE_DIR}")


set(ENGINE_ASSETS_FULL ${ENGINE_ASSETS})
list(TRANSFORM ENGINE_ASSETS_FULL PREPEND "${ASSETS_STAGING_DIR}/")

set(ENGINE_ASSETS_OBJ "${ASSETS_BINARY_DIR}/engine_assets.o")
add_custom_command(
    OUTPUT ${ENGINE_ASSETS_OBJ}
    COMMAND ${LD_EXECUTABLE} -r -b binary -o ${ENGINE_ASSETS_OBJ} ${ENGINE_ASSETS}
    DEPENDS ${ENGINE_ASSETS_FULL}
    WORKING_DIRECTORY ${ASSETS_STAGING_DIR}
    COMMENT "Building object ${ENGINE_ASSETS_OBJ}"
)

add_custom_target(EngineAssets ALL DEPENDS ${ENGINE_ASSETS_FULL} ${ENGINE_ASSETS_OBJ})
add_dependencies(${PROJECT_NAME} EngineAssets)

target_sources(${PROJECT_NAME} PRIVATE ${ENGINE_ASSETS_OBJ})
