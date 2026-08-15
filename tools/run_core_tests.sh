#!/usr/bin/env bash
# core + app 层 PC 单测（硬件无关，host gcc 编译运行）
# 用法：bash tools/run_core_tests.sh
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -Werror -g -O0 -I core -I core/osal -I core/util -I core/log -I app -I app/conn -I tests/core"
OUT_DIR="build/tests"
FAILED=0

mkdir -p "$OUT_DIR"

echo "==> PC 单测（core + app 层，host gcc）"

# 每个 test_*.c 独立编译运行（含 mock_osal + 被测模块）
for TEST_SRC in tests/core/test_*.c tests/app/test_*.c; do
  NAME="$(basename "$TEST_SRC" .c)"
  EXE="$OUT_DIR/$NAME.exe"

  # 按测试名链接被测模块（新增测试时在此登记）
  SRC="$TEST_SRC"
  case "$NAME" in
    test_rb)  SRC="$TEST_SRC core/util/rb.c" ;;
    test_log) SRC="$TEST_SRC core/log/log.c core/util/rb.c tests/core/mock_osal.c" ;;
    test_at)  SRC="$TEST_SRC app/conn/at.c" ;;
    *) echo "  ⚠️  未登记被测模块: $NAME（跳过）"; continue ;;
  esac

  if ! $CC $CFLAGS -o "$EXE" $SRC 2> "$OUT_DIR/$NAME.build.log"; then
    echo "  ❌ $NAME 编译失败"
    tail -10 "$OUT_DIR/$NAME.build.log" | sed 's/^/    /'
    FAILED=1
    continue
  fi

  if "$EXE"; then
    echo "  ✅ $NAME 通过"
  else
    echo "  ❌ $NAME 失败"
    FAILED=1
  fi
done

if [ "$FAILED" -ne 0 ]; then
  echo "==> ❌ 单测未全部通过"
  exit 1
fi
echo "==> ✅ PC 单测全部通过"
exit 0
