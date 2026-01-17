cmake_minimum_required(VERSION 3.14)
project(stitch VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enhanced compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wformat=2
        -Wnull-dereference
        -Wimplicit-fallthrough
    )

    # Additional warnings for GCC
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wuseless-cast
        )
    endif()
endif()

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

# ==============================================================================
# Static Analysis and Memory Checking Targets
# ==============================================================================

# Cppcheck static analysis
find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK}
            --enable=warning,performance,portability
            --inconclusive
            --force
            --inline-suppr
            --std=c++17
            --language=c++
            --suppress=missingIncludeSystem
            --suppress=unusedFunction
            --suppress=functionStatic
            --suppress=noExplicitConstructor
            --quiet
            -I ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/*.cpp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck static analysis..."
    )
    message(STATUS "cppcheck found: ${CPPCHECK}")
    message(STATUS "  Run 'make cppcheck' to perform static analysis")
else()
    message(STATUS "cppcheck not found - static analysis target not available")
endif()

# Valgrind memory checking
find_program(VALGRIND valgrind)
if(VALGRIND)
    add_custom_target(valgrind
        COMMAND ${VALGRIND}
            --leak-check=full
            --show-leak-kinds=all
            --track-origins=yes
            --verbose
            --log-file=${CMAKE_BINARY_DIR}/valgrind-out.txt
            --error-exitcode=1
            ${CMAKE_BINARY_DIR}/tests/run_tests
        DEPENDS run_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running valgrind memory check on tests..."
    )

    add_custom_target(valgrind-report
        COMMAND ${VALGRIND}
            --leak-check=full
            --show-leak-kinds=all
            --track-origins=yes
            --verbose
            --log-file=${CMAKE_BINARY_DIR}/valgrind-out.txt
            ${CMAKE_BINARY_DIR}/tests/run_tests
        COMMAND cat ${CMAKE_BINARY_DIR}/valgrind-out.txt
        DEPENDS run_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running valgrind and displaying report..."
    )

    message(STATUS "valgrind found: ${VALGRIND}")
    message(STATUS "  Run 'make valgrind' to check for memory leaks")
    message(STATUS "  Run 'make valgrind-report' to see detailed report")
else()
    message(STATUS "valgrind not found - memory check targets not available")
endif()

# Combined analysis target
add_custom_target(analyze
    COMMENT "Running all static analysis and memory checks..."
)

if(CPPCHECK)
    add_dependencies(analyze cppcheck)
endif()

if(VALGRIND)
    add_dependencies(analyze valgrind)
endif()
