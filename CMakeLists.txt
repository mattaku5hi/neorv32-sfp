cmake_minimum_required(VERSION 3.20)
include(CMakePrintHelpers)
# to use cmake_print_variables

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

set(SUFFIX_BOOT "boot")
set(SUFFIX_DEMO "demo")
set(SUFFIX_COREMARK "coremark")
set(SUFFIX_APP "app")

# set(CMAKE_CROSSCOMPILING 1)
set(CURDIR ${CMAKE_SOURCE_DIR})
set(OUTDIR ${CMAKE_BINARY_DIR})
set(OUTDIR_BOOT "${OUTDIR}/${SUFFIX_BOOT}")
set(OUTDIR_DEMO "${OUTDIR}/${SUFFIX_DEMO}")
set(OUTDIR_COREMARK "${OUTDIR}/${SUFFIX_COREMARK}")
set(OUTDIR_APP "${OUTDIR}/${SUFFIX_APP}")
set(FREERTOS_HOME "${CURDIR}/FreeRTOS-Kernel")
set(NEORV32_HOME "${CURDIR}/neorv32")
set(LOCAL_COREMARK_HOME "${CURDIR}/my_coremark")
set(EEMBC_COREMARK_HOME "${CURDIR}/coremark")
set(NEORV32_COMMON "${NEORV32_HOME}/sw/common")
set(NEORV32_EXAMPLE "${NEORV32_HOME}/sw/example")
set(NEORV32_LIB "${NEORV32_HOME}/sw/lib")
set(NEORV32_SRC "${NEORV32_LIB}/source")
set(NEORV32_RTL "${NEORV32_HOME}/rtl")
set(NEORV32_LD "${NEORV32_COMMON}/neorv32.ld")
set(IMAGE_GEN_SUBDIR "image_gen")
set(IMAGE_LOAD_SUBDIR "image_load")

set(ARTI_FREERTOS_LIB "freertos_config")

set(ARTI_IMAGE_GEN "${PROJECT_NAME}_image_gen")

set(ARTI_BOOT_ELF "${PROJECT_NAME}_${SUFFIX_BOOT}.elf")
set(ARTI_BOOT_BIN "${PROJECT_NAME}_${SUFFIX_BOOT}.bin")
set(ARTI_BOOT_IMAGE "${PROJECT_NAME}_${SUFFIX_BOOT}.vhd")
set(ARTI_BOOT_EXE_BIN "${PROJECT_NAME}_${SUFFIX_BOOT}_exe.bin")

set(ARTI_DEMO_ELF "${PROJECT_NAME}_${SUFFIX_DEMO}.elf")
set(ARTI_DEMO_BIN "${PROJECT_NAME}_${SUFFIX_DEMO}.bin")
set(ARTI_DEMO_EXE_BIN "${PROJECT_NAME}_${SUFFIX_DEMO}_exe.bin")

set(ARTI_COREMARK_ELF "${PROJECT_NAME}_${SUFFIX_COREMARK}.elf")
set(ARTI_COREMARK_BIN "${PROJECT_NAME}_${SUFFIX_COREMARK}.bin")
set(ARTI_COREMARK_EXE_BIN "${PROJECT_NAME}_${SUFFIX_COREMARK}_exe.bin")

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

set(INCLUDE_DIRECTORIES_FREERTOS
    "${FREERTOS_HOME}/include"
    "${FREERTOS_HOME}/portable/GCC/RISC-V"
)

