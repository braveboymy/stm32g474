#!/usr/bin/env bash
# 拉取并固定第三方依赖（版本锁定见 third_party/README.md）
set -euo pipefail
cd "$(dirname "$0")/.."

FREERTOS_TAG="V11.1.0"
CUBE_TAG="v1.6.3"

mkdir -p third_party

if [ ! -d third_party/FreeRTOS-Kernel/.git ]; then
  echo "==> cloning FreeRTOS-Kernel @ ${FREERTOS_TAG}"
  git clone --depth 1 --branch "${FREERTOS_TAG}" \
    https://github.com/FreeRTOS/FreeRTOS-Kernel.git third_party/FreeRTOS-Kernel
fi

if [ ! -d third_party/STM32CubeG4/.git ]; then
  echo "==> cloning STM32CubeG4 @ ${CUBE_TAG}（约 500MB，一次性）"
  git clone --depth 1 --branch "${CUBE_TAG}" \
    https://github.com/STMicroelectronics/STM32CubeG4.git third_party/STM32CubeG4
  echo "==> 初始化 HAL / CMSIS / BSP 子模块"
  git -C third_party/STM32CubeG4 submodule update --init --depth 1 \
    Drivers/STM32G4xx_HAL_Driver \
    Drivers/CMSIS/Device/ST/STM32G4xx \
    Drivers/BSP/STM32G4xx_Nucleo
fi

echo "==> done"
