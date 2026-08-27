#!/usr/bin/env bash
# 安装 git hooks（pre-commit：编译 + 单测 + MISRA + 格式检查）
#
# hook 单一真源：<skill>/templates/githooks/（本仓库 = .pi/skills/stm32g474-devtools/templates/githooks）
# 升级 skill 后重新运行本脚本即可刷新。
#
# 用法（项目根执行）：bash <skill>/scripts/install_hooks.sh
set -euo pipefail

# 定位项目根（含 devtool.conf）：优先 cwd，否则按脚本位置查找
ROOT="$(pwd)"
if [ ! -f "$ROOT/devtool.conf" ]; then
  d="$(cd "$(dirname "$0")" && pwd)"
  while [ "$d" != "/" ] && [ ! -f "$d/devtool.conf" ]; do d="$(dirname "$d")"; done
  [ -f "$d/devtool.conf" ] && ROOT="$d"
fi
cd "$ROOT"

HOOKS_DIR="$(pwd)/.pi/skills/stm32g474-devtools/templates/githooks"
if [ ! -f "$HOOKS_DIR/pre-commit" ]; then
  echo "错误: 未找到 hook 模板: $HOOKS_DIR/pre-commit" >&2
  exit 1
fi

git config core.hooksPath "$HOOKS_DIR"
echo "==> git hooks 已安装: core.hooksPath = $HOOKS_DIR"
echo "    提交时将自动执行：编译 + PC 单测 + MISRA + 格式检查"
echo "    紧急跳过：SKIP_CHECKS=1 git commit ... 或 git commit --no-verify"