set(COMPILE_DEFINITIONS_NEO_SIGILLUM_BOOT
    ${COMPILE_DEFINITIONS_NEO_SIGILLUM}
    -DMAKE_BOOTLOADER
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

set(COMPILE_DEFINITIONS_NEO_SIGILLUM_COREMARK
    -DBAUD_RATE=115200
    -DITERATIONS=555
    -DTOTAL_DATA_SIZE=2000
    -DMEM_METHOD=MEM_STACK
    -DSEED_METHOD=SEED_VOLATILE
    -DCOREMARK_COMPILE_OPTIONS="-march=${MARCH} -mabi=${MABI} -g ${CC_OPTIMIZATION}"
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

# Set NeoRV32 chip extensions for FreeRTOS-Kernel ported on a RISC-V chip
set(CHIP_SPECIFIC_EXTENSIONS "${CHIP_SPECIFIC_EXTENSIONS} ${CURDIR}/inc/freertos_risc_v_chip_specific_extensions.h")
# Port FreeRTOS kernel on the desired device (see available options in FreeRTOS root CMakeLists)
set(FREERTOS_PORT "GCC_RISC_V") 
set(FREERTOS_RISCV_EXTENSION "${CURDIR}/inc/freertos_risc_v_chip_specific_extensions.h")
# Build FreeRTOS-Kernel as a static library
add_library(${ARTI_FREERTOS_LIB} INTERFACE)
target_include_directories(${ARTI_FREERTOS_LIB} SYSTEM INTERFACE "${CURDIR}/inc")
# Use FreeRTOS-Kernel memory model 4 (with pseudo-heap)
set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)
# Add FreeRTOS-Kernel CMake subproject
add_subdirectory(FreeRTOS-Kernel)

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
    "${NEORV32_SRC}/neorv32_uart.c"
    "${NEORV32_SRC}/neorv32_xip.c"

    "${CURDIR}/src/utils_common.c"
    "${CURDIR}/src/spi_flash.c"
)

set(ASM_SOURCES_NEO_SIGILLUM_BOOT
    ${ASM_SOURCES_NEO_SIGILLUM_COMMON}
)

set(C_SOURCES_NEO_SIGILLUM_BOOT
    ${C_SOURCES_NEO_SIGILLUM_COMMON}
    "${CURDIR}/bootloader/bootloader.c"
)

set(ASM_SOURCES_NEO_SIGILLUM_DEMO
    "${ASM_SOURCES_NEO_SIGILLUM_COMMON}"
)

set(C_SOURCES_NEO_SIGILLUM_DEMO
    "${C_SOURCES_NEO_SIGILLUM_COMMON}"
    "${CURDIR}/demo/blinky.c"
    "${CURDIR}/demo/main.c"
)

set(ASM_SOURCES_NEO_SIGILLUM_COREMARK
    "${ASM_SOURCES_NEO_SIGILLUM_COMMON}"
)

set(C_SOURCES_NEO_SIGILLUM_COREMARK
    "${C_SOURCES_NEO_SIGILLUM_COMMON}"
    "${LOCAL_COREMARK_HOME}/coremark_portme.c"
    "${LOCAL_COREMARK_HOME}/coremark_printf.c"
    "${EEMBC_COREMARK_HOME}/core_list_join.c"
    "${EEMBC_COREMARK_HOME}/core_main.c"
    "${EEMBC_COREMARK_HOME}/core_matrix.c"
    "${EEMBC_COREMARK_HOME}/core_state.c"
    "${EEMBC_COREMARK_HOME}/core_util.c"
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

###### Image loader host system utility ######
# Add a CMake subproject - NeoRV32 specific image loader utility
add_subdirectory(${IMAGE_LOAD_SUBDIR})
#################################################

###### Bootloader executable #####
add_executable(${ARTI_BOOT_ELF} ${ASM_SOURCES_NEO_SIGILLUM_BOOT} ${C_SOURCES_NEO_SIGILLUM_BOOT})

# Build it to the dedicated folder
set_target_properties(${ARTI_BOOT_ELF} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTDIR_BOOT}
)

# Bootloader executable's include directories
target_include_directories(${ARTI_BOOT_ELF} PRIVATE 
    ${INCLUDE_DIRECTORIES_NEO_SIGILLUM}
)
# Bootloader executable's compiler options
target_compile_options(${ARTI_BOOT_ELF} PRIVATE
    ${CC_OPTIONS_NEO_SIGILLUM_BOOT}
)

target_link_options(${ARTI_BOOT_ELF} PRIVATE
    ${LINK_OPTIONS_NEO_SIGILLUM_BOOT}    
)

#################################################

###### FreeRTOS libraries #####
# FreeRTOS-Kernel library's include directories
target_include_directories(${ARTI_FREERTOS_LIB} INTERFACE 
    ${INCLUDE_DIRECTORIES_FREERTOS}
    ${INCLUDE_DIRECTORIES_NEO_SIGILLUM}
)
# FreeRTOS-Kernel library's compiler options
target_compile_options(${ARTI_FREERTOS_LIB} INTERFACE
    ${CC_OPTIONS_NEO_SIGILLUM}
)
# FreeRTOS-Kernel library's linker options
target_link_options(${ARTI_FREERTOS_LIB} INTERFACE
    ${CC_OPTIONS_NEO_SIGILLUM}
)
#################################################

