#!/usr/bin/env bash
# 通过 J-Link 烧录应用（app.bin -> 0x08008000）
# 用法：tools/flash_jlink.sh
set -euo pipefail
cd "$(dirname "$0")/.."

JLINK=""
for d in "/c/Program Files/SEGGER"/JLink*/ "/c/Program Files (x86)/SEGGER"/JLink*/; do
  if [ -x "${d}JLink.exe" ]; then
    JLINK="${d}JLink.exe"
    break
  fi
done
if [ -z "$JLINK" ]; then
  echo "未找到 J-Link 软件，请从 https://www.segger.com/downloads/jlink/ 安装" >&2
  exit 1
fi

echo "==> 使用 $JLINK"
echo "==> 目标: STM32G474RE (SWD, 4MHz)"

"$JLINK" -device STM32G474RE -if SWD -speed 4000 -autoconnect 1 \
  -CommanderScript tools/flash_app.jlink
