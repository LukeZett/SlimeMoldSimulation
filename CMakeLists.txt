cmake_minimum_required(VERSION 3.16)
project(
    MC
    VERSION 0.0.1 # any version number
    LANGUAGES CXX C # programming languages used by the project
)

add_executable(App main.cpp "ShaderSources.h")
set_target_properties(App PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

target_include_directories(App PUBLIC "${PROJECT_SOURCE_DIR}")

add_subdirectory(WGF3)
target_link_libraries(App PUBLIC WGF3)
target_include_directories(App PUBLIC "${PROJECT_SOURCE_DIR}/WGF3")

add_subdirectory(dep/imgui-docking)
target_link_libraries(App PRIVATE imgui)



if (MSVC)
    target_compile_options(App PRIVATE /W4)
else()
    target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
endif()

if (XCODE)
    set_target_properties(App PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
    )
endif()


if (EMSCRIPTEN)
    target_link_options(App PRIVATE
        -sUSE_GLFW=3 # Use Emscripten-provided GLFW
        -sUSE_WEBGPU # Handle WebGPU symbols
        -sASYNCIFY # Required by WebGPU-C++
        --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        -sALLOW_MEMORY_GROWTH
    )
    set_target_properties(App PROPERTIES SUFFIX ".html")
endif()

target_copy_webgpu_binaries(App)


configure_file(resources/textureRender.wgsl textureRender.wgsl COPYONLY)
configure_file(resources/compute.wgsl compute.wgsl COPYONLY)
configure_file(resources/computeAgents.wgsl computeAgents.wgsl COPYONLY)
