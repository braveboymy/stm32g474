cmake_minimum_required(VERSION 3.20)
project(stm32g474-platform VERSION 0.1.0 LANGUAGES C ASM)

if(NOT CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "必须通过 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake 指定交叉工具链")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------- 目录
set(THIRD_PARTY_DIR  ${CMAKE_SOURCE_DIR}/third_party)
set(CUBE_DIR         ${THIRD_PARTY_DIR}/STM32CubeG4)
set(HAL_DIR          ${CUBE_DIR}/Drivers/STM32G4xx_HAL_Driver)
set(CMSIS_DEV_DIR    ${CUBE_DIR}/Drivers/CMSIS/Device/ST/STM32G4xx)
set(CMSIS_CORE_DIR   ${CUBE_DIR}/Drivers/CMSIS/Include)
set(FREERTOS_DIR     ${THIRD_PARTY_DIR}/FreeRTOS-Kernel)
set(FREERTOS_PORT    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F)

# ---------------------------------------------------------------- 公共选项
set(TARGET_DEFS
  STM32G474xx
  USE_HAL_DRIVER
)

add_compile_options(
  -ffunction-sections
  -fdata-sections
  -fstack-usage
)
# 统一由下方 add_compile_options 控制优化等级，屏蔽 CMake 默认 Release 标志
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "")
set(CMAKE_C_FLAGS_DEBUG "")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-Og -g3)
else()
  add_compile_options(-O2 -g3)
endif()

# ================================================================ HAL 库
add_library(hal STATIC
  ${HAL_DIR}/Src/stm32g4xx_hal.c
  ${HAL_DIR}/Src/stm32g4xx_hal_rcc.c
  ${HAL_DIR}/Src/stm32g4xx_hal_rcc_ex.c
  ${HAL_DIR}/Src/stm32g4xx_hal_pwr.c
  ${HAL_DIR}/Src/stm32g4xx_hal_pwr_ex.c
  ${HAL_DIR}/Src/stm32g4xx_hal_gpio.c
  ${HAL_DIR}/Src/stm32g4xx_hal_iwdg.c
  ${HAL_DIR}/Src/stm32g4xx_hal_pcd.c
  ${HAL_DIR}/Src/stm32g4xx_hal_pcd_ex.c
  ${HAL_DIR}/Src/stm32g4xx_ll_usb.c
  ${HAL_DIR}/Src/stm32g4xx_hal_dma.c
  ${HAL_DIR}/Src/stm32g4xx_hal_cortex.c
  ${HAL_DIR}/Src/stm32g4xx_hal_flash.c
  ${HAL_DIR}/Src/stm32g4xx_hal_flash_ex.c
  ${HAL_DIR}/Src/stm32g4xx_hal_uart.c
  ${HAL_DIR}/Src/stm32g4xx_hal_uart_ex.c
  ${HAL_DIR}/Src/stm32g4xx_hal_tim.c
  ${HAL_DIR}/Src/stm32g4xx_hal_tim_ex.c
)
target_include_directories(hal PUBLIC
  ${HAL_DIR}/Inc
  ${CMSIS_DEV_DIR}/Include
  ${CMSIS_CORE_DIR}
  ${CMAKE_SOURCE_DIR}/config
)
target_compile_definitions(hal PUBLIC ${TARGET_DEFS})
target_compile_options(hal PRIVATE -w)

# ================================================================ FreeRTOS
add_library(freertos STATIC
  ${FREERTOS_DIR}/tasks.c
  ${FREERTOS_DIR}/queue.c
  ${FREERTOS_DIR}/list.c
  ${FREERTOS_DIR}/timers.c
  ${FREERTOS_DIR}/event_groups.c
  ${FREERTOS_DIR}/stream_buffer.c
  ${FREERTOS_PORT}/port.c
  ${FREERTOS_DIR}/portable/MemMang/heap_4.c
)
target_include_directories(freertos PUBLIC
  ${FREERTOS_DIR}/include
  ${FREERTOS_PORT}
  ${CMAKE_SOURCE_DIR}/config
)
target_compile_definitions(freertos PUBLIC ${TARGET_DEFS})
target_compile_options(freertos PRIVATE -w)


# ================================================================ USB Device 中间件（第三方，M4 前置）
set(MW_USB_DIR ${CUBE_DIR}/Middlewares/ST/STM32_USB_Device_Library)
add_library(usb_mw STATIC
  ${MW_USB_DIR}/Core/Src/usbd_core.c
  ${MW_USB_DIR}/Core/Src/usbd_ctlreq.c
  ${MW_USB_DIR}/Core/Src/usbd_ioreq.c
  ${MW_USB_DIR}/Class/CDC/Src/usbd_cdc.c
)
target_include_directories(usb_mw PUBLIC
  ${MW_USB_DIR}/Core/Inc
  ${MW_USB_DIR}/Class/CDC/Inc
  ${CMAKE_SOURCE_DIR}/config
)
target_compile_definitions(usb_mw PUBLIC ${TARGET_DEFS})
target_compile_options(usb_mw PRIVATE -w)
target_link_libraries(usb_mw PUBLIC hal)

