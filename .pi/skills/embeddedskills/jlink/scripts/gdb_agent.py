"""gdb_agent.py — 常驻调试状态管理器（LLM 全自动调试前端，L1 MVP）

设计：短命 gdb --batch 会话 + 状态文件持久化。
每次调用是独立会话（复用 jlink_gdb 的连接机制与 jlink_gdb_common 的 GDB 执行），
但断点表、命中事件时间线持久化在 <workspace>/build/debug/ 下，实现：
  - 断点跨命令存续（breakpoints.json）
  - record 断点命中自动快照并写事件（events.jsonl）
  - 状态可回查（list/events/status）

record 断点：命中时自动抓取帧/局部变量/寄存器快照并写入事件时间线，
  按 --stop-after 收集 N 个事件后停止（由 GDB commands 块内 if 控制）。
stop 断点：命中时 gdb 停下，快照由主命令序列（bt/info locals/info registers）抓取。
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from jlink_gdb import (  # noqa: E402
    _state_lookup,
    add_common_args,
    cleanup,
    resolve_device_params,
    start_gdbserver,
    wait_gdbserver_ready,
)
from jlink_gdb_common import (  # noqa: E402
    _parse_frames,
    _parse_registers,
    _parse_variables,
    run_gdb_commands,
)
from jlink_runtime import (  # noqa: E402
    default_config_path,
    hidden_subprocess_kwargs,
    is_missing,
    load_json_file,
    load_project_config,
    load_workspace_state,
    make_result,
    make_timing,
    normalize_path,
    now_iso,
    output_json,
    parameter_context,
    resolve_param,
    save_json_file,
    workspace_root,
)

COMMANDS = {
    "break": "添加/更新断点（持久化）",
    "list": "列出断点表（只读，不连目标）",
    "delete": "删除断点",
    "run": "重放断点并运行，收集命中事件",
    "step": "单步（step）",
    "next": "单步越过（next）",
    "finish": "运行到函数返回",
    "reset": "复位目标（SYSRESETREQ）并立即 halt",
    "status": "目标当前状态（halt 后）",
    "events": "读取事件时间线",
    "clear": "清空断点表与事件",
}

BREAKPOINTS_FILE = "breakpoints.json"
EVENTS_FILE = "events.jsonl"
LOCK_FILE = ".lock"
LOCK_RETRY_SEC = 8.0
LOCK_STALE_AGE = 60.0

EVENT_START_RE = re.compile(r"agent-event-start bp=(\d+)")
BP_SET_RE = re.compile(r"agent-bp-set id=(\d+) num=(\d+)")
AGENT_STOP_RE = re.compile(r"agent-stop bp=(\d+)")
BREAKPOINT_HIT_RE = re.compile(r"^Breakpoint (\d+),", re.MULTILINE)
FINAL_MARKER = "agent-final"

PROVIDER = "jlink-agent"


# ---------------------------------------------------------------------------
# 状态文件锁（O_EXCL 创建 + 超龄回收，跨进程互斥）
# ---------------------------------------------------------------------------


def _acquire_lock(debug_dir: Path) -> bool:
    debug_dir.mkdir(parents=True, exist_ok=True)
    lock = debug_dir / LOCK_FILE
    deadline = time.time() + LOCK_RETRY_SEC
    while True:
        try:
            fd = os.open(str(lock), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            os.write(fd, str(os.getpid()).encode("ascii"))
            os.close(fd)
            return True
        except FileExistsError:
            try:
                age = time.time() - lock.stat().st_mtime
            except OSError:
                continue
            if age > LOCK_STALE_AGE:
                try:
                    lock.unlink()
                except OSError:
                    pass
                continue
            if time.time() >= deadline:
                return False
            time.sleep(0.05)
        except OSError:
            return False


def _release_lock(debug_dir: Path) -> None:
    try:
        (debug_dir / LOCK_FILE).unlink()
    except OSError:
        pass


# ---------------------------------------------------------------------------
# 断点表读写
# ---------------------------------------------------------------------------


def _load_breakpoints(debug_dir: Path) -> tuple[list[dict[str, Any]], int]:
    data = load_json_file(debug_dir / BREAKPOINTS_FILE)
    items = data.get("items", [])
    if not isinstance(items, list):
        items = []
    next_id = data.get("next_id", 1)
    try:
        next_id = int(next_id)
    except (TypeError, ValueError):
        next_id = 1
    return [item for item in items if isinstance(item, dict)], next_id


def _save_breakpoints(debug_dir: Path, items: list[dict[str, Any]], next_id: int) -> None:
    save_json_file(debug_dir / BREAKPOINTS_FILE, {"next_id": next_id, "items": items})


# ---------------------------------------------------------------------------
# GDB 命令序列构造
# ---------------------------------------------------------------------------


def _break_cmd(item: dict[str, Any]) -> str:
    """生成断点命令。必须用硬件断点 hbreak：GDB 默认软件断点在 Flash 地址会
    把 BKPT 写进 Flash（STM32G4 单 bank RWW 冲突 → HardFault，本平台实测触发），
    hbreak 走 DWT 硬件断点（Cortex-M4 共 4 个），安全。"""
    spec = str(item.get("spec", ""))
    cond = item.get("cond")
    return f"hbreak {spec} if {cond}" if cond else f"hbreak {spec}"


def _commands_block(item: dict[str, Any], stop_after: int | None) -> str:
    """record 断点的自动快照命令块。

    整体作为一个 -ex 参数（\\n 连接）：GDB 的 commands...end 必须连续喂给，
    逐条 -ex 会被当作顶层命令执行（silent/end 会报错）。
    stop_after 为正数时用 $_agent_hits 计数控制收集数量；否则无条件继续。
    """
    lines = ["commands", "silent"]
    # 注意：脚本文件模式下 \n 转义会被 GDB 当换行（引号未终止报错），故 printf 不带转义；
    # 后续命令（bt/info）的输出自带换行，标记行可被 EVENT_START_RE 正常切分。
    lines.append(f'printf "agent-event-start bp={item["id"]}"')
    lines.append("bt")
    lines.append("info locals")
    lines.append("info registers")
    lines.append("set $_agent_hits = $_agent_hits + 1")
    if stop_after is not None and stop_after > 0:
        lines.append(f"if $_agent_hits < {stop_after}")
        lines.append("  continue")
        lines.append("else")
        # 停止路径显式打标记：silent 会抑制 "Breakpoint N," 行，无法用 BREAKPOINT_HIT_RE 溯源
        lines.append(f'  printf "agent-stop bp={item["id"]}"')
        lines.append("end")
    else:
        lines.append("continue")
    lines.append("end")
    return "\n".join(lines)


def build_replay_commands(
    items: list[dict[str, Any]],
    *,
    with_record: bool = True,
    stop_after: int | None = None,
) -> list[str]:
    """重放断点表：monitor halt → 逐条 break + 编号映射 printf → （record 时）commands 块。

    返回的命令用于脚本文件模式（run_gdb_script）；commands...end 块仅在
    脚本/交互输入流中生效，-ex 参数模式会被当作顶层命令导致块失效。
    """
    cmds = ["monitor halt"]
    for item in sorted(items, key=lambda x: int(x.get("id", 0))):
        cmds.append(_break_cmd(item))
        cmds.append(f'printf "agent-bp-set id={item["id"]} num=%d", $bpnum')
        if with_record and item.get("mode") == "record":
            cmds.append(_commands_block(item, stop_after))
    return cmds


def run_gdb_script(
    gdb_exe: str,
    elf_file: str,
    target_remote: str,
    script_lines: list[str],
    timeout: int = 30,
) -> dict:
    """以 GDB 脚本文件（-x）模式执行命令序列。

    必须用脚本/交互模式而非 -ex 参数：GDB 的 commands...end 块需要从命令
    输入流读取后续行，-ex 一次性字符串会被当成顶层命令依次执行，
    silent/end 等会报错且块不生效（实测确认）。"""
    lines = ["set pagination off", "set confirm off", "set width 0"]
    if elf_file:
        lines.append(f'file "{Path(elf_file).resolve().as_posix()}"')
    lines.append(f"target remote {target_remote}")
    lines.extend(script_lines)
    lines.append("quit")

    fd, script_path = tempfile.mkstemp(suffix=".gdb", prefix="agent_", text=True)
    try:
        with os.fdopen(fd, "w", encoding="utf-8", errors="replace") as handle:
            handle.write("\n".join(lines) + "\n")
        cmd = [gdb_exe, "--batch", "--nx", "-x", script_path]
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            encoding="utf-8",
            errors="replace",
            **hidden_subprocess_kwargs(),
        )
        combined = "\n".join(part for part in (proc.stdout, proc.stderr) if part)
        return {
            "status": "ok" if proc.returncode == 0 else "error",
            "stdout": combined,
            "stderr": proc.stderr,
            "returncode": proc.returncode,
        }
    except subprocess.TimeoutExpired as exc:
        return {
            "status": "timeout",
            "stdout": exc.stdout or "",
            "stderr": exc.stderr or "",
            "returncode": None,
        }
    finally:
        try:
            os.unlink(script_path)
        except OSError:
            pass


# ---------------------------------------------------------------------------
# 输出解析
# ---------------------------------------------------------------------------


def _split_record_events(stdout: str) -> list[tuple[int, str]]:
    """按 agent-event-start 标记切分：[(bp_id, body), ...]"""
    parts = EVENT_START_RE.split(stdout)
    out: list[tuple[int, str]] = []
    for index in range(1, len(parts), 2):
        out.append((int(parts[index]), parts[index + 1]))
    return out


def _parse_bp_map(stdout: str) -> dict[int, int]:
    """gdb 断点号 -> agent 断点 id"""
    mapping: dict[int, int] = {}
    for match in BP_SET_RE.finditer(stdout):
        mapping[int(match.group(2))] = int(match.group(1))
    return mapping


def _snapshot_from_body(body: str) -> dict[str, Any]:
    frames = _parse_frames(body)
    frame = frames[0] if frames else {}
    return {
        "frame": frame,
        "locals": _parse_variables(body),
        "regs": _parse_registers(body),
        "source_location": frame.get("location", ""),
    }


# ---------------------------------------------------------------------------
# 事件持久化
# ---------------------------------------------------------------------------


def _append_events(debug_dir: Path, events: list[dict[str, Any]]) -> int:
    path = debug_dir / EVENTS_FILE
    debug_dir.mkdir(parents=True, exist_ok=True)
    with open(path, "a", encoding="utf-8") as handle:
        for event in events:
            handle.write(json.dumps(event, ensure_ascii=False) + "\n")
    return len(events)


def _load_events(debug_dir: Path, tail: int | None = None) -> list[dict[str, Any]]:
    path = debug_dir / EVENTS_FILE
    events: list[dict[str, Any]] = []
    if path.exists():
        for line in path.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError:
                continue
    if tail is not None and tail > 0:
        events = events[-tail:]
    return events


def _update_hits(debug_dir: Path, events: list[dict[str, Any]]) -> None:
    items, next_id = _load_breakpoints(debug_dir)
    ts = now_iso()
    changed = False
    for event in events:
        bp_id = event.get("bp_id")
        if bp_id is None:
            continue
        for item in items:
            if item.get("id") == bp_id:
                item["hits"] = int(item.get("hits", 0)) + 1
                item["last_hit_at"] = ts
                changed = True
                break
    if changed:
        _save_breakpoints(debug_dir, items, next_id)


# ---------------------------------------------------------------------------
# 会话参数解析
# ---------------------------------------------------------------------------


def _resolve_session(
    args: argparse.Namespace,
    config: dict,
    state: dict,
    project_config: dict,
    workspace: Path,
    config_path: str,
    *,
    hardware: bool = False,
) -> dict[str, Any]:
    session: dict[str, Any] = {
        "parameter_sources": {},
        "hardware": hardware,
        "workspace": workspace,
        "config_path": config_path,
        "debug_dir": workspace / "build" / "debug",
    }
    if not hardware:
        # 纯文件操作（list/delete/events/clear）不依赖硬件参数
        return session

    parameter_sources: dict[str, str] = {}

    gdbserver_exe, source = resolve_param(
        "gdbserver_exe",
        args.gdbserver_exe,
        config=config,
        config_keys=["gdbserver_exe"],
        required=True,
        normalize_as_path=True,
        workspace=str(workspace),
    )
    parameter_sources["gdbserver_exe"] = source
    gdb_exe, source = resolve_param(
        "gdb_exe",
        args.gdb_exe,
        config=config,
        config_keys=["gdb_exe"],
        required=True,
        normalize_as_path=True,
        workspace=str(workspace),
    )
    parameter_sources["gdb_exe"] = source

    state_lookup = _state_lookup(state)
    dev_params = resolve_device_params(args, project_config, state_lookup)
    device = dev_params["device"]
    if is_missing(device):
        raise ValueError("缺少必要参数: device")
    interface = dev_params["interface"] or "SWD"
    speed = dev_params["speed"] or "4000"

    serial_no, source = resolve_param(
        "serial_no",
        args.serial_no,
        config=config,
        config_keys=["serial_no"],
        state_record=state_lookup,
        state_keys=["serial_no"],
    )
    parameter_sources["serial_no"] = source

    elf_file, source = resolve_param(
        "elf",
        args.elf,
        config=config,
        config_keys=["default_elf"],
        state_record=state_lookup,
        state_keys=["elf_file", "debug_file"],
        normalize_as_path=True,
        workspace=str(workspace),
    )
    parameter_sources["elf"] = source
    if is_missing(elf_file):
        elf_file = str(workspace / "build" / "bin" / "app")

    session.update(
        {
            "gdbserver_exe": gdbserver_exe,
            "gdb_exe": gdb_exe,
            "device": device,
            "interface": interface,
            "speed": speed,
            "serial_no": serial_no or "",
            "elf_file": elf_file,
            "parameter_sources": parameter_sources,
        }
    )
    return session


def _start_gdbserver(session: dict[str, Any]) -> tuple[Any, int, str]:
    gdb_proc, gdb_port = start_gdbserver(
        gdbserver_exe=session["gdbserver_exe"],
        device=session["device"],
        interface=session["interface"],
        speed=session["speed"],
        serial_no=session["serial_no"],
        gdb_port=0,
    )
    ready, server_output = wait_gdbserver_ready(gdb_proc)
    if not ready:
        cleanup([gdb_proc])
        raise ConnectionError(f"J-Link GDB Server 启动失败: {server_output or '目标无响应'}")
    return gdb_proc, gdb_port, server_output


def _session_context(session: dict[str, Any]) -> dict:
    return parameter_context(
        provider=PROVIDER,
        workspace=str(session["workspace"]),
        parameter_sources=session["parameter_sources"],
        config_path=session["config_path"],
    )


# ---------------------------------------------------------------------------
# 子命令
# ---------------------------------------------------------------------------


def cmd_break(args: argparse.Namespace, session: dict[str, Any], started_at: str, started_ts: float) -> dict:
    debug_dir = session["debug_dir"]
    if not _acquire_lock(debug_dir):
        raise OSError("状态文件被占用，请稍后重试")
    try:
        items, next_id = _load_breakpoints(debug_dir)
        spec = args.spec
        cond = args.cond or None
        mode = args.mode
        ts = now_iso()

        if args.id is not None:
            target = next((item for item in items if item.get("id") == args.id), None)
            if target is None:
                raise ValueError(f"断点 id={args.id} 不存在")
            target.update({"spec": spec, "cond": cond, "mode": mode})
            bp_id = args.id
        else:
            bp_id = next_id
            next_id = next_id + 1
            items.append(
                {
                    "id": bp_id,
                    "spec": spec,
                    "cond": cond,
                    "mode": mode,
                    "hits": 0,
                    "created_at": ts,
                    "last_hit_at": None,
                }
            )

        # 连接目标验证断点可设置（失败仅告警，断点声明式保存）
        warnings: list[str] = []
        procs: list[Any] = []
        try:
            gdb_proc, gdb_port, server_output = _start_gdbserver(session)
            procs = [gdb_proc]
            cmds = build_replay_commands(items, with_record=False) + ["info breakpoints"]
            res = run_gdb_commands(session["gdb_exe"], session["elf_file"], f"localhost:{gdb_port}", cmds, timeout=25)
            bp_map = _parse_bp_map(res.get("stdout", ""))
            failed = [item for item in items if item.get("id") not in bp_map.values()]
            for item in failed:
                warnings.append(f"断点 id={item['id']} 设置失败（spec: {item['spec']}）")
            if not bp_map:
                warnings.append("目标连接成功但未解析到任何断点（检查 spec 语法或源码行号）")
        except (ConnectionError, OSError) as exc:
            warnings.append(f"目标连接验证跳过: {exc}")
        finally:
            cleanup(procs)

        _save_breakpoints(debug_dir, items, next_id)
    finally:
        _release_lock(debug_dir)

    summary = f"断点 id={bp_id} 已保存（spec={spec}, mode={mode}）"
    if warnings:
        summary = summary + f"，警告 {len(warnings)} 条"
    return make_result(
        status="ok",
        action="break",
        summary=summary,
        details={
            "breakpoints": items,
            "next_id": next_id,
            "warnings": warnings,
            "state_file": str(debug_dir / BREAKPOINTS_FILE),
        },
        context=_session_context(session),
        metrics={"breakpoints": len(items)},
        timing=make_timing(started_at, (time.time() - started_ts) * 1000),
    )


def cmd_list(args: argparse.Namespace, session: dict[str, Any], started_at: str, started_ts: float) -> dict:
    items, next_id = _load_breakpoints(session["debug_dir"])
    return make_result(
        status="ok",
        action="list",
        summary=f"断点 {len(items)} 个",
        details={
            "breakpoints": items,
            "next_id": next_id,
            "state_file": str(session["debug_dir"] / BREAKPOINTS_FILE),
        },
        context=_session_context(session),
        metrics={"breakpoints": len(items)},
        timing=make_timing(started_at, (time.time() - started_ts) * 1000),
    )


def cmd_delete(args: argparse.Namespace, session: dict[str, Any], started_at: str, started_ts: float) -> dict:
    debug_dir = session["debug_dir"]
    if args.id is None and not args.all:
        raise ValueError("delete 必须提供 --id 或 --all")
    if not _acquire_lock(debug_dir):
        raise OSError("状态文件被占用，请稍后重试")
    try:
        items, next_id = _load_breakpoints(debug_dir)
        if args.all:
            removed = list(items)
            items = []
        else:
            target = next((item for item in items if item.get("id") == args.id), None)
            if target is None:
                raise ValueError(f"断点 id={args.id} 不存在")
            removed = [target]
            items = [item for item in items if item.get("id") != args.id]
        _save_breakpoints(debug_dir, items, next_id)
    finally:
        _release_lock(debug_dir)
    removed_ids = [item["id"] for item in removed]
    return make_result(
        status="ok",
        action="delete",
        summary=f"已删除断点 {removed_ids}",
        details={"removed": removed, "breakpoints": items},
        context=_session_context(session),
        metrics={"removed": len(removed), "breakpoints": len(items)},
        timing=make_timing(started_at, (time.time() - started_ts) * 1000),
    )


def cmd_run(args: argparse.Namespace, session: dict[str, Any], started_at: str, started_ts: float) -> dict:
    debug_dir = session["debug_dir"]
    items, _ = _load_breakpoints(debug_dir)
    if not items:
        raise ValueError("断点表为空，请先执行 break")
    elf_file = session["elf_file"]
    if not Path(elf_file).exists():
        raise ValueError(f"ELF 不存在: {elf_file}（先执行 dev.py build）")

    gdb_proc, gdb_port, server_output = _start_gdbserver(session)
    procs: list[Any] = [gdb_proc]
    try:
        cmds = build_replay_commands(items, with_record=True, stop_after=args.stop_after)
        cmds.append("set $_agent_hits = 0")
        cmds.append("continue")
        cmds.append(f'printf "{FINAL_MARKER}"')
        cmds.append("bt 1")
        cmds.append("info locals")
        cmds.append("info registers")
        res = run_gdb_script(session["gdb_exe"], elf_file, f"localhost:{gdb_port}", cmds, timeout=args.timeout)
    finally:
        cleanup(procs)

    stdout = res.get("stdout", "")
    events: list[dict[str, Any]] = []
    for bp_id, body in _split_record_events(stdout):
        event = {"ts": now_iso(), "type": "bp-hit", "bp_id": bp_id}
        event.update(_snapshot_from_body(body))
        events.append(event)

    bp_map = _parse_bp_map(stdout)
    final_part = stdout.split(FINAL_MARKER, 1)
    if len(final_part) == 2 and final_part[1].strip():
        snap = _snapshot_from_body(final_part[1])
        # 停止原因溯源：record 块内停止路径会打 agent-stop bp=N；
        # stop 断点命中则依赖 "Breakpoint N," 行（非 silent，正常打印）
        stop_match = AGENT_STOP_RE.search(final_part[0])
        hit = BREAKPOINT_HIT_RE.search(stdout)
        if stop_match:
            bp_id = int(stop_match.group(1))
        elif hit:
            bp_id = bp_map.get(int(hit.group(1)))
        else:
            bp_id = None
        # 去重：与最后一条 record 事件同断点同位置时跳过（stop_after 满足后主序列重复抓帧）
        duplicate = False
        if events and bp_id is not None:
            last = events[-1]
            duplicate = (
                last.get("type") == "bp-hit"
                and last.get("bp_id") == bp_id
                and last.get("frame", {}).get("location") == snap.get("source_location")
            )
        if not duplicate:
            event = {
                "ts": now_iso(),
                "type": "bp-hit" if bp_id is not None else "halt",
                "bp_id": bp_id,
            }
            event.update(snap)
            events.append(event)

    if not _acquire_lock(debug_dir):
        raise OSError("状态文件被占用，请稍后重试")
    try:
        _append_events(debug_dir, events)
        _update_hits(debug_dir, events)
    finally:
        _release_lock(debug_dir)

    timed_out = res.get("status") == "timeout"
    failed = [item for item in items if item.get("id") not in bp_map.values()]
    warnings = [f"断点 id={item['id']} 重放失败（spec: {item['spec']}）" for item in failed]
    status = "ok" if res.get("status") in ("ok", "timeout") else "error"
    summary = f"run 完成，捕获事件 {len(events)} 个" + ("（超时）" if timed_out else "")
    return make_result(
        status=status,
        action="run",
        summary=summary,
        details={
            "events": events,
            "timed_out": timed_out,
            "warnings": warnings,
            "gdb_port": gdb_port,
            "server_output": server_output,
            "returncode": res.get("returncode", 0),
        },
        context=_session_context(session),
        metrics={"events": len(events), "breakpoints": len(items)},
        next_actions=["事件已写入 events.jsonl，可用 events --tail N 回查"],
        timing=make_timing(started_at, (time.time() - started_ts) * 1000),
    )


def cmd_step(args: argparse.Namespace, session: dict[str, Any], started_at: str, started_ts: float) -> dict:
    action = args.command
    debug_dir = session["debug_dir"]
    items, _ = _load_breakpoints(debug_dir)
    elf_file = session["elf_file"]
    if not Path(elf_file).exists():
        raise ValueError(f"ELF 不存在: {elf_file}（先执行 dev.py build）")

    gdb_proc, gdb_port, server_output = _start_gdbserver(session)
    procs: list[Any] = [gdb_proc]
    try:
        # 单步场景断点全部按 stop 处理（不挂 commands 块，命中即停，行为可预期）
        cmds = build_replay_commands(items, with_record=False)
        cmds.extend([action, "bt 1", "info locals", "info registers"])
        res = run_gdb_script(session["gdb_exe"], elf_file, f"localhost:{gdb_port}", cmds, timeout=args.timeout)
    finally:
        cleanup(procs)

    snap = _snapshot_from_body(res.get("stdout", ""))
    event = {"ts": now_iso(), "type": "step", "bp_id": None}
    event.update(snap)
    if not _acquire_lock(debug_dir):
        raise OSError("状态文件被占用，请稍后重试")
    try:
        _append_events(debug_dir, [event])
    finally:
        _release_lock(debug_dir)

    timed_out = res.get("status") == "timeout"
    status = "ok" if res.get("status") in ("ok", "timeout") else "error"
    return make_result(
        status=status,
        action=action,
        summary=f"{action} 完成" + ("（超时）" if timed_out else ""),
        details={"events": [event], "gdb_port": gdb_port, "server_output": server_output},
        context=_session_context(session),
        metrics={"events": 1},
        timing=make_timing(started_at, (time.time() - started_ts) * 1000),
    )


def cmd_reset(args: argparse.Namespace, session: dict[str, Any], started_at: str, started_ts: float) -> dict:
    elf_file = session["elf_file"]
    if not Path(elf_file).exists():
        raise ValueError(f"ELF 不存在: {elf_file}（先执行 dev.py build）")

    gdb_proc, gdb_port, server_output = _start_gdbserver(session)
    procs: list[Any] = [gdb_proc]
    try:
        cmds = ["monitor reset"]
        if args.run:
            cmds.append("continue")
        else:
            cmds.append("bt 1")
        res = run_gdb_commands(session["gdb_exe"], elf_file, f"localhost:{gdb_port}", cmds, timeout=25)
    finally:
        cleanup(procs)

    body = res.get("stdout", "")
    frames = _parse_frames(body)
    frame = frames[0] if frames else {}
    running = args.run
    return make_result(
        status="ok" if res.get("status") == "ok" else "error",
        action="reset",
        summary=f"目标已复位" + ("并恢复运行" if running else "，当前 halt"),
        details={
            "frame": frame,
            "source_location": frame.get("location", ""),
            "running": running,
            "gdb_port": gdb_port,
            "server_output": server_output,
        },
        context=_session_context(session),
        timing=make_timing(started_at, (time.time() - started_ts) * 1000),
    )


def cmd_status(args: argparse.Namespace, session: dict[str, Any], started_at: str, started_ts: float) -> dict:
    elf_file = session["elf_file"]
    if not Path(elf_file).exists():
        raise ValueError(f"ELF 不存在: {elf_file}（先执行 dev.py build）")

    gdb_proc, gdb_port, server_output = _start_gdbserver(session)
    procs: list[Any] = [gdb_proc]
    try:
        cmds = ["monitor halt", "bt 1", "info registers", "info locals"]
        res = run_gdb_commands(session["gdb_exe"], elf_file, f"localhost:{gdb_port}", cmds, timeout=25)
    finally:
        cleanup(procs)

    body = res.get("stdout", "")
    frames = _parse_frames(body)
    frame = frames[0] if frames else {}
    items, _ = _load_breakpoints(session["debug_dir"])
    total_hits = sum(int(item.get("hits", 0)) for item in items)
    return make_result(
        status="ok" if res.get("status") == "ok" else "error",
        action="status",
        summary=f"PC={frame.get('address', '?')} {frame.get('function', '?')}",
        details={
            "frame": frame,
            "registers": _parse_registers(body),
            "locals": _parse_variables(body),
            "source_location": frame.get("location", ""),
            "breakpoints": {"total": len(items), "total_hits": total_hits},
            "gdb_port": gdb_port,
            "server_output": server_output,
        },
        context=_session_context(session),
        metrics={"breakpoints": len(items), "total_hits": total_hits},
        timing=make_timing(started_at, (time.time() - started_ts) * 1000),
    )


def cmd_events(args: argparse.Namespace, session: dict[str, Any], started_at: str, started_ts: float) -> dict:
    events = _load_events(session["debug_dir"], args.tail)
    return make_result(
        status="ok",
        action="events",
        summary=f"事件 {len(events)} 条",
        details={
            "events": events,
            "event_file": str(session["debug_dir"] / EVENTS_FILE),
        },
        context=_session_context(session),
        metrics={"events": len(events)},
        timing=make_timing(started_at, (time.time() - started_ts) * 1000),
    )


def cmd_clear(args: argparse.Namespace, session: dict[str, Any], started_at: str, started_ts: float) -> dict:
    debug_dir = session["debug_dir"]
    if not _acquire_lock(debug_dir):
        raise OSError("状态文件被占用，请稍后重试")
    try:
        _save_breakpoints(debug_dir, [], 1)
        events_path = debug_dir / EVENTS_FILE
        if events_path.exists():
            events_path.unlink()
    finally:
        _release_lock(debug_dir)
    return make_result(
        status="ok",
        action="clear",
        summary="已清空断点表与事件时间线",
        details={"state_dir": str(debug_dir)},
        context=_session_context(session),
        timing=make_timing(started_at, (time.time() - started_ts) * 1000),
    )


# ---------------------------------------------------------------------------
# 命令行
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="J-Link 常驻调试状态管理器（LLM 全自动调试前端）")
    sub = parser.add_subparsers(dest="command")
    for name, help_text in COMMANDS.items():
        sub_parser = sub.add_parser(name, help=help_text)
        add_common_args(sub_parser)
        if name == "break":
            sub_parser.add_argument("--spec", required=True, help="断点位置：file:line / function / *addr")
            sub_parser.add_argument("--cond", default=None, help="条件表达式，如 g_sysmon_beat == 3")
            sub_parser.add_argument("--mode", choices=("record", "stop"), default="record", help="命中行为：record=自动快照+继续；stop=停下")
            sub_parser.add_argument("--id", type=int, default=None, help="更新指定 id 的断点（缺省则新增）")
        elif name == "delete":
            sub_parser.add_argument("--id", type=int, default=None, help="删除指定 id")
            sub_parser.add_argument("--all", action="store_true", help="删除全部")
        elif name == "run":
            sub_parser.add_argument("--timeout", type=int, default=30, help="运行超时秒数（默认 30）")
            sub_parser.add_argument("--stop-after", type=int, default=1, help="收集 N 个命中事件后停止（0=不限制）")
        elif name in ("step", "next", "finish"):
            sub_parser.add_argument("--timeout", type=int, default=30, help="执行超时秒数（默认 30）")
        elif name == "reset":
            sub_parser.add_argument("--run", action="store_true", help="复位后立即恢复运行（默认复位后 halt）")
        elif name == "events":
            sub_parser.add_argument("--tail", type=int, default=None, help="只显示尾部 N 条（默认全部）")
    return parser


def _print_breakpoints(items: list[dict[str, Any]]) -> None:
    if not items:
        print("（断点表为空）")
        return
    print("id  mode    hits  spec")
    for item in sorted(items, key=lambda x: int(x.get("id", 0))):
        cond = f" if {item.get('cond')}" if item.get("cond") else ""
        print(f"{item['id']:<3} {item.get('mode', ''):<7} {item.get('hits', 0):<5} {item['spec']}{cond}")


def _print_events(events: list[dict[str, Any]]) -> None:
    if not events:
        print("（无事件）")
        return
    for event in events:
        frame = event.get("frame") or {}
        bp = f" bp={event.get('bp_id')}" if event.get("bp_id") is not None else ""
        print(
            f"[{event.get('ts', '')}] {event.get('type', '')}{bp} "
            f"{frame.get('function', '??')} @ {frame.get('location', '?')}"
        )


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        sys.exit(1)

    started_at = now_iso()
    started_ts = time.time()
    workspace = workspace_root(args.workspace)
    config_path = normalize_path(args.config or str(default_config_path(__file__)))
    config = load_json_file(config_path)
    state = load_workspace_state(str(workspace))
    project_config = load_project_config(str(workspace))

    try:
        hardware = args.command in {"break", "run", "step", "next", "finish", "status", "reset"}
        session = _resolve_session(
            args,
            config,
            state,
            project_config,
            workspace,
            config_path,
            hardware=hardware,
        )
        if args.command == "break":
            result = cmd_break(args, session, started_at, started_ts)
        elif args.command == "list":
            result = cmd_list(args, session, started_at, started_ts)
        elif args.command == "delete":
            result = cmd_delete(args, session, started_at, started_ts)
        elif args.command == "run":
            result = cmd_run(args, session, started_at, started_ts)
        elif args.command in ("step", "next", "finish"):
            result = cmd_step(args, session, started_at, started_ts)
        elif args.command == "status":
            result = cmd_status(args, session, started_at, started_ts)
        elif args.command == "reset":
            result = cmd_reset(args, session, started_at, started_ts)
        elif args.command == "events":
            result = cmd_events(args, session, started_at, started_ts)
        elif args.command == "clear":
            result = cmd_clear(args, session, started_at, started_ts)
        else:
            raise ValueError(f"未知子命令: {args.command}")
    except (ValueError, ConnectionError, OSError) as exc:
        result = make_result(
            status="error",
            action=args.command,
            summary=str(exc),
            details={},
            context=parameter_context(provider=PROVIDER, workspace=str(workspace), config_path=config_path),
            error={"code": "agent_error", "message": str(exc)},
            timing=make_timing(started_at, (time.time() - started_ts) * 1000),
        )

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        pass

    if args.as_json:
        output_json(result)
    elif result["status"] == "ok":
        print(f"[agent-{args.command}] {result['summary']}")
        details = result.get("details", {})
        if args.command in ("list", "break"):
            _print_breakpoints(details.get("breakpoints", []))
            for warning in details.get("warnings", []):
                print(f"  ⚠ {warning}")
        elif args.command in ("run", "step", "next", "finish"):
            _print_events(details.get("events", []))
        elif args.command == "events":
            _print_events(details.get("events", []))
        elif args.command == "status":
            frame = details.get("frame", {})
            print(f"  PC: {frame.get('address', '?')}  {frame.get('function', '?')} @ {details.get('source_location', '?')}")
            regs = details.get("registers", {})
            if regs:
                shown = list(regs.items())[:12]
                print("  registers: " + ", ".join(f"{k}={v}" for k, v in shown))
    else:
        print(f"[agent-{args.command}] 失败 — {result['error']['message']}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
