cmake_minimum_required(VERSION 3.14)
project(stitch VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Source files for the main library
set(STITCH_SOURCES
    src/http_parser.cpp
    src/command_interpreter.cpp
    src/response_generator.cpp
    src/connection_handler.cpp
    src/socket_manager.cpp
)

# Create a library with all the core functionality (for testing)
add_library(stitch_lib STATIC ${STITCH_SOURCES})
target_include_directories(stitch_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Main executable
add_executable(stitch src/main.cpp)
target_link_libraries(stitch PRIVATE stitch_lib)

# Enable testing
enable_testing()

# Add tests subdirectory
add_subdirectory(tests)

# Installation
install(TARGETS stitch DESTINATION bin)
