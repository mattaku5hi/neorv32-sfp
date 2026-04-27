cmake_minimum_required(VERSION 3.20)
include(CMakePrintHelpers)
# to use cmake_print_variables

# When BUILD_TESTS is ON, configure only the host-side test subproject and
# skip the cross-compiled firmware build entirely.  This lets the same
# top-level source tree be used for both firmware and native test builds
# (in different build directories).
option(BUILD_TESTS "Build host-side unit tests instead of firmware" OFF)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
    return()
endif()

project(neo_sigillum
    VERSION 0.7
    LANGUAGES ASM C
    DESCRIPTION "Software running on neorv32 software RISC-V core, integrated in Godin GW2A FPGA"
)

set(TOOLCHAIN_OBJDUMP "${TOOLCHAIN_PREFIX}objdump")
set(TOOLCHAIN_OBJCOPY "${TOOLCHAIN_PREFIX}objcopy")
set(TOOLCHAIN_READELF "${TOOLCHAIN_PREFIX}readelf")
set(TOOLCHAIN_SIZE "${TOOLCHAIN_PREFIX}size")
set(TOOLCHAIN_GDB "${TOOLCHAIN_PREFIX}gdb")

set(SUFFIX_APP "app")

# set(CMAKE_CROSSCOMPILING 1)
set(CURDIR ${CMAKE_SOURCE_DIR})
set(OUTDIR ${CMAKE_BINARY_DIR})
set(OUTDIR_APP "${OUTDIR}/${SUFFIX_APP}")
set(NEORV32_HOME "${CURDIR}/neorv32")
set(NEORV32_COMMON "${NEORV32_HOME}/sw/common")
set(NEORV32_EXAMPLE "${NEORV32_HOME}/sw/example")
set(NEORV32_LIB "${NEORV32_HOME}/sw/lib")
set(NEORV32_SRC "${NEORV32_LIB}/source")
set(NEORV32_RTL "${NEORV32_HOME}/rtl")
set(NEORV32_LD "${NEORV32_COMMON}/neorv32.ld")
set(IMAGE_GEN_SUBDIR "image_gen")

set(ARTI_IMAGE_GEN "${PROJECT_NAME}_image_gen")

set(ARTI_APP_ELF "${PROJECT_NAME}.elf")
set(ARTI_APP_BIN "${PROJECT_NAME}.bin")
set(ARTI_APP_RAW_EXE_BIN "${PROJECT_NAME}_raw_exe.bin")
set(ARTI_APP_RAW_EXE_HEX "${PROJECT_NAME}_raw_exe.hex")
set(ARTI_APP_RAW_EXE_COE "${PROJECT_NAME}_raw_exe.coe")
set(ARTI_APP_RAW_EXE_MEM "${PROJECT_NAME}_raw_exe.mem")
set(ARTI_APP_RAW_EXE_MIF "${PROJECT_NAME}_raw_exe.mif")
set(ARTI_APP_EXE_BIN "${PROJECT_NAME}_exe.bin")
set(ARTI_APP_IMAGE  "${PROJECT_NAME}.vhd")

set(MARCH "rv32i_zicsr_zifencei" CACHE STRING "CPU architecture")
set(MABI "ilp32" CACHE STRING "Machine binary interface")
set(CC_OPTIMIZATION "Os")

set(COMPILE_DEFINITIONS_NEO_SIGILLUM
    -DAPP_EXE_SIGNATURE=0xb0bacafe
    -DAPP_CHUNK_SIZE=256
    -DAPP_CHUNK_CRC_INITIAL=0xffff
    -DPRINT_LOG_UART
)

set(INCLUDE_DIRECTORIES_NEO_SIGILLUM 
    "${NEORV32_LIB}/include"
    "${CURDIR}/inc"
)

set(COMPILE_DEFINITIONS_NEO_SIGILLUM_BOOT
    ${COMPILE_DEFINITIONS_NEO_SIGILLUM}
#    -DMAKE_BOOTLOADER
)

# Define compiler options for both FreeRTOS-Kernel lib and gowin-riscv executable
set(CC_OPTIONS_NEO_SIGILLUM 
    ${COMPILE_DEFINITIONS_NEO_SIGILLUM}
    "-march=${MARCH}"
    "-mabi=${MABI}"
    "-Wall"
    "-ffunction-sections"
    "-fdata-sections"
    "-nostartfiles"
    "-mno-fdiv"
    "-mstrict-align"
    "-mbranch-cost=10"
)

