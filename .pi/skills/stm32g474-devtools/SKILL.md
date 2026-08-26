---
name: stm32g474-devtools
description: >-
  STM32G474 平台开发闭环：编译固件、J-Link 烧录（bootloader+app）、仿真验证
  （寄存器级状态检查、CPU 寄存器读取）、日志获取（RAM 镜像/串口）、一键全流程
  verify。当用户要求编译/构建固件、烧录/下载程序、连接仿真调试、验证固件运行状态、
  获取板卡日志、排查启动失败/卡死，或提及 devtool/dev.py/J-Link 时使用。
---

# STM32G474 开发闭环

本 skill 提供本平台从代码到硬件验证的完整闭环：编译 → 烧录 → 仿真验证 → 日志获取。
所有操作均有脚本支撑（`scripts/dev.py`），可直接执行，也支持多步组合（`verify`）。

## 前置条件

- 工具链：arm-none-eabi-gcc、cmake、ninja（`tools/devtool.py` 自动探测，无串口场景也可用）
- J-Link 软件（SEGGER 安装目录：`C:/Program Files/SEGGER/JLink_*` 或 `(x86)/SEGGER/JLink_*`，devtool.py 自动探测取版本最大者；本机已装 JLink_V688c），目标 STM32G474RE，SWD 4MHz
- 若为克隆 J-Link：用环境变量 `JLINK_VERSION` 锁定版本（如 `JLINK_VERSION=688`），兼容版场景见 references/troubleshooting.md「Out of sync」
- Python：项目根执行过 `uv sync`（`console` 子命令需 pyserial，其余零依赖）

## 标准流程（build → flash → verify）

脚本入口：`scripts/dev.py`（内部复用项目 `tools/devtool.py`，保持单一实现）

```bash
# 1. 编译（Debug: -Og，Release: -O2）
python .pi/skills/stm32g474-devtools/scripts/dev.py build [Debug|Release]

# 2. 烧录：bootloader.bin -> 0x08000000，app.bin -> 0x08008000（自动校验）
python .pi/skills/stm32g474-devtools/scripts/dev.py flash

# 3. 一键验证：flash 后 3 秒，读寄存器确认时间基准/LED 活跃，再拉取 RAM 日志
python .pi/skills/stm32g474-devtools/scripts/dev.py verify
```

等价于分步执行 `dev.py build` → `dev.py flash` → `dev.py status` → `dev.py log`。

## 命令参考

| 命令 | 用途 | 典型输出 |
|---|---|---|
| `dev.py build [Debug\|Release]` | 编译固件 | `OK: build/bin/app.bin（N 字节）` |
| `dev.py flash` | J-Link 烧录 boot+app 并校验 | `烧录并校验成功，目标已复位运行` |
| `dev.py connect` | J-Link 连接测试 | `连接成功：目标已响应` |
| `dev.py status` | 采样 TIM6 计数 / GPIOA ODR 验证运行 | `TIM6 CNT: 运行中` |
| `dev.py log [--tail N]` | J-Link 读 RAM 日志镜像（无串口可用） | `[00035000] I/mon(mon): heap free=47400` |
| `dev.py console [--list]` | 串口日志（需 ST-LINK VCP） | 实时日志流，Ctrl+C 退出 |
| `dev.py regs` | halt 后读 CPU 寄存器 + addr2line 定位 PC | `PC = 0800BA5C` + 对应函数 |
| `dev.py debug` | LLM 调试代理（gdb_agent）：持久断点 + 命中事件采集 | 见下方「LLM 调试代理」节 |
| `dev.py verify` | 一键闭环（build→flash→status→log） | 全流程输出 |

## LLM 调试代理（dev.py debug）

断点/事件持久化在 `build/debug/`，每次命令独立会话但状态跨命令存续，适合 LLM 全自动调试：

