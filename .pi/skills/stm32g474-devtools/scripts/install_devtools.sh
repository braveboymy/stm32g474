#!/usr/bin/env bash
# 在嵌入式项目初始化 devtools skill 的工程脚手架（新项目接入，10 秒完成）
#
# 用法：
#   bash <skill>/scripts/install_devtools.sh [项目根]
#   （缺省目标 = 当前目录）
#
# 生成内容（已存在则跳过，不覆盖）：
#   devtool.conf          # 项目配置（从 devtool.conf.example 复制后按需修改）
#
# pre-commit 钩子单一真源在 <skill>/templates/githooks/，安装执行：
#   bash <skill>/scripts/install_hooks.sh
#
# 命令入口统一使用（不生成转发壳）：
#   python .pi/skills/stm32g474-devtools/scripts/dev.py <build|flash|test|misra|...>
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SKILL_DIR="$(dirname "$SCRIPT_DIR")"

TARGET="${1:-$(pwd)}"
TARGET="$(cd "$TARGET" && pwd)"

copy_if_missing() {
  local src="$1" dst="$2"
  if [ -e "$dst" ]; then
    echo "  - 已存在，跳过: $dst"
  else
    cp "$src" "$dst"
    echo "  - 创建: $dst"
  fi
}

echo "==> 初始化 devtools 工程脚手架: $TARGET"
echo "    skill 目录: $SKILL_DIR"

# 项目配置
copy_if_missing "$SKILL_DIR/templates/devtool.conf.example" "$TARGET/devtool.conf"

echo ""
echo "==> 完成。下一步："
echo "    1. 编辑 $TARGET/devtool.conf（device/分区/bin 名/构建参数/MISRA 配置）"
echo "    2. bash $SCRIPT_DIR/install_hooks.sh                       （安装 pre-commit 钩子，可选）"
echo "    3. python .pi/skills/stm32g474-devtools/scripts/dev.py info   （验证工具链）"
echo "    4. python .pi/skills/stm32g474-devtools/scripts/dev.py misra  （验证 MISRA）"