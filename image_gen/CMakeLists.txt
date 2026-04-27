cmake_minimum_required(VERSION 3.20)

project(neo_sigillum_image_gen
    VERSION 0.7
    LANGUAGES ASM C
    DESCRIPTION "Utility for creating NeoRV32 target-specific images"
)

set(CMAKE_C_COMPILER "/usr/bin/gcc")
# Current !!! source dir because it's a CMake sub-project
set(CURDIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ARTI_IMAGE_GEN ${PROJECT_NAME})

set(CC_OPTIONS_IMAGE_GEN
    -Wall
    -Wextra
    -Werror
    -pedantic
    -O
    -g
)

set(C_SOURCES_IMAGE_GEN
    "${CURDIR}/image_gen.c"
)

add_executable(${ARTI_IMAGE_GEN} ${C_SOURCES_IMAGE_GEN})

target_compile_definitions(${ARTI_IMAGE_GEN} PRIVATE
    ${COMPILE_DEFINITIONS_NEO_SIGILLUM}
)

target_include_directories(${ARTI_IMAGE_GEN} PRIVATE 
    ${INCLUDE_DIRECTORIES}
)

target_compile_options(${ARTI_IMAGE_GEN} PRIVATE
    ${CC_OPTIONS_IMAGE_GEN}
)

target_link_options(${ARTI_IMAGE_GEN} PRIVATE
    ${CC_OPTIONS_IMAGE_GEN} 
)
