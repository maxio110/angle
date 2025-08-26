# Use a modern CMake version
cmake_minimum_required(VERSION 3.16)

# Define the project. This is important for IDEs.
project(ANGLE_IDE_Helper LANGUAGES CXX)

# --- Index all source files for IDEs ---
# Use GLOB_RECURSE to find all .cpp and .h files recursively.
# This will find FrameCapture.cpp under src/libANGLE/capture/

file(GLOB_RECURSE ANGLE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan-headers/include/*.h"
        # Add any other third_party or source directories you need
)

# --- Create a Target for the IDE ---
# This is the crucial step. We create a library and add all the
# source files to it. The IDE's C++ extension will use this
# to understand the project.
add_library(angle_ide_sources INTERFACE)

target_sources(angle_ide_sources INTERFACE ${ANGLE_SOURCES})

# --- Add Include Directories ---
# This tells the IDE where to find headers like "libANGLE/capture/FrameCapture.h"
# when included from a .cpp file.
target_include_directories(angle_ide_sources INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        # ANGLE uses many third-party headers, you may need to add more.
        # For example, the Vulkan headers are essential.
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan-headers/include"
)

# --- Add Preprocessor Definitions ---
# ANGLE requires many preprocessor defines to compile correctly.
# Without these, the IDE might parse the code incorrectly.
target_compile_definitions(angle_ide_sources INTERFACE
        # Common defines for ANGLE on Linux/WSL
        ANGLE_PLATFORM_LINUX
        ANGLE_ENABLE_VULKAN
        ANGLE_ENABLE_GL
        ANGLE_ENABLE_GLES1
        ANGLE_CAPTURE_ENABLED # Very important for FrameCapture.cpp
        EGL_EGLEXT_PROTOTYPES
        GL_GLEXT_PROTOTYPES

        # Add any other defines your specific build configuration uses
        # For example, if you are doing a Debug build:
        _DEBUG
        ANGLE_ENABLE_ASSERTS
)

# Optional: Add a dummy executable to make it feel more "real" to the IDE
# You can use one of the test main.cpp files for this.
add_executable(dummy_main "${CMAKE_CURRENT_SOURCE_DIR}/_bad_scm/third_party/vulkan-tools/srcqm_ijk1x/src/tests/main.cpp")
target_link_libraries(dummy_main PRIVATE angle_ide_sources)