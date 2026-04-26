cmake_minimum_required(VERSION 3.20)

project(neo_sigillum_image_load
    VERSION 0.7
    LANGUAGES CXX
    DESCRIPTION "Utility for creating NeoRV32 target-specific images"
)

set(CMAKE_CXX_COMPILER "/usr/bin/g++")
# Current !!! source dir because it's a CMake sub-project
set(CURDIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ARTI_IMAGE_LOAD ${PROJECT_NAME})

set(INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/inc
)

set(CXX_OPTIONS_IMAGE_LOAD
    -fpermissive
    -Wall
    -Wextra
    # -Werror
    # -pedantic
    -O
    -g
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(${CXX_OPTIONS_IMAGE_LOAD}
        ${CXX_OPTIONS_IMAGE_LOAD}
        -g3
    )
endif()

set(CXX_SOURCES_IMAGE_LOAD
    "${CURDIR}/image_load.cpp"
)

add_executable(${ARTI_IMAGE_LOAD} ${CXX_SOURCES_IMAGE_LOAD})

target_compile_definitions(${ARTI_IMAGE_LOAD} PRIVATE
    ${COMPILE_DEFINITIONS_NEO_SIGILLUM}
)

target_include_directories(${ARTI_IMAGE_LOAD} PRIVATE 
    ${INCLUDE_DIRECTORIES}
)

target_compile_options(${ARTI_IMAGE_LOAD} PRIVATE
    ${CXX_OPTIONS_IMAGE_LOAD}
)

target_link_options(${ARTI_IMAGE_LOAD} PRIVATE
    ${CXX_OPTIONS_IMAGE_LOAD} 
)

target_link_libraries(${ARTI_IMAGE_LOAD} PRIVATE
    ftd2xx
)
