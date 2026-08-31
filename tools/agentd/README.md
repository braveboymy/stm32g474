# agentd — agent 状态指示灯系统（Go 产品版）

单一原生二进制，无任何运行时依赖。把 pi / Claude Code / Codex 的运行状态通过
STM32 双 LED 指示：**运行中 / 等待用户 / 完成 / 失败 / 断链**。

```
pi 扩展（~/.pi/agent/extensions/status-bridge.ts，随 pi 自动加载）
Claude Code hooks（~/.claude/settings.json）         ──┐
Codex hooks（~/.codex/hooks.json）                   ──┴─► agentd forward（毫秒级）
                                                              │ UDP 127.0.0.1:47831
                                                              ▼
                                              agentd serve（常驻桥 = "驱动本体"）
                                                              │ 串口协议（行协议，\n 结尾）
                                                              ▼
                                              STM32G474 task_status → 双 LED
```

## 构建（Windows）

```bat
:: 前置：Go 装在 D:\Tools\go（脚本会临时加入 PATH，不污染系统环境）
tools\agentd\build.bat
```

跨平台：`GOOS=linux/darwin GOARCH=arm64 go build` 已验证通过
（串口库 go.bug.st/serial 纯 Go，无 CGO）。

## 安装 / 卸载（产品级体验，单文件自安装）

```bat
agentd.exe install            :: ① 复制自身到 %LOCALAPPDATA%\agentd\ ② 写 pi 扩展
                              :: ③ 合并 CC/Codex hooks（幂等、保留用户配置）
                              :: ④ 注册登录自启（schtasks onlogon）
agentd.exe install --dry-run  :: 演练，不落盘
agentd.exe uninstall          :: 清理 hooks + 卸载服务
```

- hooks 调用**自身绝对路径**，升级只需替换二进制，配置永不失效
- Linux 注册 systemd 用户服务，macOS 注册 LaunchAgent（同源码，构建标签分流）
- Windows 无控制台窗口（`-ldflags "-s -w"` 加 `-H windowsgui` 可再压 ~0.5MB 并隐藏窗口，
  当前为调试友好保留了控制台）

## 命令

| 命令 | 说明 |
|---|---|
| `agentd serve [--device COMx] [--dry-run]` | 常驻桥；无板调试用 `--dry-run` + `echo STATUS? \| agentd serve --dry-run` |
| `agentd forward <agent> <event>` | 适配器（hook 调用），桥未启动时静默 |
| `agentd install / uninstall` | 自安装/反安装（见上） |
| `agentd version` | 打印版本 |

## 串口协议（与 MCU 固件 app/tasks/task_status.c 兼容，**勿改格式**）

| 方向 | 行 | 含义 |
|---|---|---|
| PC→MCU | `pi,RUN` / `cc,WAIT` / `cx,FAIL` | 状态变化（STATE=IDLE/RUN/WAIT/DONE/FAIL） |
| PC→MCU | `HBT` | 心跳每 2s；MCU 6s 无下行判定断链 |
| PC→MCU | `SNAP,END` | 快照结束（应答 STATUS? 后） |
| MCU→PC | `STATUS?` | 复位同步查询 → 全量快照 + SNAP,END |

事件→状态映射集中在 `events.go` 的 `eventMap`（agent 升级只改这里）：
pi `agent_start`→RUN / `ui_prompt_start`→WAIT / `ui_prompt_end`→RUN / `agent_settled`→DONE
cc·cx `session_start`·`user_prompt_submit`→RUN / `stop`→WAIT / `stop_failure`→FAIL / `session_end`→IDLE

## 目录

| 文件 | 职责 |
|---|---|
| `main.go` | 子命令分发、常量 |
| `serve.go` | 桥：UDP + 串口（写队列单写者）+ 心跳 + STATUS? 应答 |
| `forward.go` | 薄适配器 |
| `events.go` | 事件→状态映射 |
| `install.go` | 配置合并（JSON UseNumber 保留用户字面量）+ pi 扩展模板 |
| `service_windows.go` / `service_unix.go` | schtasks / systemd / launchd |
| `detect_windows.go` | PowerShell 按 STLink/STMicro 名探测 COM 口 |

最初曾实现一版 Python 原型，已删除；如需要对照历史可在 git 历史中查看。