cmake_minimum_required(VERSION 3.20)

# Host-side unit tests for the freestanding modules used on the NEORV32
# target.  This subproject is built natively (no cross toolchain): the
# parent project enables it via -DBUILD_TESTS=ON, in which case the parent
# CMakeLists.txt skips firmware configuration entirely (see top-level
# CMakeLists.txt).

project(neo_sigillum_tests
    VERSION 0.7
    LANGUAGES C
    DESCRIPTION "Host unit tests for neo_io / neo_parser / neo_exec")

# Force host toolchain even if a cross toolchain leaked in via cache.
if(NOT DEFINED HOST_C_COMPILER)
    set(HOST_C_COMPILER "/usr/bin/gcc")
endif()
set(CMAKE_C_COMPILER "${HOST_C_COMPILER}")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CURDIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SRCDIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/../inc)

set(TEST_SOURCES
    ${SRCDIR}/neo_io.c
    ${SRCDIR}/neo_parser.c
    ${SRCDIR}/neo_exec.c
    ${CURDIR}/host_io.c
    ${CURDIR}/main.c
    ${CURDIR}/test_io.c
    ${CURDIR}/test_parser.c
    ${CURDIR}/test_linebuf.c
    ${CURDIR}/test_exec.c
)

add_executable(run_tests ${TEST_SOURCES})

target_include_directories(run_tests PRIVATE
    ${INCDIR}
    ${CURDIR}
)

target_compile_definitions(run_tests PRIVATE
    NEO_EXEC_NO_NEORV32
)

target_compile_options(run_tests PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -O2
    -g
)

enable_testing()
add_test(NAME run_tests COMMAND run_tests)
set_tests_properties(run_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
