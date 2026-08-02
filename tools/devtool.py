#!/usr/bin/env python3
"""STM32G474 平台开发工具（uv 管理环境，零第三方依赖）

用法（项目根目录执行）：
    uv run python tools/devtool.py info                    # 工具链/固件信息
    uv run python tools/devtool.py build [Debug|Release]   # 编译固件
    uv run python tools/devtool.py connect                 # 测试 J-Link 连接
    uv run python tools/devtool.py flash                   # 烧录 + 校验 + 复位运行

返回码：0 成功，1 失败（含超时/连接失败）。
"""

from __future__ import annotations

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build"
BIN_DIR = BUILD_DIR / "bin"
APP_BIN = BIN_DIR / "app.bin"
APP_BIN_ADDR = "0x08008000"
BOOT_BIN = BIN_DIR / "bootloader.bin"
BOOT_BIN_ADDR = "0x08000000"
JLINK_DEVICE = "STM32G474RE"
JLINK_SPEED_KHZ = "4000"

# 工具链探测：PATH 优先，其次常见安装位置（Windows）
TOOL_PATTERNS = {
    "cmake": [
        "C:/Program Files/CMake/bin/cmake.exe",
        "C:/Program Files (x86)/CMake/bin/cmake.exe",
    ],
    "ninja": [
        "C:/Users/*/AppData/Local/Microsoft/WinGet/Packages/Ninja-build.Ninja*/ninja.exe",
    ],
    "arm-none-eabi-gcc": [
        "C:/Program Files (x86)/Arm GNU Toolchain*/*/bin/arm-none-eabi-gcc.exe",
        "C:/Program Files/Arm GNU Toolchain*/*/bin/arm-none-eabi-gcc.exe",
    ],
    "JLink.exe": [
        "C:/Program Files/SEGGER/JLink*/JLink.exe",
        "C:/Program Files (x86)/SEGGER/JLink*/JLink.exe",
    ],
}


class ToolError(Exception):
    pass


def find_tool(name: str) -> Path | None:
    exe = shutil.which(name)
    if exe:
        return Path(exe)
    hits = [Path(p) for pat in TOOL_PATTERNS.get(name, []) for p in glob.glob(pat)]
    if not hits:
        return None

    # 环境变量 JLINK_VERSION 可指定优先版本（如 JLINK_VERSION=688 选 JLink_V688c，
    # 用于避开克隆 J-Link 的检测问题；不设置时保持取版本号最大者）
    pref = os.environ.get("JLINK_VERSION")
    if pref and name == "JLink.exe":
        for p in hits:
            m = re.search(r"V?(\d+(?:\.\d+)*)", p.name)
            if m and m.group(1).startswith(pref):
                return p

    # 多版本命中时取版本号最大者（如 JLink_V964 > JLink_V688）
    def ver_key(p: Path) -> tuple:
        m = re.search(r"V?(\d+(?:\.\d+)*)", p.name)
        return tuple(int(x) for x in m.group(1).split(".")) if m else (0,)

    return max(hits, key=ver_key)


def need_tool(name: str) -> Path:
    p = find_tool(name)
    if p is None:
        raise ToolError(f"未找到 {name}，请安装或加入 PATH")
    return p


def run_stream(cmd: list, timeout: int, cwd: Path | None = None) -> tuple[int, str]:
    """流式运行命令，返回 (returncode, 完整输出)。超时则终止并报错。"""
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=cwd,
        text=True,
        bufsize=1,
        encoding="utf-8",
        errors="replace",
    )
    lines: list[str] = []

    def reader() -> None:
        assert proc.stdout is not None
        for line in proc.stdout:
            lines.append(line)
            sys.stdout.write(line)
            sys.stdout.flush()

    t = threading.Thread(target=reader, daemon=True)
    t.start()
    try:
        rc = proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        t.join(timeout=5)
        raise ToolError(f"命令超时（>{timeout}s），已终止: {' '.join(str(x) for x in cmd)}")
    t.join()
    return rc, "".join(lines)


# ---------------------------------------------------------------------------
# build
# ---------------------------------------------------------------------------

def build_type_in_cache() -> str | None:
    cache = BUILD_DIR / "CMakeCache.txt"
    if not cache.exists():
        return None
    m = re.search(r"CMAKE_BUILD_TYPE:STRING=(\w+)", cache.read_text(encoding="utf-8", errors="replace"))
    return m.group(1) if m else None


