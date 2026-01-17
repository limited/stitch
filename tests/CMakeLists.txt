# Find CppUnit
find_package(PkgConfig REQUIRED)
pkg_check_modules(CPPUNIT REQUIRED cppunit)

# Test sources
set(TEST_SOURCES
    test_main.cpp
    test_http_parser.cpp
    test_command_interpreter.cpp
    test_response_generator.cpp
    test_connection_handler.cpp
)

# Create test executable
add_executable(run_tests ${TEST_SOURCES})

# Link against the main library and CppUnit
target_link_libraries(run_tests PRIVATE
    stitch_lib
    ${CPPUNIT_LIBRARIES}
)

target_include_directories(run_tests PRIVATE
    ${CPPUNIT_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_options(run_tests PRIVATE ${CPPUNIT_CFLAGS_OTHER})

# Add test to CTest
add_test(NAME StitchTests COMMAND run_tests)
