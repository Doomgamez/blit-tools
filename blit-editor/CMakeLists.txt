cmake_minimum_required(VERSION 3.16)

# the Blit level editor: Dear ImGui on SDL2 + OpenGL, edits levels/*.blvl of a Blit project
project(BlitEditor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

find_package(SDL2 REQUIRED CONFIG)
find_package(SDL2_image REQUIRED CONFIG)
find_package(OpenGL REQUIRED)

set(THIRD_PARTY "${CMAKE_SOURCE_DIR}/third_party")

# Dear ImGui (docking branch) with the SDL2 and OpenGL backends, and ImGuizmo for the transform handles
add_library(imgui STATIC
    "${THIRD_PARTY}/imgui/imgui.cpp"
    "${THIRD_PARTY}/imgui/imgui_draw.cpp"
    "${THIRD_PARTY}/imgui/imgui_tables.cpp"
    "${THIRD_PARTY}/imgui/imgui_widgets.cpp"
    "${THIRD_PARTY}/imgui/imgui_demo.cpp"
    "${THIRD_PARTY}/imgui/backends/imgui_impl_sdl2.cpp"
    "${THIRD_PARTY}/imgui/backends/imgui_impl_opengl3.cpp"
    "${THIRD_PARTY}/imgui/misc/cpp/imgui_stdlib.cpp"
    "${THIRD_PARTY}/ImGuizmo/ImGuizmo.cpp"
)
target_include_directories(imgui PUBLIC
    "${THIRD_PARTY}/imgui"
    "${THIRD_PARTY}/imgui/backends"
    "${THIRD_PARTY}/imgui/misc/cpp"
    "${THIRD_PARTY}/ImGuizmo"
)
target_link_libraries(imgui PUBLIC SDL2::SDL2)

# the same GL loader the engine uses (GL 2.1 + extensions, framebuffers come from ARB_framebuffer_object)
add_library(glad STATIC "${THIRD_PARTY}/glad/src/gl.c")
target_include_directories(glad PUBLIC "${THIRD_PARTY}/glad/include")

file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(blit-editor WIN32 ${EDITOR_SOURCES})
target_include_directories(blit-editor PRIVATE "${CMAKE_SOURCE_DIR}/src" "${THIRD_PARTY}/cgltf")

if(WIN32)
    target_link_libraries(blit-editor PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(blit-editor PRIVATE imgui glad SDL2::SDL2 SDL2_image::SDL2_image OpenGL::GL)

if(WIN32)
    # the folder picker (IFileOpenDialog) and starting the engine's tools without console windows
    target_link_libraries(blit-editor PRIVATE ole32 shell32 uuid)

    get_filename_component(TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_command(TARGET blit-editor POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DEXECUTABLE=$<TARGET_FILE:blit-editor>"
            "-DSEARCH_DIR=${TOOLCHAIN_BIN}"
            "-DOBJDUMP=${CMAKE_OBJDUMP}"
            -P "${CMAKE_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake"
        COMMENT "Copying the DLLs blit-editor needs"
    )
endif()
