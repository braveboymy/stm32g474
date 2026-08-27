#!/usr/bin/env python3
"""嵌入式平台开发工具（配置驱动，多项目可复用）

引擎位于 stm32g474-devtools skill 的 scripts/ 下，通过项目根 devtool.conf
读取项目特定参数（设备型号、Flash 分区、构建参数、MISRA/单测配置）。

用法（项目根目录执行，或经 dev.py 透传）：
    python .pi/skills/stm32g474-devtools/scripts/dev.py info                    # 工具链/固件信息
    python .pi/skills/stm32g474-devtools/scripts/dev.py build [Debug|Release]   # 编译固件
    python .pi/skills/stm32g474-devtools/scripts/dev.py connect                 # 测试 J-Link 连接
    python .pi/skills/stm32g474-devtools/scripts/dev.py flash                   # 烧录 + 校验 + 复位运行
    python .pi/skills/stm32g474-devtools/scripts/dev.py misra [dir...]          # MISRA C:2012 检查
    python .pi/skills/stm32g474-devtools/scripts/dev.py test                    # core 层 PC 单测

返回码：0 成功，1 失败（含超时/连接失败）。
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

CONFIG_NAME = "devtool.conf"


# ---------------------------------------------------------------------------
# 配置加载（项目根 = 向上查找 devtool.conf）
# ---------------------------------------------------------------------------

def find_project_root(start: Path | None = None) -> Path:
    """从 start（默认 cwd）向上查找 devtool.conf，返回项目根。"""
    d = (start or Path.cwd()).resolve()
    for p in [d, *d.parents]:
        if (p / CONFIG_NAME).exists():
            return p
    raise ToolError(f"未找到 {CONFIG_NAME}（项目根配置）。新项目可用 install_devtools.sh 初始化。")


def load_config(project_root: Path) -> dict:
    cfg_path = project_root / CONFIG_NAME
    try:
        return json.loads(cfg_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        raise ToolError(f"配置读取失败: {cfg_path}（{e}）")


ROOT = find_project_root()
CFG = load_config(ROOT)

BUILD_DIR = Path(str(CFG["build"]["dir"]))
if not BUILD_DIR.is_absolute():
    BUILD_DIR = ROOT / BUILD_DIR
BIN_DIR = BUILD_DIR / "bin"
JLINK_DEVICE = str(CFG["jlink"]["device"])
JLINK_IF = str(CFG["jlink"].get("interface", "SWD"))
JLINK_SPEED_KHZ = str(CFG["jlink"]["speed"])

BINS = [
    {"name": str(b["name"]), "file": ROOT / str(b["path"]), "addr": str(b["addr"])}
    for b in CFG["bins"]
]
if not BINS:
    raise ToolError(f"{CONFIG_NAME} 的 bins 为空（至少一个固件分区）")

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
    # 固定安装位置优先：PATH 中可能存在同名异质工具（如 JDK 自带的 jlink 模块工具），
    # 会抢走 JLink.exe 的定位；SEGGER 安装目录是权威来源，PATH 仅作兜底
    hits = [Path(p) for pat in TOOL_PATTERNS.get(name, []) for p in glob.glob(pat)]
    if hits:
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

    exe = shutil.which(name)
    if exe:
        return Path(exe)
    return None


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
                str(cmake), "-S", str(ROOT), "-B", str(BUILD_DIR), "-G", str(CFG["build"]["generator"]),
                f"-DCMAKE_MAKE_PROGRAM={ninja}",
                f"-DCMAKE_TOOLCHAIN_FILE={CFG['build']['toolchain']}",
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
    built = [b for b in BINS if b["file"].exists()]
    for b in built:
        print(f"==> OK: {b['file']}（{b['file'].stat().st_size} 字节）烧录地址 {b['addr']}")
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
        str(jlink), "-device", JLINK_DEVICE, "-if", JLINK_IF,
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
    print(f"==> 连接 {JLINK_DEVICE}（{JLINK_IF} {JLINK_SPEED_KHZ}kHz）")
    rc, out, log = run_jlink(["connect", "q"], "jlink_connect.log", args.timeout)
    ok = any(s in out for s in ("Cortex-M4 processor detected", "Found SW-DP", "Connected successfully"))
    if rc != 0 or jlink_hard_fail(out) or not ok:
        print(f"==> 连接失败（日志: {log}）")
        return 1
    print("==> 连接成功：目标已响应")
    return 0


def cmd_flash(args) -> int:
    missing = [b for b in BINS if not b["file"].exists()]
    if missing:
        raise ToolError("固件不存在：" + "、".join(str(b["file"]) for b in missing) + "（先运行 build）")
    names = "，".join(f"{b['name']}->{b['addr']}" for b in BINS)
    print(f"==> 烧录 {names}")
    cmds = ["r"]
    for b in BINS:
        cmds.append(f"loadbin {b['file'].as_posix()} {b['addr']}")
        cmds.append(f"verifybin {b['file'].as_posix()} {b['addr']}")
    cmds += ["r", "g", "q"]
    rc, out, log = run_jlink(cmds, "jlink_flash.log", args.timeout)
    verified = out.count("Verify successful") >= len(BINS)
    if rc != 0 or jlink_hard_fail(out) or not verified:
        print(f"==> 烧录失败（日志: {log}）")
        return 1
    print("==> 烧录并校验成功，目标已复位运行")
    return 0


# ---------------------------------------------------------------------------
# status：读寄存器验证固件运行状态
# ---------------------------------------------------------------------------

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
    regs = [(str(r[0]), str(r[1])) for r in CFG["status"]["regs"]]
    hard_metric = str(CFG["status"].get("hard_metric", ""))
    print(f"==> 连接 {JLINK_DEVICE} 并采样寄存器（{args.samples} 次采样，间隔 {args.interval}ms）")
    script = ["connect"]
    for i in range(args.samples):
        for reg, _ in regs:
            script.append(f"mem32 {reg}, 1")
        if i < args.samples - 1:
            script.append(f"sleep {args.interval}")
    script.append("q")
    rc, out, log = run_jlink(script, "jlink_status.log", args.timeout)
    if rc != 0 or jlink_hard_fail(out):
        print(f"==> 读取失败（日志: {log}）")
        return 1
    samples = parse_mem32(out)
    ok = True
    for reg, desc in regs:
        r = reg.lower().replace("0x", "")
        seq = [v for k, v in samples if k == r]
        if len(seq) < 2:
            print(f"  {desc:<32}: 采样不完整")
            ok = False
            continue
        alive = len(set(seq)) > 1
        print(f"  {desc:<32}: {['0x%08X' % v for v in seq]} {'运行中' if alive else '无变化'}")
        # 硬指标（如 TIM6 HAL 时间基准，外设不受 CPU halt 影响）不递增判失败；
        # ODR/LED 在 J-Link halt 时任务暂停，仅作参考不判失败
        if hard_metric and hard_metric in desc and not alive:
            ok = False
    if ok:
        print("==> 固件运行正常：时间基准活跃（TIM6 递增）")
        return 0
    print("==> 固件可能未运行或已停机（详见日志）")
    return 1


# ---------------------------------------------------------------------------
# misra：cppcheck + MISRA C:2012 addon
# ---------------------------------------------------------------------------

def misra_addon_path() -> Path:
    """优先 devtool.conf 的 misra.addon（相对项目根），缺省用 skill 自带的 assets/"""
    raw = CFG.get("misra", {}).get("addon")
    if raw:
        p = Path(str(raw))
        return p if p.is_absolute() else (ROOT / p)
    return Path(__file__).resolve().parents[1] / "assets" / "cppcheck-addons" / "misra.json"


def cmd_misra(args) -> int:
    if not CFG.get("misra"):
        raise ToolError(f"{CONFIG_NAME} 缺少 misra 段（dirs/includes/defs）")
    cppcheck = find_tool("cppcheck")
    if cppcheck is None:
        # 常见 Windows 安装路径兜底
        cands = glob.glob("C:/Program Files/Cppcheck/cppcheck.exe")
        cppcheck = Path(cands[0]) if cands else None
    if cppcheck is None:
        raise ToolError("未找到 cppcheck，请安装：winget install Cppcheck.Cppcheck")

    addon = misra_addon_path()
    if not addon.exists():
        raise ToolError(f"MISRA addon 不存在: {addon}")
    # 旧版 cppcheck 无 --addon-path，且 addon json 的相对 script 按 cwd 解析：
    # 运行时生成副本，把 script 改写为绝对路径（同时保持 skill 内 json 可移植）
    addon_data = json.loads(addon.read_text(encoding="utf-8"))
    addon_data["script"] = str(addon.parent / str(addon_data["script"]))
    addon_copy = BUILD_DIR / "addons" / "misra.json"
    addon_copy.parent.mkdir(parents=True, exist_ok=True)
    addon_copy.write_text(json.dumps(addon_data, indent=2), encoding="utf-8")
    dirs = list(args.dirs) if args.dirs else CFG["misra"]["dirs"]
    includes = CFG["misra"].get("includes", [])
    defs = CFG["misra"].get("defs", [])

    print("==> MISRA C:2012 检查（cppcheck --addon=misra）")
    print(f"    范围: {' '.join(dirs)}（第三方代码豁免）")
    print(f"    规则子集: {addon}")

    proc = subprocess.run(
        [str(cppcheck), "--std=c11", f"--addon={addon_copy}"] + includes + defs +
        ["--suppress=missingIncludeSystem", "--suppress=misra-config", "--inline-suppr",
         "--enable=warning,style"] + dirs,
        timeout=args.timeout, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace",
    )
    rc, out = proc.returncode, (proc.stdout or "") + (proc.stderr or "")
    if rc == 0 and not out.strip():
        print("==> ✅ 无 MISRA 违规")
        return 0

    violations = [ln for ln in out.splitlines() if "misra-c2012-" in ln]
    if "Did not find addon" in out:
        raise ToolError("MISRA addon 未加载成功（cppcheck 无法找到 addon 脚本），检查 assets/cppcheck-addons/")
    if not violations:
        print("==> ✅ 无 MISRA 违规")
        return 0
    for v in violations:
        print(v.replace("(style)", ""))
    print(f"\n==> ❌ 存在 MISRA 违规（{len(violations)} 处）。处置方式：")
    print("    1. 修复代码")
    print("    2. 或加行内豁免注释（仅限合理场景）：// cppcheck-suppress misra-c2012-<规则号>")
    print("    3. 若认为规则不适用于本场景，在 AGENTS.md MISRA 章节补充豁免理由")
    return 1


# ---------------------------------------------------------------------------
# info
# ---------------------------------------------------------------------------

def cmd_test(args) -> int:
    """core 层 PC 单测（host gcc，无需硬件）。被测模块表来自 devtool.conf 的 tests.sources。"""
    cc = shutil.which("gcc")
    if cc is None:
        raise ToolError("未找到 host gcc（PC 单测需要）")
    t = CFG.get("tests")
    if not t:
        raise ToolError(f"{CONFIG_NAME} 缺少 tests 段")
    cflags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-g", "-O0"]
    cflags += [f"-I{d}" for d in t.get("includes", [])]
    out_dir = BUILD_DIR / "tests"
    out_dir.mkdir(parents=True, exist_ok=True)
    failed = 0

    print("==> PC 单测（core + app 层，host gcc）")
    sources = {str(k): [str(s) for s in v] for k, v in t.get("sources", {}).items()}
    for test_dir in t.get("dirs", []):
        for test_src in sorted(glob.glob(str(ROOT / test_dir / "test_*.c"))):
            name = Path(test_src).stem
            srcs = sources.get(name)
            if srcs is None:
                print(f"  ⚠️  未登记被测模块: {name}（devtool.conf tests.sources 添加）")
                continue
            files = [str(ROOT / s) if not Path(s).is_absolute() else s for s in srcs]
            exe = out_dir / f"{name}.exe"
            build_log = out_dir / f"{name}.build.log"
            proc = subprocess.run([cc] + cflags + ["-o", str(exe)] + files,
                                  capture_output=True, text=True, encoding="utf-8", errors="replace")
            if proc.returncode != 0:
                print(f"  ❌ {name} 编译失败")
                build_log.write_text(proc.stdout + proc.stderr, encoding="utf-8")
                for ln in (proc.stdout + proc.stderr).splitlines()[-10:]:
                    print(f"    {ln}")
                failed = 1
                continue
            run = subprocess.run([str(exe)], cwd=ROOT)
            if run.returncode == 0:
                print(f"  ✅ {name} 通过")
            else:
                print(f"  ❌ {name} 失败")
                failed = 1

    if failed:
        print("==> ❌ 单测未全部通过")
        return 1
    print("==> ✅ PC 单测全部通过")
    return 0


def cmd_info(args) -> int:
    print(f"项目根目录: {ROOT}")
    print(f"项目: {CFG.get('project', '')}")
    for name in ("cmake", "ninja", "arm-none-eabi-gcc", "JLink.exe", "cppcheck"):
        p = find_tool(name)
        print(f"  {name:<18}: {p if p else '未找到'}")
    for b in BINS:
        if b["file"].exists():
            print(f"固件: {b['file']}（{b['file'].stat().st_size} 字节，烧录地址 {b['addr']}）")
        else:
            print(f"固件: {b['file']}（尚未构建，先运行 build）")
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

    log_cfg = CFG.get("log", {})
    buf_size = int(log_cfg.get("ram_buf_size", 2048))
    map_path = Path(str(log_cfg.get("map", "build/app.map")))
    if not map_path.is_absolute():
        map_path = ROOT / map_path

    buf_addr = parse_map_symbol(map_path, "s_ram_buf")
    rb_addr = parse_map_symbol(map_path, "s_ram_rb")
    if buf_addr is None or rb_addr is None:
        raise ToolError("map 文件中未找到日志缓冲符号（先 build，并确认 log_enable_ram 已调用）")
    print(f"==> 读取 RAM 日志镜像 @ 0x{buf_addr:08X}（环形缓冲 {buf_size} 字节）")

    bin_path = BUILD_DIR / "log_ram.bin"
    script = ["connect", "h"]
    # rb_t 布局：buf(4) size(4) head(4) tail(4)（4 字小读取 mem32 正常）
    script.append(f"mem32 0x{rb_addr:08X}, 4")
    # 日志数据用 savebin 读取（注意：J-Link V9.64 的 count 参数按十六进制解析）
    script.append(f"savebin {bin_path.as_posix()}, 0x{buf_addr:08X}, 0x{buf_size:X}")
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
    if size > buf_size:
        print(f"==> 环形缓冲 size 异常：{size}")
        return 1
    if not bin_path.exists() or bin_path.stat().st_size < buf_size:
        print(f"==> 日志数据读取失败（{bin_path}）")
        return 1
    data = bin_path.read_bytes()[:buf_size]
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
    p = argparse.ArgumentParser(prog="devtool", description="嵌入式平台开发工具（配置驱动）")
    sub = p.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build", help="编译固件")
    b.add_argument("build_type", nargs="?", default="Release", choices=["Debug", "Release"])
    c = sub.add_parser("connect", help="测试 J-Link 连接")
    c.add_argument("--timeout", type=int, default=90)
    f = sub.add_parser("flash", help="烧录固件（devtool.conf bins）并校验")
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
    sub.add_parser("test", help="core 层 PC 单测（host gcc，无需硬件）")
    sub.add_parser("info", help="工具链与固件信息")
    m = sub.add_parser("misra", help="MISRA C:2012 检查（cppcheck addon）")
    m.add_argument("dirs", nargs="*", help="检查目录，缺省用 devtool.conf misra.dirs")
    m.add_argument("--timeout", type=int, default=600)
    args = p.parse_args()
    try:
        return {
            "build": cmd_build, "connect": cmd_connect, "flash": cmd_flash,
            "status": cmd_status, "console": cmd_console, "log": cmd_log,
            "test": cmd_test, "info": cmd_info, "misra": cmd_misra,
        }[args.cmd](args)
    except ToolError as e:
        print(f"错误: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())