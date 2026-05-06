# the name of the target operating system
set(CMAKE_SYSTEM_NAME Generic)

# define path to toolchain (if necessary)
if (NOT DEFINED TOOLCHAIN_PREFIX)
    set(TOOLCHAIN_PREFIX "/opt/riscv/bin/riscv32-unknown-elf-")
endif()

# which compilers to use
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}g++")

# adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
