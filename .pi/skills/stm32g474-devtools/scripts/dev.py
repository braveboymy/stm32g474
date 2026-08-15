#!/usr/bin/env python3
"""STM32G474 平台开发闭环脚本（项目级 skill：stm32g474-devtools）

用法（项目任意目录）：
  dev.py info                        # 工具链与固件信息
  dev.py build [Debug|Release]       # 编译固件
  dev.py flash                       # 烧录 bootloader(0x08000000) + app(0x08008000)
  dev.py connect                     # J-Link 连接测试
  dev.py status                      # 寄存器级验证固件运行（TIM6 tick / LED）
  dev.py log [--tail N]              # J-Link 读 RAM 日志镜像（无需串口）
  dev.py console [--list]            # 串口日志（需 ST-LINK VCP / pyserial）
  dev.py regs                        # 读取 CPU 寄存器（卡死定位）
  dev.py verify [--tail N]           # 一键闭环：build -> flash -> status -> log

说明：
  - 编译/烧录/状态/日志等核心操作复用 tools/devtool.py（uv 管理环境）
  - regs 直接走 J-Link Commander（复用 devtool 的封装）
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import time
from pathlib import Path

# .pi/skills/<name>/scripts/dev.py -> parents[4] = 项目根
ROOT = Path(__file__).resolve().parents[4]
DEVTOOL = ROOT / "tools" / "devtool.py"
sys.path.insert(0, str(ROOT / "tools"))
import devtool  # noqa: E402  复用 J-Link 封装（run_jlink/find_tool 等）


def python_cmd() -> list[str]:
    """优先项目 .venv，其次 uv run，最后系统 Python"""
    for p in (ROOT / ".venv" / "Scripts" / "python.exe", ROOT / ".venv" / "bin" / "python"):
        if p.exists():
            return [str(p)]
    uv = shutil.which("uv")
    if uv:
        return [uv, "run", "python"]
    return [sys.executable]


def devtool_run(args: list[str]) -> int:
    cmd = python_cmd() + [str(DEVTOOL)] + args
    print(f"==> {' '.join(str(c) for c in cmd)}")
    return subprocess.call(cmd, cwd=ROOT)


def cmd_regs(args) -> int:
    print(f"==> 连接 {devtool.JLINK_DEVICE} 并读取 CPU 寄存器（halt 后）")
    rc, out, log = devtool.run_jlink(["connect", "h", "regs", "q"], "jlink_regs.log", args.timeout)
    if rc != 0 or devtool.jlink_hard_fail(out):
        print(f"==> 读取失败（日志: {log}）")
        return 1
    for line in out.splitlines():
        s = line.strip()
        if s.startswith(("PC =", "SP(", "MSP", "PSP", "LR", "XPSR", "IPSR", "R0 ", "R1 ", "R2 ", "R3 ", "R4 ", "R5 ", "R12")):
            print(s)
    print("==> PC 对应源码：")
    nm = devtool.find_tool("arm-none-eabi-gcc")
    if nm:
        m = re_search_pc(out)
        if m:
            addr2line = nm.parent / "arm-none-eabi-addr2line"
            subprocess.call([str(addr2line), "-e", str(ROOT / "build" / "bin" / "app"), "-f", m])
    return 0


def re_search_pc(out: str) -> str | None:
    import re
    m = re.search(r"PC = ([0-9A-Fa-f]{8})", out)
    return m.group(1) if m else None


def cmd_verify(args) -> int:
    for step in ("build", "flash"):
        if devtool_run([step]) != 0:
            return 1
    print("==> 等待固件运行 3 秒...")
    time.sleep(3)
    if devtool_run(["status"]) != 0:
        return 1
    return devtool_run(["log", "--tail", str(args.tail)])


def main() -> int:
    if sys.stdout and hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", line_buffering=True)
        sys.stderr.reconfigure(encoding="utf-8", line_buffering=True)

    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    # 透传子命令：第一个参数是命令名，其余参数原样转给 tools/devtool.py
    passthrough = ("info", "build", "flash", "connect", "status", "log", "console", "test")
    if sys.argv[1] in passthrough:
        return devtool_run([sys.argv[1]] + sys.argv[2:])

    p = argparse.ArgumentParser(prog="dev", description="STM32G474 开发闭环（项目级 skill 脚本）")
    sub = p.add_subparsers(dest="cmd", required=True)

    r = sub.add_parser("regs", help="读取 CPU 寄存器（卡死定位）")
    r.add_argument("--timeout", type=int, default=90)

    v = sub.add_parser("verify", help="一键闭环：build -> flash -> status -> log")
    v.add_argument("--tail", type=int, default=40)
    v.add_argument("build_type", nargs="?", default="Release", choices=["Debug", "Release"])

    args = p.parse_args()

    if args.cmd == "regs":
        return cmd_regs(args)
    if args.cmd == "verify":
        return cmd_verify(args)
    return 1


if __name__ == "__main__":
    sys.exit(main())
