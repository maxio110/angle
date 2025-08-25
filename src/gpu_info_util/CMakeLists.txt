# ANGLE GPU Info Utilities CMakeLists.txt

set(GPU_INFO_UTIL_SOURCES
    SystemInfo.cpp
    SystemInfo.h
    SystemInfo_android.cpp
    SystemInfo_apple.mm
    SystemInfo_fuchsia.cpp
    SystemInfo_internal.h
    SystemInfo_ios.cpp
    SystemInfo_libpci.cpp
    SystemInfo_linux.cpp
    SystemInfo_macos.mm
    SystemInfo_vulkan.cpp
    SystemInfo_vulkan.h
    SystemInfo_win.cpp
    SystemInfo_x11.cpp
)

add_library(angle_gpu_info_util EXCLUDE_FROM_ALL ${GPU_INFO_UTIL_SOURCES})

target_include_directories(angle_gpu_info_util PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common/base
    ${CMAKE_SOURCE_DIR}/src/common/third_party/xxhash
    ${CMAKE_SOURCE_DIR}/third_party/abseil-cpp
)

target_compile_features(angle_gpu_info_util PUBLIC cxx_std_20)
target_link_libraries(angle_gpu_info_util angle_common)