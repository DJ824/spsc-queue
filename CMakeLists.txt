cmake_minimum_required(VERSION 3.0)
project(LockFreeBenchmark)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags for optimization and debugging
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")

# Add debug symbols in debug mode but keep optimizations
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O3")

# Find pthread library (required for thread support)
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Add the benchmark executable
add_executable(lock_free_benchmark benchmark.cpp)

# Link against pthread
target_link_libraries(lock_free_benchmark ${CMAKE_THREAD_LIBS_INIT})

# Set output directory
set_target_properties(lock_free_benchmark
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Add a custom target for running the benchmark
add_custom_target(run_benchmark
    COMMAND ${CMAKE_BINARY_DIR}/bin/lock_free_benchmark
    DEPENDS lock_free_benchmark
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Print configuration details
message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")
