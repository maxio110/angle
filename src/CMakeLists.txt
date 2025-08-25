# ANGLE Source CMakeLists.txt
# This is primarily for IntelliSense support, not for building

# Common library
add_subdirectory(common)

# Core ANGLE libraries
add_subdirectory(libANGLE)

# Compiler/translator
add_subdirectory(compiler)

# Image utilities
add_subdirectory(image_util)

# GPU info utilities  
add_subdirectory(gpu_info_util)

# Feature support utilities
add_subdirectory(feature_support_util)