def cmd_build(args) -> int:
    cmake = need_tool("cmake")
    need_tool("arm-none-eabi-gcc")
    ninja = find_tool("ninja")
    if ninja is None:
        raise ToolError("未找到 ninja")
    # 把工具目录注入子进程 PATH（cmake 需要 arm-none-eabi-gcc/ninja 可执行）
    for name in ("cmake", "ninja", "arm-none-eabi-gcc"):
        p = find_tool(name)
        if p is not None:
            os.environ["PATH"] = str(p.parent) + os.pathsep + os.environ.get("PATH", "")
    if build_type_in_cache() != args.build_type:
        print(f"==> 配置 CMake（{args.build_type}）")
        rc, _ = run_stream(
            [
                str(cmake), "-S", str(ROOT), "-B", str(BUILD_DIR), "-G", "Ninja",
                f"-DCMAKE_MAKE_PROGRAM={ninja}",
                "-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake",
                f"-DCMAKE_BUILD_TYPE={args.build_type}",
            ],
            timeout=120,
        )
        if rc != 0:
            raise ToolError("CMake 配置失败")
    print("==> 编译")
    rc, _ = run_stream([str(cmake), "--build", str(BUILD_DIR)], timeout=600)
    if rc != 0:
        raise ToolError("编译失败")
    if APP_BIN.exists():
        print(f"==> OK: {APP_BIN}（{APP_BIN.stat().st_size} 字节）烧录地址 {APP_BIN_ADDR}")
    return 0


# ---------------------------------------------------------------------------
# J-Link（connect / flash）
# ---------------------------------------------------------------------------

def jlink_script(commands: list[str]) -> str:
    fd, path = tempfile.mkstemp(suffix=".jlink", prefix="devtool_", text=True)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write("\n".join(commands) + "\n")
    return path


def run_jlink(script_commands: list[str], log_name: str, timeout: int) -> tuple[int, str, Path]:
    jlink = need_tool("JLink.exe")
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    script = jlink_script(script_commands)
    log_path = BUILD_DIR / log_name
    cmd = [
        str(jlink), "-device", JLINK_DEVICE, "-if", "SWD",
        "-speed", JLINK_SPEED_KHZ, "-autoconnect", "1", "-NoGui", "1",
        "-Log", str(log_path), "-CommanderScript", script,
    ]
    try:
        rc, out = run_stream(cmd, timeout=timeout)
    finally:
        os.unlink(script)
    return rc, out, log_path


def jlink_hard_fail(out: str) -> bool:
    return any(s in out for s in ("Cannot connect", "FAILED", "Error:"))


def cmd_connect(args) -> int:
    print(f"==> 连接 {JLINK_DEVICE}（SWD {JLINK_SPEED_KHZ}kHz）")
    rc, out, log = run_jlink(["connect", "q"], "jlink_connect.log", args.timeout)
    ok = any(s in out for s in ("Cortex-M4 processor detected", "Found SW-DP", "Connected successfully"))
    if rc != 0 or jlink_hard_fail(out) or not ok:
        print(f"==> 连接失败（日志: {log}）")
        return 1
    print("==> 连接成功：目标已响应")
    return 0


def cmd_flash(args) -> int:
    if not APP_BIN.exists():
        raise ToolError(f"固件不存在：{APP_BIN}（先运行 build）")
    if not BOOT_BIN.exists():
        raise ToolError(f"引导程序不存在：{BOOT_BIN}（先运行 build）")
    print(f"==> 烧录 {BOOT_BIN.name} -> {BOOT_BIN_ADDR}，{APP_BIN.name} -> {APP_BIN_ADDR}")
    rc, out, log = run_jlink(
        [
            "r",
            f"loadbin {BOOT_BIN.as_posix()} {BOOT_BIN_ADDR}",
            f"verifybin {BOOT_BIN.as_posix()} {BOOT_BIN_ADDR}",
            f"loadbin {APP_BIN.as_posix()} {APP_BIN_ADDR}",
            f"verifybin {APP_BIN.as_posix()} {APP_BIN_ADDR}",
            "r",
            "g",
            "q",
        ],
        "jlink_flash.log",
        args.timeout,
    )
    verified = out.count("Verify successful") >= 2
    if rc != 0 or jlink_hard_fail(out) or not verified:
        print(f"==> 烧录失败（日志: {log}）")
        return 1
    print("==> 烧录并校验成功，目标已复位运行")
    return 0


# ---------------------------------------------------------------------------
# status：读寄存器验证固件运行状态
# ---------------------------------------------------------------------------

REG_READS = [
    ("0x40001024", "TIM6 CNT（HAL 时间基准，1kHz 递增）"),
    ("0x48000014", "GPIOA ODR（LED PA5 位 5）"),
]


