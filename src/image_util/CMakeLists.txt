# ANGLE Image Utilities CMakeLists.txt

set(IMAGE_UTIL_SOURCES
    AstcDecompressor.cpp
    AstcDecompressor.h
    AstcDecompressorNoOp.cpp
    AstcDecompressorTestUtils.h
    copyimage.cpp
    copyimage.h
    copyimage.inc
    generatemip.h
    generatemip.inc
    imageformats.cpp
    imageformats.h
    loadimage.cpp
    loadimage.h
    loadimage.inc
    loadimage_astc.cpp
    loadimage_etc.cpp
    loadimage_paletted.cpp
    storeimage.h
    storeimage_paletted.cpp
)

add_library(angle_image_util EXCLUDE_FROM_ALL ${IMAGE_UTIL_SOURCES})

target_include_directories(angle_image_util PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common/base
    ${CMAKE_SOURCE_DIR}/src/common/third_party/xxhash
    ${CMAKE_SOURCE_DIR}/third_party/abseil-cpp
)

target_compile_features(angle_image_util PUBLIC cxx_std_20)
target_link_libraries(angle_image_util angle_common)