###### Demo executable #####
add_executable(${ARTI_DEMO_ELF} ${ASM_SOURCES_NEO_SIGILLUM_DEMO} ${C_SOURCES_NEO_SIGILLUM_DEMO})

if(TARGET ${ARTI_DEMO_ELF})
    # Build it to the dedicated folder
    set_target_properties(${ARTI_DEMO_ELF} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${OUTDIR_DEMO}
    )

    # Demo executable's include directories
    target_include_directories(${ARTI_DEMO_ELF} PRIVATE 
        ${INCLUDE_DIRECTORIES_FREERTOS}
        ${INCLUDE_DIRECTORIES_NEO_SIGILLUM}
    )
    # Demo executable's compiler options
    target_compile_options(${ARTI_DEMO_ELF} PRIVATE
        ${CC_OPTIONS_NEO_SIGILLUM}
    )
    # Demo executable's linker options
    target_link_options(${ARTI_DEMO_ELF} PRIVATE
        ${LINK_OPTIONS_NEO_SIGILLUM}
        -Wl,--gc-sections,-Map=${OUTDIR_DEMO}/${PROJECT_NAME}_${SUFFIX_DEMO}.map
    )
    # Demo executable's libraries to be linked (of course, statically)
    target_link_libraries(${ARTI_DEMO_ELF} PRIVATE
        freertos_kernel
    )
endif()
#################################################


###### Coremark executable #####
# add_executable(${ARTI_COREMARK_ELF} ${ASM_SOURCES_NEO_SIGILLUM_COREMARK} ${C_SOURCES_NEO_SIGILLUM_COREMARK})

if(TARGET ${ARTI_COREMARK_ELF})
    # Build it to the dedicated folder
    set_target_properties(${ARTI_COREMARK_ELF} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${OUTDIR_COREMARK}
    )

    # Coremark executable's include directories
    target_include_directories(${ARTI_COREMARK_ELF} PRIVATE 
        ${INCLUDE_DIRECTORIES_NEO_SIGILLUM}
        ${EEMBC_COREMARK_HOME}
        ${LOCAL_COREMARK_HOME}
    )

    # Coremark executable's compiler options
    target_compile_options(${ARTI_COREMARK_ELF} PRIVATE
        ${CC_OPTIONS_NEO_SIGILLUM}
        ${COMPILE_DEFINITIONS_NEO_SIGILLUM_COREMARK}
    )
    
    # Coremark executable's linker options
    target_link_options(${ARTI_COREMARK_ELF} PRIVATE
        ${LINK_OPTIONS_NEO_SIGILLUM}
        -Wl,--gc-sections,-Map=${OUTDIR_COREMARK}/${PROJECT_NAME}_${SUFFIX_COREMARK}.map
    )
endif()

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
    freertos_kernel
    gcc
    m
)
#################################################

# Make sure that image generator is built first
add_dependencies(${ARTI_BOOT_ELF} ${ARTI_IMAGE_GEN})
if(TARGET ${ARTI_DEMO_ELF})
add_dependencies(${ARTI_DEMO_ELF} ${ARTI_IMAGE_GEN})
endif()
if(TARGET ${ARTI_COREMARK_ELF})
    add_dependencies(${ARTI_COREMARK_ELF} ${ARTI_IMAGE_GEN})
endif()
add_dependencies(${ARTI_APP_ELF} ${ARTI_IMAGE_GEN})

