#!/usr/bin/env bash
# 安装 git hooks（pre-commit：编译 + MISRA 检查）
# 用法：bash tools/install_hooks.sh
set -euo pipefail
cd "$(dirname "$0")/.."

HOOKS_DIR="$(pwd)/tools/githooks"
git config core.hooksPath "$HOOKS_DIR"
echo "==> git hooks 已安装: core.hooksPath = $HOOKS_DIR"
echo "    提交时将自动执行：编译 + MISRA C:2012 检查"
echo "    紧急跳过：SKIP_CHECKS=1 git commit ... 或 git commit --no-verify"
