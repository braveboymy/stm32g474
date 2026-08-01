#!/usr/bin/env bash
# MISRA C:2012 静态检查（cppcheck + misra addon）
# 范围：本项目代码（core/bsp/app/bootloader），第三方（third_party/）豁免
# 规则子集与豁免理由见 AGENTS.md「MISRA C:2012」章节
# 用法：tools/check_misra.sh [--fix-dir DIR]
set -euo pipefail
cd "$(dirname "$0")/.."

CPPCHECK=""
for p in "/c/Program Files/Cppcheck/cppcheck.exe" "/usr/bin/cppcheck" "$(command -v cppcheck 2>/dev/null || true)"; do
  if [ -n "$p" ] && [ -x "$p" ]; then CPPCHECK="$p"; break; fi
done
if [ -z "$CPPCHECK" ]; then
  echo "未找到 cppcheck，请安装：winget install Cppcheck.Cppcheck" >&2
  exit 1
fi

ADDON="$(pwd)/tools/cppcheck-addons/misra.json"
DIRS="core bsp app bootloader"
if [ -n "${1:-}" ]; then
  DIRS="$1"
fi

CUBE="third_party/STM32CubeG4"
FREERTOS="third_party/FreeRTOS-Kernel"
INCLUDES="-Iconfig -Icore -Ibsp -Iapp"
INCLUDES="$INCLUDES -I$CUBE/Drivers/STM32G4xx_HAL_Driver/Inc"
INCLUDES="$INCLUDES -I$CUBE/Drivers/CMSIS/Device/ST/STM32G4xx/Include"
INCLUDES="$INCLUDES -I$CUBE/Drivers/CMSIS/Include"
INCLUDES="$INCLUDES -I$FREERTOS/include -I$FREERTOS/portable/GCC/ARM_CM4F"

DEFS="-DSTM32G474xx -DUSE_HAL_DRIVER -DVECT_TAB_OFFSET=0x00008000U"

echo "==> MISRA C:2012 检查（cppcheck --addon=misra）"
echo "    范围: $DIRS（第三方代码豁免）"
echo "    规则子集: tools/cppcheck-addons/misra.json"

OUT="$("$CPPCHECK" --std=c11 --addon="$ADDON" $INCLUDES $DEFS \
  --suppress=missingIncludeSystem --suppress=misra-config --inline-suppr \
  --enable=warning,style $DIRS 2>&1 || true)"

VIOLATIONS="$(echo "$OUT" | grep -a "misra" | grep -av "use --rule-texts" || true)"

if [ -z "$VIOLATIONS" ]; then
  echo "==> ✅ 无 MISRA 违规"
  exit 0
fi

echo "$VIOLATIONS" | sed 's/(style)//'
echo ""
echo "==> ❌ 存在 MISRA 违规（$(
  echo "$VIOLATIONS" | grep -ac 'misra'
) 处）。处置方式："
echo "    1. 修复代码"
echo "    2. 或加行内豁免注释（仅限合理场景）：// cppcheck-suppress misra-c2012-<规则号>"
echo "    3. 若认为规则不适用于本场景，在 AGENTS.md MISRA 章节补充豁免理由"
exit 1
