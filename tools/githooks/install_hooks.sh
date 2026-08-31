#!/usr/bin/env bash
# 安装 git hooks（pre-commit：编译 + 单测 + MISRA + 格式检查）
# 用法：bash tools/githooks/install_hooks.sh
# 说明：core.hooksPath 指向版本库内目录，钩子随仓库分发（克隆后一条命令即装好）
set -euo pipefail
cd "$(dirname "$0")/../.."

HOOKS_DIR="$(pwd)/tools/githooks"
chmod +x tools/githooks/pre-commit 2>/dev/null || true
git config core.hooksPath "$HOOKS_DIR"
echo "==> git hooks 已安装: core.hooksPath = $HOOKS_DIR"
echo "    提交时将自动执行：编译 + core 单测 + MISRA（本次改动文件）+ clang-format"
echo "    紧急跳过：SKIP_CHECKS=1 git commit ... 或 git commit --no-verify（事后补跑检查）"
echo "    卸载：git config --unset core.hooksPath"