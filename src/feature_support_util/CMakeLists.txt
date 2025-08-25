# ANGLE Feature Support Utilities CMakeLists.txt

set(FEATURE_SUPPORT_UTIL_SOURCES
    feature_support_util.cpp
    feature_support_util.h
)

add_library(angle_feature_support_util EXCLUDE_FROM_ALL ${FEATURE_SUPPORT_UTIL_SOURCES})

target_include_directories(angle_feature_support_util PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common/base
    ${CMAKE_SOURCE_DIR}/src/common/third_party/xxhash
    ${CMAKE_SOURCE_DIR}/third_party/abseil-cpp
)

target_compile_features(angle_feature_support_util PUBLIC cxx_std_20)
target_link_libraries(angle_feature_support_util angle_common)