def parse_mem32(out: str) -> list[tuple[str, int]]:
    """按输出顺序返回 [(地址, 值), ...]，兼容 JLink mem32 输出：
    每行格式 'AAAAAAAA = v0 v1 v2 v3'（无 0x 前缀，一行多值）"""
    results: list[tuple[str, int]] = []
    for line in out.splitlines():
        m = re.match(r"([0-9A-Fa-f]{8})\s*=\s*(.+)", line.strip())
        if not m:
            continue
        addr = int(m.group(1), 16)
        for i, v in enumerate(m.group(2).split()):
            if re.fullmatch(r"[0-9A-Fa-f]{8}", v):
                results.append((f"{addr + i * 4:08x}", int(v, 16)))
    return results


def cmd_status(args) -> int:
    n = args.samples
    print(f"==> 连接 {JLINK_DEVICE} 并采样寄存器（{n} 次采样，间隔 {args.interval}ms）")
    script = ["connect"]
    for i in range(n):
        for reg, _ in REG_READS:
            script.append(f"mem32 {reg}, 1")
        if i < n - 1:
            script.append(f"sleep {args.interval}")
    script.append("q")
    rc, out, log = run_jlink(script, "jlink_status.log", args.timeout)
    if rc != 0 or jlink_hard_fail(out):
        print(f"==> 读取失败（日志: {log}）")
        return 1
    samples = parse_mem32(out)
    ok = True
    for reg, desc in REG_READS:
        r = reg.lower().replace("0x", "")
        seq = [v for k, v in samples if k == r]
        if len(seq) < 2:
            print(f"  {desc:<32}: 采样不完整")
            ok = False
            continue
        alive = len(set(seq)) > 1
        print(f"  {desc:<32}: {['0x%08X' % v for v in seq]} {'运行中' if alive else '无变化'}")
        # 硬指标：TIM6（HAL 时间基准，外设不受 CPU halt 影响）
        # ODR/LED 在 J-Link halt 时任务暂停，仅作参考不判失败
        if "TIM6" in desc and not alive:
            ok = False
    if ok:
        print("==> 固件运行正常：时间基准活跃（TIM6 递增）")
        return 0
    print("==> 固件可能未运行或已停机（详见日志）")
    return 1


# ---------------------------------------------------------------------------
# info
# ---------------------------------------------------------------------------

def cmd_info(args) -> int:
    print(f"项目根目录: {ROOT}")
    for name in ("cmake", "ninja", "arm-none-eabi-gcc", "JLink.exe"):
        p = find_tool(name)
        print(f"  {name:<18}: {p if p else '未找到'}")
    if APP_BIN.exists():
        print(f"固件: {APP_BIN}（{APP_BIN.stat().st_size} 字节，烧录地址 {APP_BIN_ADDR}）")
    else:
        print("固件: 尚未构建（先运行 build）")
    return 0


# ---------------------------------------------------------------------------
# console：串口查看日志（pyserial）
# ---------------------------------------------------------------------------

def cmd_console(args) -> int:
    import serial
    from serial.tools import list_ports

    if args.list:
        ports = list(list_ports.comports())
        if not ports:
            print("未发现串口")
            return 1
        for p in ports:
            print(f"  {p.device}: {p.description}")
        return 0

    port = args.port
    if port is None:
        cands = [p for p in list_ports.comports() if "STLink" in p.description or "ST-LINK" in p.description]
        if not cands:
            cands = list(list_ports.comports())
        if not cands:
            raise ToolError("未找到串口，先用 console --list 查看或用 console <COMx> 指定")
        port = cands[0].device
    print(f"==> 打开 {port} @ {args.baud}，Ctrl+C 退出")
    with serial.Serial(port, args.baud, timeout=0.1) as ser:
        try:
            while True:
                data = ser.read(4096)
                if data:
                    sys.stdout.write(data.decode("utf-8", errors="replace"))
                    sys.stdout.flush()
        except KeyboardInterrupt:
            pass
    return 0


# ---------------------------------------------------------------------------
# log：通过 J-Link 读 RAM 日志镜像（core/log 的 log_enable_ram）
# ---------------------------------------------------------------------------

LOG_RAM_BUF_SIZE = 2048  # 与 core/log/log.c 保持一致


def parse_map_symbol(map_path: Path, symbol: str) -> int | None:
    """从 app.map 解析符号地址。map 中符号名与地址分处两行：
       .bss.s_ram_buf
                    0x20000a40      0x800 libplatform.a(log.c.obj)"""
    if not map_path.exists():
        return None
    lines = map_path.read_text(encoding="utf-8", errors="replace").splitlines()
    for i, line in enumerate(lines):
        if f".bss.{symbol}" in line or line.strip().endswith(symbol):
            for cand in (line, lines[i + 1] if i + 1 < len(lines) else ""):
                m = re.search(r"0x([0-9a-fA-F]{8})", cand)
                if m:
                    return int(m.group(1), 16)
    return None


