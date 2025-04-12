cmake_minimum_required(VERSION 3.10)
set(BUILD_SHARED_LIBS OFF) # We expect external libraries to be linked statically.
set(CMAKE_CXX_STANDARD 17) # Compile as C++17.
set(CMAKE_CXX_STANDARD_REQUIRED ON) # Require C++17 support.

project(babydb)

if(EMSCRIPTEN)
        add_compile_options(-fexceptions)
        add_link_options(-fexceptions)
endif()

# CTest
enable_testing()

# People keep running CMake in the wrong folder, completely nuking their project or creating weird bugs.
# This checks if you're running CMake from a folder that already has CMakeLists.txt.
# Importantly, this catches the common case of running it from the root directory.
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" PATH_TO_CMAKELISTS_TXT)

if(EXISTS "${PATH_TO_CMAKELISTS_TXT}")
        message(FATAL_ERROR "Run CMake from a build subdirectory! \"mkdir build ; cd build ; cmake ..\" \
    Some junk files were created in this folder (CMakeCache.txt, CMakeFiles); you should delete those.")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Setting build type to `Debug` as none was specified.")
        set(CMAKE_BUILD_TYPE "Debug")
endif()

# #####################################################################################################################
# COMPILER SETUP
# #####################################################################################################################

if(NOT DEFINED BABYDB_SANITIZER)
        set(BABYDB_SANITIZER address)
endif()

# Compiler flags.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=${BABYDB_SANITIZER}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -Wno-unused-parameter -Werror")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fno-omit-frame-pointer -fno-optimize-sibling-calls -g3")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}")

# Output directory.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Includes.
set(BABYDB_SRC_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/include)
# set(BABYDB_TEST_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/test/include)

include_directories(${BABYDB_SRC_INCLUDE_DIR} ${BABYDB_TEST_INCLUDE_DIR})
include_directories(BEFORE src)

add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(test)

set(P1_FILES
        "src/storage/art.cpp"
        "src/include/storage/art.hpp"
)

add_custom_target(submit-p1
        zip project1-submission.zip
        ${P1_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)