string(JOIN " " CC_OPTIONS_NEO_SIGILLUM_STRING
    "-march=${MARCH}"
    "-mabi=${MABI}"
    "-Wall"
    "-ffunction-sections"
    "-fdata-sections"
    "-nostartfiles"
    "-mno-fdiv"
    "-mstrict-align"
    # "-mbranch-cost=10"
)

if(COMPILE_DEFINITIONS_NEO_SIGILLUM MATCHES "-DPRINT_LOG_UART")
    set(${CC_OPTIONS_NEO_SIGILLUM} 
        ${CC_OPTIONS_NEO_SIGILLUM}
        -g
    )
endif()

set(CC_OPTIONS_NEO_SIGILLUM_BOOT
    ${COMPILE_DEFINITIONS_NEO_SIGILLUM_BOOT}
    ${CC_OPTIONS_NEO_SIGILLUM}
    -Wl,--defsym,__neorv32_rom_size=8k
    # -Wl,--defsym,__neorv32_rom_base=0xFFFFC000
    -Wl,--defsym,__neorv32_rom_base=0xffe00000
    -Wl,--defsym,__neorv32_ram_size=1024
    -flto
    -Os # always the strict optimization
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(${CC_OPTIMIZATION} 
        "O0"
)
endif()

set(${CC_OPTIONS_NEO_SIGILLUM}
    ${CC_OPTIONS_NEO_SIGILLUM}
    ${CC_OPTIMIZATION}
)

set(LINK_OPTIONS_NEO_SIGILLUM
    ${CC_OPTIONS_NEO_SIGILLUM} 
    -Wl,--gc-sections -ffp-contract=off
    -Wl,--defsym,__neorv32_rom_size=32k
    -Wl,--defsym,__neorv32_ram_size=16k
    -Wl,--defsym,__neorv32_heap_size=512
    -T "${NEORV32_LD}"
)

set(LINK_OPTIONS_NEO_SIGILLUM_BOOT
    ${CC_OPTIONS_NEO_SIGILLUM_BOOT}
    -T "${NEORV32_LD}"
    -Wl,--gc-sections,-Map=${OUTDIR_BOOT}/${PROJECT_NAME}_${SUFFIX_BOOT}.map
)

# Assembler files for target executable
set(ASM_SOURCES_NEO_SIGILLUM_COMMON
    "${NEORV32_COMMON}/crt0.S"
)
# C files for target executable
set(C_SOURCES_NEO_SIGILLUM_COMMON  
    "${NEORV32_SRC}/neorv32_aux.c"
    "${NEORV32_SRC}/neorv32_clint.c"
    "${NEORV32_SRC}/neorv32_cpu.c"
    "${NEORV32_SRC}/neorv32_gpio.c"
    "${NEORV32_SRC}/neorv32_gptmr.c"
    "${NEORV32_SRC}/neorv32_neoled.c"
    "${NEORV32_SRC}/neorv32_rte.c"
    "${NEORV32_SRC}/neorv32_spi.c"
    "${NEORV32_SRC}/neorv32_twi.c"
    "${NEORV32_SRC}/neorv32_uart.c"
    "${NEORV32_SRC}/neorv32_xip.c"

    "${CURDIR}/src/utils_common.c"
    "${CURDIR}/src/spi_flash.c"
    "${CURDIR}/src/neo_io.c"
    "${CURDIR}/src/neo_uart.c"
    "${CURDIR}/src/neo_parser.c"
    "${CURDIR}/src/neo_exec.c"
    "${CURDIR}/src/neo_app.c"
)

set(ASM_SOURCES_NEO_SIGILLUM_BOOT
    ${ASM_SOURCES_NEO_SIGILLUM_COMMON}
)

set(ASM_SOURCES_NEO_SIGILLUM_APP
    ${ASM_SOURCES_NEO_SIGILLUM_COMMON}
)

set(C_SOURCES_NEO_SIGILLUM_APP
    ${C_SOURCES_NEO_SIGILLUM_COMMON}
    "${CURDIR}/src/main.c"
    "${CURDIR}/src/utils.c"
)

###### Image generator host system utility ######
# Add a CMake subproject - NeoRV32 specific image generator utility
add_subdirectory(${IMAGE_GEN_SUBDIR})
#################################################

#################################################

###### Application executable ######
add_executable(${ARTI_APP_ELF} ${ASM_SOURCES_NEO_SIGILLUM_APP} ${C_SOURCES_NEO_SIGILLUM_APP})

# Build it to the dedicated folder
set_target_properties(${ARTI_APP_ELF} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTDIR_APP}
)

