#!/usr/bin/env bash
# 构建脚本（用法：tools/build.sh [Debug|Release]，默认 Release）
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_TYPE="${1:-Release}"

# 工具链自动定位（Windows + winget 安装的常见路径；其他环境需自行加入 PATH）
find_tool() {
  command -v "$1" >/dev/null 2>&1 && return 0
  case "$1" in
    cmake)
      for d in "/c/Program Files/CMake/bin"; do
        [ -x "$d/$1.exe" ] && export PATH="$d:$PATH" && return 0
      done ;;
    arm-none-eabi-gcc)
      for d in "/c/Program Files (x86)/Arm GNU Toolchain"*/*/bin \
               "/c/Program Files/Arm GNU Toolchain"*/*/bin; do
        [ -x "$d/$1.exe" ] && export PATH="$d:$PATH" && return 0
      done ;;
    ninja)
      for d in /c/Users/*/AppData/Local/Microsoft/WinGet/Packages/Ninja-build.Ninja*/; do
        [ -x "$d/ninja.exe" ] && export PATH="$d:$PATH" && return 0
      done ;;
  esac
  return 1
}

find_tool cmake   || { echo "缺少 cmake"; exit 1; }
find_tool ninja   || { echo "缺少 ninja（或改用 -G \"Unix Makefiles\"）"; }
find_tool arm-none-eabi-gcc || { echo "缺少 arm-none-eabi-gcc"; exit 1; }

cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

cmake --build build