def cmd_log(args) -> int:
    import serial  # noqa: F401  确保依赖存在（提示信息友好）

    buf_addr = parse_map_symbol(BUILD_DIR / "app.map", "s_ram_buf")
    rb_addr = parse_map_symbol(BUILD_DIR / "app.map", "s_ram_rb")
    if buf_addr is None or rb_addr is None:
        raise ToolError("map 文件中未找到日志缓冲符号（先 build，并确认 log_enable_ram 已调用）")
    print(f"==> 读取 RAM 日志镜像 @ 0x{buf_addr:08X}（环形缓冲 {LOG_RAM_BUF_SIZE} 字节）")

    bin_path = BUILD_DIR / "log_ram.bin"
    script = ["connect", "h"]
    # rb_t 布局：buf(4) size(4) head(4) tail(4)（4 字小读取 mem32 正常）
    script.append(f"mem32 0x{rb_addr:08X}, 4")
    # 日志数据用 savebin 读取（注意：J-Link V9.64 的 count 参数按十六进制解析）
    script.append(f"savebin {bin_path.as_posix()}, 0x{buf_addr:08X}, 0x{LOG_RAM_BUF_SIZE:X}")
    script.append("q")
    rc, out, log = run_jlink(script, "jlink_log.log", args.timeout)
    if rc != 0 or jlink_hard_fail(out):
        print(f"==> 读取失败（日志: {log}）")
        return 1

    samples = parse_mem32(out)
    # rb_t 布局：buf(0) size(4) head(8) tail(12)，用地址范围过滤（startswith 会漏掉后续地址）
    rb_words = [
        v
        for _, v in sorted((k, v) for k, v in samples if rb_addr <= int(k, 16) < rb_addr + 16)
    ]
    if len(rb_words) < 4:
        print("==> 环形缓冲元数据读取失败")
        return 1
    size, head, tail = rb_words[1], rb_words[2], rb_words[3]
    if size > LOG_RAM_BUF_SIZE:
        print(f"==> 环形缓冲 size 异常：{size}")
        return 1
    if not bin_path.exists() or bin_path.stat().st_size < LOG_RAM_BUF_SIZE:
        print(f"==> 日志数据读取失败（{bin_path}）")
        return 1
    data = bin_path.read_bytes()[:LOG_RAM_BUF_SIZE]
    # 环形展开：[tail, head) 为有效数据
    if head >= tail:
        ordered = data[tail:head]
    else:
        ordered = data[tail:] + data[:head]
    text = ordered.decode("utf-8", errors="replace")
    lines = [ln for ln in text.split("\r\n") if ln.strip()]
    print(f"==> 最近 {len(lines)} 条日志：")
    for ln in lines[-args.tail:]:
        print(ln)
    return 0


def main() -> int:
    # Windows 控制台默认 GBK，统一为 UTF-8 输出
    if sys.stdout and hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    p = argparse.ArgumentParser(prog="devtool", description="STM32G474 平台开发工具")
    sub = p.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build", help="编译固件")
    b.add_argument("build_type", nargs="?", default="Release", choices=["Debug", "Release"])
    c = sub.add_parser("connect", help="测试 J-Link 连接")
    c.add_argument("--timeout", type=int, default=90)
    f = sub.add_parser("flash", help="烧录固件到 0x08008000 并校验")
    f.add_argument("--timeout", type=int, default=300)
    s = sub.add_parser("status", help="读寄存器验证固件运行状态")
    s.add_argument("--samples", type=int, default=3, help="采样次数")
    s.add_argument("--interval", type=int, default=400, help="采样间隔 ms")
    s.add_argument("--timeout", type=int, default=90)
    c = sub.add_parser("console", help="串口查看日志（需 pyserial）")
    c.add_argument("port", nargs="?", help="COM 口，如 COM3；缺省自动查找")
    c.add_argument("--baud", type=int, default=115200)
    c.add_argument("--list", action="store_true", help="列出可用串口")
    l = sub.add_parser("log", help="J-Link 读 RAM 日志镜像（无需串口）")
    l.add_argument("--tail", type=int, default=40, help="显示最近 N 条")
    l.add_argument("--timeout", type=int, default=90)
    sub.add_parser("info", help="工具链与固件信息")
    args = p.parse_args()
    try:
        return {"build": cmd_build, "connect": cmd_connect, "flash": cmd_flash, "status": cmd_status, "console": cmd_console, "log": cmd_log, "info": cmd_info}[args.cmd](args)
    except ToolError as e:
        print(f"错误: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