# ================================================================ 平台库（bsp + core，业务无关）
# 注意：覆盖 HAL/库 weak 符号的文件（hal_timebase/msp/retarget/freertos_hooks）
# 必须编入 app 可执行文件而非静态库，否则静态库链接顺序会导致 weak 实现胜出
add_library(platform STATIC
  bsp/board.c
  bsp/clock.c
  bsp/iwdg.c
  bsp/led.c
  bsp/uart.c
  core/osal/osal_freertos.c
  core/log/log.c
  core/fault/fault.c
  core/util/rb.c
)
target_include_directories(platform PUBLIC
  ${CMAKE_SOURCE_DIR}/bsp
  ${CMAKE_SOURCE_DIR}/core
  ${CMAKE_SOURCE_DIR}/core/osal
  ${CMAKE_SOURCE_DIR}/core/log
  ${CMAKE_SOURCE_DIR}/core/fault
  ${CMAKE_SOURCE_DIR}/core/util
  ${CMAKE_SOURCE_DIR}/config
)
target_link_libraries(platform PUBLIC hal freertos)
target_compile_options(platform PRIVATE -Wall -Wextra)

# ================================================================ Bootloader
# 极简引导（0x08000000，32KB）：校验 app 向量表后跳转，详见 bootloader/main.c
add_executable(bootloader
  bootloader/main.c
  bootloader/startup/startup_stm32g474xx.s
  bsp/system/system_stm32g4xx.c
)
target_include_directories(bootloader PRIVATE
  ${CMSIS_DEV_DIR}/Include
  ${CMSIS_CORE_DIR}
)
target_compile_definitions(bootloader PRIVATE STM32G474xx)
target_compile_options(bootloader PRIVATE -Wall -Wextra)
target_link_options(bootloader PRIVATE
  -T ${CMAKE_SOURCE_DIR}/bootloader/linker/stm32g474ret6_boot.ld
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_BINARY_DIR}/bootloader.map
  --specs=nano.specs
  --specs=nosys.specs
)
set_target_properties(bootloader PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_custom_command(TARGET bootloader POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bootloader> ${CMAKE_BINARY_DIR}/bin/bootloader.bin
  COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:bootloader> ${CMAKE_BINARY_DIR}/bin/bootloader.hex
  COMMENT "生成 bootloader.bin / bootloader.hex"
)

# ================================================================ 应用
add_executable(app
  app/main.c
  bsp/startup/startup_stm32g474xx.s
  bsp/system/system_stm32g4xx.c
  core/osal/freertos_hooks.c
  core/fault/fault_arm.c
  bsp/hal_timebase.c
  bsp/msp.c
  bsp/retarget.c
  bsp/usb_cdc.c
  app/tasks/task_demo.c
  app/tasks/task_sysmon.c
  app/tasks/task_wdg.c
)
target_include_directories(app PRIVATE
  ${CMAKE_SOURCE_DIR}/app
  ${CMAKE_SOURCE_DIR}/bsp
  ${CMAKE_SOURCE_DIR}/core
  ${CMAKE_SOURCE_DIR}/core/osal
  ${CMAKE_SOURCE_DIR}/core/log
  ${CMAKE_SOURCE_DIR}/core/fault
  ${CMAKE_SOURCE_DIR}/core/util
  ${CMAKE_SOURCE_DIR}/config
  ${HAL_DIR}/Inc
  ${CMSIS_DEV_DIR}/Include
  ${CMSIS_CORE_DIR}
  ${FREERTOS_DIR}/include
  ${FREERTOS_PORT}
)
target_link_libraries(app PRIVATE platform usb_mw)
target_compile_options(app PRIVATE -Wall -Wextra)

# 应用从 0x08008000 开始（前 32KB 留给 Bootloader），向量表偏移见 docs/flash-partition.md
# USER_VECT_TAB_ADDRESS：system_stm32g4xx.c（v1.6.3）必须定义此宏才会写 SCB->VTOR
# （否则 app 向量表留在 0x08000000，仅靠 bootloader 跳转前设置 VTOR 兜底）
target_compile_definitions(app PRIVATE
  ${TARGET_DEFS}
  VECT_TAB_OFFSET=0x00008000U
  USER_VECT_TAB_ADDRESS
)

target_link_options(app PRIVATE
  -T ${CMAKE_SOURCE_DIR}/bsp/linker/stm32g474ret6_app.ld
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_BINARY_DIR}/app.map
  -Wl,--undefined=_write
  --specs=nano.specs
  --specs=nosys.specs
)

# ---------------------------------------------------------------- 产物
set_target_properties(app PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_custom_command(TARGET app POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:app> ${CMAKE_BINARY_DIR}/bin/app.bin
  COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:app> ${CMAKE_BINARY_DIR}/bin/app.hex
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:app>
  COMMENT "生成 app.bin / app.hex"
)
