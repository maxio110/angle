cmake_minimum_required(VERSION 3.20)
project(ANGLE
    VERSION 0.1
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set the compiler paths to match your compile_commands.json
set(CMAKE_CXX_COMPILER "${CMAKE_SOURCE_DIR}/third_party/llvm-build/Release+Asserts/bin/clang++")
set(CMAKE_C_COMPILER "${CMAKE_SOURCE_DIR}/third_party/llvm-build/Release+Asserts/bin/clang")

# Common include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common/base
    ${CMAKE_SOURCE_DIR}/src/common/third_party/xxhash
    ${CMAKE_SOURCE_DIR}/third_party/abseil-cpp
    ${CMAKE_SOURCE_DIR}/buildtools/third_party/libc++
)

# Platform-specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_definitions(
        -DANGLE_IS_LINUX
        -DANGLE_ENABLE_SHARE_CONTEXT_LOCK=1
        -DANGLE_ENABLE_CONTEXT_MUTEX=1
        -DANGLE_USE_VULKAN_DISPLAY
        -DANGLE_USE_X11
        -DANGLE_USE_WAYLAND
        -DANGLE_VULKAN_DISPLAY_MODE_HEADLESS
        -DANGLE_HAS_ASTCENC
        -DANGLE_DEBUG_LAYERS_ENABLED
        -DANGLE_OUTSIDE_WEBKIT
        -DANGLE_ENABLE_DEBUG_ANNOTATIONS
        -DANGLE_USE_ABSEIL
        -D_DEBUG
        -DDYNAMIC_ANNOTATIONS_ENABLED=1
    )
endif()

# Compiler flags (based on your compile_commands.json)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -fPIC -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")

# Add subdirectories
add_subdirectory(src)

# This is primarily for IntelliSense - create an indexing target
add_library(angle_indexing EXCLUDE_FROM_ALL
    # Just include key files for indexing
    src/libANGLE/Texture.cpp
    src/common/angleutils.cpp
    src/libANGLE/Context.cpp
    src/libANGLE/Program.cpp
    src/libANGLE/Shader.cpp
)

# Set target properties
target_include_directories(angle_indexing PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common/base
    ${CMAKE_SOURCE_DIR}/src/common/third_party/xxhash
    ${CMAKE_SOURCE_DIR}/third_party/abseil-cpp
    ${CMAKE_SOURCE_DIR}/third_party/zlib/google
    ${CMAKE_SOURCE_DIR}/third_party/zlib
)