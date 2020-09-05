# the name of the target operating system
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_VERSION   1)
set(CMAKE_SYSTEM_PROCESSOR MSP)

# which compilers to use for C and C++
set(CMAKE_C_COMPILER msp430-elf-gcc)
set(CMAKE_CXX_COMPILER msp430-elf-g++)
set(CMAKE_ASM_COMPILER msp430-elf-gcc)
set(CMAKE_OBJCOPY msp430-elf-objcopy)
set(CMAKE_OBJDUMP msp430-elf-objdump)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# core flags
set(CORE_FLAGS "-w -pedantic-errors ${ADDITIONAL_CORE_FLAGS}")

# compiler: language specific flags
set(CMAKE_C_FLAGS "${CORE_FLAGS} -nostartfiles -nostdlib -ffreestanding -Wall -std=c99 -fdata-sections -ffunction-sections")
set(CMAKE_C_FLAGS_DEBUG "" CACHE INTERNAL "c compiler flags: Debug")
set(CMAKE_C_FLAGS_RELEASE "" CACHE INTERNAL "c compiler flags: Release")

set(CMAKE_CXX_FLAGS "${CORE_FLAGS} -Wall -std=gnu++11 -fdata-sections -ffunction-sections")
set(CMAKE_CXX_FLAGS_DEBUG "" CACHE INTERNAL "cxx compiler flags: Debug")
set(CMAKE_CXX_FLAGS_RELEASE "" CACHE INTERNAL "cxx compiler flags: Release")

set(CMAKE_ASM_FLAGS "${CORE_FLAGS}")
set(CMAKE_ASM_FLAGS_DEBUG "-nostartfiles" CACHE INTERNAL "asm compiler flags: Debug")
set(CMAKE_ASM_FLAGS_RELEASE "" CACHE INTERNAL "asm compiler flags: Release")

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# find additional toolchain executables
find_program(MSP_SIZE_EXECUTABLE msp430-elf-size)
find_program(MSP_GDB_EXECUTABLE msp430-elf-gdb)
find_program(MSP_OBJCOPY_EXECUTABLE msp430-elf-objcopy)
find_program(MSP_OBJDUMP_EXECUTABLE msp430-elf-objdump)
find_program(MSP_NM_EXECUTABLE msp430-elf-nm)
