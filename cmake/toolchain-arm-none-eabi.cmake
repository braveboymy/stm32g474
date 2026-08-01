# =============================================================================
# 交叉编译工具链：ARM GNU Toolchain (arm-none-eabi)
# 用法：cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake
# =============================================================================

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

set(TOOLCHAIN_PREFIX arm-none-eabi-)

find_program(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc     REQUIRED)
find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc     REQUIRED)
find_program(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy REQUIRED)
find_program(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump)
find_program(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size    REQUIRED)

# 避免 CMake 在配置阶段做链接测试（嵌入式目标无 libc 运行时）
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 架构相关标志（C/ASM/链接统一）
set(ARCH_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_C_FLAGS_INIT            "${ARCH_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT          "${ARCH_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT   "${ARCH_FLAGS}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
