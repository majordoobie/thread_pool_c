cmake_minimum_required(VERSION 3.20)
project(thread_pool_c C CXX)

# Uncomment this line if you need some serious debugging of the make process
#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

# Add the cmake modules directory to call functions in them
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

# Include the deps
include(deps)
include(build_utils)

# Compile the library
add_library(thread_pool SHARED thread_pool.c)
set_project_properties(thread_pool ${PROJECT_SOURCE_DIR})

# If debug is enabled make sure to include CTest at the root. This will allow
# the ctest config to be placed at the root of the build directory
IF (CMAKE_BUILD_TYPE STREQUAL "Debug")
    include(CTest)
    add_subdirectory(tests)
ENDIF()