###### Post-build special commands ######
# Hold several custom post-build operations 
add_custom_command(TARGET ${ARTI_BOOT_ELF}
    POST_BUILD    

    # Generate raw firmware files
    COMMAND ${TOOLCHAIN_OBJCOPY} -O binary ${OUTDIR_BOOT}/${ARTI_BOOT_ELF} ${OUTDIR_BOOT}/${ARTI_BOOT_BIN}
    COMMAND ${TOOLCHAIN_SIZE} ${OUTDIR_BOOT}/${ARTI_BOOT_ELF}
    # Generate images for NeoRV32 FreeRTOS demo project
    COMMAND echo "Generating \'${ARTI_BOOT_IMAGE}\'"
    COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -bld_vhd ${OUTDIR_BOOT}/${ARTI_BOOT_BIN} ${OUTDIR_BOOT}/${ARTI_BOOT_IMAGE}
    COMMAND echo "Generating \'${ARTI_BOOT_EXE_BIN}\'"
    COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -app_bin ${OUTDIR_BOOT}/${ARTI_BOOT_BIN} ${OUTDIR_BOOT}/${ARTI_BOOT_EXE_BIN}
)

if(TARGET ${ARTI_COREMARK_ELF})
    add_custom_command(TARGET ${ARTI_COREMARK_ELF}
        POST_BUILD    

        # Generate raw firmware files
        COMMAND ${TOOLCHAIN_OBJCOPY} -O binary ${OUTDIR_COREMARK}/${ARTI_COREMARK_ELF} ${OUTDIR_COREMARK}/${ARTI_COREMARK_BIN}
        COMMAND ${TOOLCHAIN_SIZE} ${OUTDIR_COREMARK}/${ARTI_COREMARK_ELF}
        # Generate images for NeoRV32 FreeRTOS demo project
        COMMAND echo "Generating \'${ARTI_COREMARK_EXE_BIN}\'"
        COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -app_bin ${OUTDIR_COREMARK}/${ARTI_COREMARK_BIN} ${OUTDIR_COREMARK}/${ARTI_COREMARK_EXE_BIN}
    )
endif()

if(TARGET ${ARTI_DEMO_ELF})
    add_custom_command(TARGET ${ARTI_DEMO_ELF}
        POST_BUILD    

        # Generate raw firmware files
        COMMAND ${TOOLCHAIN_OBJCOPY} -O binary ${OUTDIR_DEMO}/${ARTI_DEMO_ELF} ${OUTDIR_DEMO}/${ARTI_DEMO_BIN}
        # COMMAND ${TOOLCHAIN_OBJCOPY} -I elf32-little ${OUTDIR_DEMO}/${ARTI_DEMO_ELF} -j .text -O binary ${OUTDIR_DEMO}/${ARTI_DEMO_ELF}_text.bin
        # COMMAND ${TOOLCHAIN_OBJCOPY} -I elf32-little ${OUTDIR_DEMO}/${ARTI_DEMO_ELF} -j .rodata -O binary ${OUTDIR_DEMO}/${ARTI_DEMO_ELF}_rodata.bin
        # COMMAND ${TOOLCHAIN_OBJCOPY} -I elf32-little ${OUTDIR_DEMO}/${ARTI_DEMO_ELF} -j .data -O binary ${OUTDIR_DEMO}/${ARTI_DEMO_ELF}_data.bin
        # COMMAND cat ${OUTDIR_DEMO}/${ARTI_DEMO_ELF}_text.bin ${OUTDIR_DEMO}/${ARTI_DEMO_ELF}_rodata.bin ${OUTDIR_DEMO}/${ARTI_DEMO_ELF}_data.bin > ${OUTDIR_DEMO}/${ARTI_DEMO_BIN}
        # COMMAND rm -f ${OUTDIR_DEMO}/${ARTI_DEMO_ELF}_text.bin ${OUTDIR_DEMO}/${ARTI_DEMO_ELF}_rodata.bin ${OUTDIR_DEMO}/${ARTI_DEMO_ELF}_data.bin
        # DEPENDEES ${ARTI_DEMO_ELF}

        COMMAND ${TOOLCHAIN_SIZE} ${OUTDIR_DEMO}/${ARTI_DEMO_ELF}
        # Generate images for NeoRV32 FreeRTOS demo project
        COMMAND echo "Generating \'${ARTI_DEMO_EXE_BIN}\'"
        COMMAND ${OUTDIR}/${IMAGE_GEN_SUBDIR}/${ARTI_IMAGE_GEN} -app_bin ${OUTDIR_DEMO}/${ARTI_DEMO_BIN} ${OUTDIR_DEMO}/${ARTI_DEMO_EXE_BIN}
    )
endif()

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