# Application executable's include directories
target_include_directories(${ARTI_APP_ELF} PRIVATE 
    ${INCLUDE_DIRECTORIES_FREERTOS}
    ${INCLUDE_DIRECTORIES_NEO_SIGILLUM}
)
# Application executable's compiler options
target_compile_options(${ARTI_APP_ELF} PRIVATE
    ${CC_OPTIONS_NEO_SIGILLUM}
)
# Application executable's linker options
target_link_options(${ARTI_APP_ELF} PRIVATE
    ${LINK_OPTIONS_NEO_SIGILLUM}
    -Wl,--gc-sections,-Map=${OUTDIR_APP}/${PROJECT_NAME}.map 
)
# Application executable's libraries to be linked (of course, statically)
target_link_libraries(${ARTI_APP_ELF} PRIVATE
    gcc
    m
)
#################################################

# Make sure that image generator is built first
add_dependencies(${ARTI_APP_ELF} ${ARTI_IMAGE_GEN})

###### Post-build special commands ######
# Hold several custom post-build operations 
add_custom_command(TARGET ${ARTI_APP_ELF}
    POST_BUILD
    
    COMMAND ${TOOLCHAIN_OBJCOPY} -O binary ${OUTDIR_APP}/${ARTI_APP_ELF} ${OUTDIR_APP}/${ARTI_APP_BIN}
    COMMAND ${TOOLCHAIN_OBJCOPY} -O ihex ${OUTDIR_APP}/${ARTI_APP_ELF} ${OUTDIR_APP}/${PROJECT_NAME}.hex
    COMMAND ${TOOLCHAIN_OBJDUMP} -h -S -z ${OUTDIR_APP}/${ARTI_APP_ELF} >${OUTDIR_APP}/${PROJECT_NAME}.asm
    # Check memory utilization
    COMMAND ${TOOLCHAIN_SIZE} ${OUTDIR_APP}/${ARTI_APP_ELF}
    # Generate images for NeoRV32 target project
    COMMAND echo "Generating \'${ARTI_APP_RAW_EXE_BIN}\'"
    COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -raw_bin ${OUTDIR_APP}/${ARTI_APP_BIN} ${OUTDIR_APP}/${ARTI_APP_RAW_EXE_BIN} 
    COMMAND echo "Generating \'${ARTI_APP_RAW_EXE_HEX}\'"
    COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -raw_hex ${OUTDIR_APP}/${ARTI_APP_BIN} ${OUTDIR_APP}/${ARTI_APP_RAW_EXE_HEX}   
    COMMAND echo "Generating \'${ARTI_APP_RAW_EXE_COE}\'"
    COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -raw_coe ${OUTDIR_APP}/${ARTI_APP_BIN} ${OUTDIR_APP}/${ARTI_APP_RAW_EXE_COE}   
    COMMAND echo "Generating \'${ARTI_APP_RAW_EXE_MEM}\'"
    COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -raw_mem ${OUTDIR_APP}/${ARTI_APP_BIN} ${OUTDIR_APP}/${ARTI_APP_RAW_EXE_MEM}
    COMMAND echo "Generating \'${ARTI_APP_RAW_EXE_MIF}\'"
    COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -raw_mif ${OUTDIR_APP}/${ARTI_APP_BIN} ${OUTDIR_APP}/${ARTI_APP_RAW_EXE_MIF}
    COMMAND echo "Generating \'${ARTI_APP_IMAGE}\'"
    COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -app_vhd ${OUTDIR_APP}/${ARTI_APP_BIN} ${OUTDIR_APP}/${ARTI_APP_IMAGE} 
    COMMAND echo "Generating \'${ARTI_APP_EXE_BIN}\'"
    COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -app_bin ${OUTDIR_APP}/${ARTI_APP_BIN} ${OUTDIR_APP}/${ARTI_APP_EXE_BIN}
    # Check memory utilization
    COMMAND echo "Size of \'${ARTI_APP_EXE_BIN}\', bytes:"
    COMMAND wc -c < ${OUTDIR_APP}/${ARTI_APP_EXE_BIN}                                
)
#################################################