```bash
dev.py debug break --spec "task_sysmon.c:54" --cond "g_sysmon_beat == 3" --mode record \
    --watch-expr g_sysmon_beat --watch-expr sys_freq
#     record=命中自动快照+继续（写入 events.jsonl）；stop=命中即停
#     --watch-expr 可多次：命中时对变量逐条 p 求值，写入事件 values（供 trace 导出）
dev.py debug run --stop-after 2 --timeout 30 [--reset]   # 重放断点，收集 N 个事件后停止
#     --reset：运行前先复位目标（BSS 清零，条件断点从确定性起点开始）
dev.py debug step|next|finish                  # 单步/越过/运行到返回
dev.py debug reset [--run]                     # 复位目标（默认 halt；--run 恢复运行）
dev.py debug status                            # 目标当前 PC/帧/寄存器
dev.py debug list|delete --id N|clear          # 断点管理
dev.py debug events --tail 10                  # 回查事件时间线（每行含 frame/locals/regs/values 快照）
dev.py debug trace [--expr g_sysmon_beat]      # 变量轨迹导出 CSV（来自事件 values，可过滤列）
dev.py debug regress --scenario xxx.json --repeat 3   # 断点回归（隔离执行，不污染断点/事件）
dev.py debug serve [--port 8765]               # Web 监督面板（静态页实时渲染断点/事件）
```

### 回归场景文件（JSON，示例：tools/scenarios/sysmon-heartbeat.json）

```json
{
  "name": "sysmon-heartbeat",                  // 必填
  "setup": {"reset": true},                    // 可选：每轮 run 前复位目标
  "breakpoints": [                             // 必填（≥1）；字段同 break 子命令
    {"spec": "task_sysmon.c:54", "cond": "g_sysmon_beat >= 3",
     "mode": "record", "watch_exprs": ["g_sysmon_beat", "sys_freq"]}
  ],
  "run": {"stop_after": 2, "timeout": 20},    // 可选
  "asserts": [                                 // 可选；判据见下
    {"type": "event-count", "bp_id": 1, "min": 2, "max": 2},
    {"type": "value-equals", "expr": "sys_freq", "value": "160000000"},
    {"type": "value-range", "expr": "g_sysmon_beat", "gte": 0, "lte": 10}
  ]
}
```

断言语义：event-count=某断点本轮事件数在 [min,max]；value-equals=任一事件 values 命中该值；
value-range=全部采集值在 [gte,lte]（无采集判不过）。任何断言失败 → regress 返回 status=error，
可用 --output 落盘报告 JSON。场景目录建议 tools/scenarios/（随固件演进入库）。

注意事项：
- 断点一律硬件断点（hbreak/DWT，共 4 个）：Flash 软件断点会触发 STM32G4 单 bank RWW 冲突 HardFault
- 调试 halt 时 IWDG 照常计数：长时间暂停可能被复位，复位后断点条件可能不再命中
- 条件表达式优先用全局变量（局部变量在 -Og 下可能 optimized out）

## 日志获取（两种通道）

1. **RAM 镜像（推荐，无串口依赖）**：`dev.py log`
   - 固件侧：`log_enable_ram()`（core/log/log.c）将日志同时写入 2KB 环形缓冲
   - 工具侧：J-Link `savebin` 读缓冲 + 解析 head/tail 重组
2. **串口**：`dev.py console`（板载 ST-LINK VCP 未接时不可用，自动提示）

## 卡死/启动失败定位流程

1. `dev.py regs` → 看 PC 落在哪个函数（addr2line 自动给出）
2. 若 PC 在 `vAssertCalled`/`assert_failed`：读取 UART TX 缓冲找断言消息
   （`dev.py status` 前的 log 输出中已有线索；或按 references/troubleshooting.md 查症状表）
3. 若 PC 正常但 TIM6/ODR 无变化：按 troubleshooting.md 检查启动链路

## 参考

- [排错手册：启动链路与常见症状](references/troubleshooting.md)（本轮实测踩坑记录）
- 分区约定：`docs/flash-partition.md`（boot 32KB / app 448KB @0x08008000 / 参数 32KB）
- 底层工具：`tools/devtool.py`（uv 管理）、`tools/fetch_third_party.sh`（依赖锁定）
