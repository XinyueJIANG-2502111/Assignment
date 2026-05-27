cmake_minimum_required(VERSION 3.16)

# [Project Name and Language]
project(NeonSwingerGame CXX)

# [C++ Standard Specification]
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========================================================
# [DxLib and Build Environment Configuration]
# ========================================================

# Force target as a Windows GUI application to prevent a black console CMD window from popping up at startup
set(CMAKE_WIN32_EXECUTABLE ON)

# Check the compiler. The official DxLib package provides the best support for MSVC (Visual Studio).
if(NOT MSVC)
    message(WARNING "DxLib configuration is optimized for MSVC (Visual Studio). Other compilers may need adjustments.")
endif()

# ========================================================
# [Source Files and Include Paths Search]
# ========================================================

# 1. Automatically scan all .cpp, .hpp, and .h files under the Source folder
file(GLOB_RECURSE SOURCE_FILES 
    "Source/*.cpp"
    "Source/*.hpp"
    "Source/*.h"
)

# 2. Define the executable target
add_executable(${PROJECT_NAME} WIN32 ${SOURCE_FILES})

# 3. Configure header include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    "Source"    # Allows your code to directly use #include "Vector.hpp"
    "deps"      # Allows your code to directly use #include "DxLib.h"
)

# ========================================================
# [Third-Party Library Linking]
# ========================================================

# 1. Find the core DxLib static library in the deps directory
# Note: Please modify according to the actual filename in your deps folder. Typically, the VS version is named DxLib.lib, or DxLib_x64.lib for 64-bit.
find_library(DXLIB_LIBRARY 
    NAMES DxLib DxLib_x64 Dx_x64
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/deps"
)

if(NOT DXLIB_LIBRARY)
    message(FATAL_ERROR "Could not find the DxLib static library file. Please check the deps folder.")
endif()

# 2. Link library files to your project
# In addition to DxLib itself, Windows core multimedia and DirectX base libraries must be linked, otherwise massive unresolved external symbol errors (LNK2019) will occur.
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${DXLIB_LIBRARY}
    # Windows core multimedia and graphics system dependencies
    winmm
    imm32
    gdi32
    ole32
    uuid
    user32
    shell32
    oleaut32
    # DirectX core components
    d3d9
    d3dx9
    dxguid
)

# ========================================================
# [Optional: Resource Files Copy]
# ========================================================
# If the resources folder exists (storing sound effects and BGM), automatically copy it to the build output directory post-build
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    )
endif()