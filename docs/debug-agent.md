# LLM 调试代理（gdb_agent）— 工具链总结

> 面向 LLM 全自动调试的 J-Link/GDB 调试前端：断点持久化、命中事件采集、变量轨迹、断点回归与人类监督面板。
> 代码：`.pi/skills/embeddedskills/jlink/scripts/gdb_agent.py`（面板 `debug_panel.html`）
> 入口：`dev.py debug <子命令>`（等价 `python .pi/skills/embeddedskills/jlink/scripts/gdb_agent.py <子命令>`）

## 1. 设计动机与形态

**动机**：传统 one-shot 调试（`jlink_gdb.py --batch`）每次会话断点丢失、无事件记录、状态不透明，LLM 无法"断点放那、跑一轮回来看"。本工具以**短命进程 + 状态文件持久化**实现：

```
每次命令 = 独立 gdb 会话（复用既有链路，稳）
状态     = build/debug/ 下断点表/事件时间线跨命令存续
```

**架构**：

```
                           ┌─ LLM（结构化 JSON 交互）──┐
STM32G474 ← SWD ← JLinkGDBServerCL ← gdb_agent.py ←─┤
 (board)         （每次命令起停）    （状态管理/断言）  └─ 人类（Web 面板 serve）
                                                    └─ dev.py build/flash（配合）
```

- 断点一律 **hbreak 硬件断点**（DWT，全片 4 个）——见踩坑 #1
- record 断点命中自动快照（帧/局部变量/寄存器/声明变量）写 `events.jsonl`，支持 stop-after 计数
- 回归场景隔离执行：不写事件、不更新 hits

## 2. 命令速查

| 子命令 | 作用 | 关键参数 |
|---|---|---|
| `break` | 添加/更新断点（持久化 + 连接验证） | `--spec` `--cond` `--mode record\|stop` `--watch-expr`（可多次）`--id` |
| `list` | 断点表（离线） | — |
| `delete` | 删断点 | `--id N` / `--all` |
| `run` | 重放断点并运行，收集命中事件 | `--stop-after N`（默认 1）`--timeout` `--reset`（先复位） |
| `step`/`next`/`finish` | 单步/越过/运行到返回 | `--timeout` |
| `reset` | 复位目标 | `--run`（恢复运行，默认 halt） |
| `status` | 当前 PC/帧/寄存器（halt 后快照） | — |
| `events` | 回查事件时间线 | `--tail N` |
| `trace` | 变量轨迹导出 CSV | `--expr`（逗号分隔过滤）`--output` |
| `regress` | 断点回归：场景重放 + 断言 | `--scenario`（必填）`--repeat N` `--output` |
| `serve` | Web 监督面板（静态服务 build/debug/） | `--port` `--timeout` `--no-browser` |
| `clear` | 清空断点表与事件 | — |

全程 `--json` 结构化输出；人类模式打印精简表格。

## 3. 状态文件（build/debug/）

- **breakpoints.json**：`{next_id, items:[{id, spec, cond, mode, watch_exprs, hits, created_at, last_hit_at}]}`。`hits` 跨命令累计。
- **events.jsonl**：每行一事件。`{ts(毫秒ISO), type: bp-hit|halt|step|run, bp_id, frame, locals, regs, values, source_location}`。每轮 run 的命中、停止、单步都有带完整快照的记录，可事后回查。
- **trace_<ts>.csv**：`trace` 导出，列 = ts,type,bp_id + 每个 watch 变量。
- regress 报告（`--output`）：每轮事件数/断言明细/通过率。

## 4. 典型工作流

### 4.1 交互式调试（LLM）

```bash
dev.py debug break --spec task_sysmon.c:54 --cond "g_sysmon_beat >= 3" \
    --mode record --watch-expr g_sysmon_beat --watch-expr sys_freq
dev.py debug run --reset --stop-after 2 --timeout 15
dev.py debug events --tail 5        # 命中快照：帧/局部变量/寄存器/values
dev.py debug status                  # 当前停点
dev.py debug step                    # 继续单步
```

### 4.2 回归验收（改代码后 / CI）

```bash
dev.py build && dev.py flash
dev.py debug regress --scenario tools/scenarios/sysmon-heartbeat.json --repeat 3
# 任一断言失败 → status=error；报告含每轮事件数与断言明细
```

场景文件（schema 详见 SKILL.md）声明断点集 + 运行参数 + 断言，随固件演进入库，`git diff` 即行为契约变更。

### 4.3 偶发故障取证

```bash
dev.py debug regress --scenario tools/scenarios/xxx.json --repeat 50 --output build/debug/report.json
# 命中时间/变量值的统计分布（repeat 轮次），替代人盯等
```

## 5. 踩坑记录（全部真机验证）

| # | 现象 | 根因 | 对策 |
|---|---|---|---|
| 1 | 断点命中后 HardFault（crash pc=断点地址，hfsr=FORCED，cfsr=0） | GDB 软件断点把 BKPT **写入 Flash**，STM32G4 单 bank 执行中擦写触发 RWW 冲突 | 一律 **hbreak**（DWT 硬件断点，≤4 个） |
| 2 | commands...end 块完全不生效（silent 不静默、内部命令不执行） | `-ex` 参数模式把块内容当顶层命令；`commands` 需从命令输入流读块 | 改用 `-x` 脚本文件模式（`run_gdb_script`） |
| 3 | `Bad format string, non-terminated '"'` | 脚本文件模式下 `\n` 转义被 GDB 当换行，字符串引号未终止 | printf 不带 `\n`（命令输出自带换行分隔） |
| 4 | `--reset` 后 run 立即返回，停在 Reset_Handler | `monitor reset` 的 VC_CORERESET 向量捕获 → continue 立即收 SIGTRAP | reset 后 `handle SIGTRAP nostop noprint pass`（hbreak 命中是 T05 停止包，不受影响） |
| 5 | 长时间 halt 后目标自己重启（beat 计数归零） | **IWDG 在调试暂停期间照常计数**，超时复位 | 调试避免长 halt；条件断点改用相对起点（`>=` 而非 `==`） |
| 6 | 局部变量 `<optimized out>`，条件断点求值失败 | `-Og` 优化把局部变量放寄存器 | 条件/ watch 表达式用全局变量 |
| 7 | record 停止事件去重失效（多一条 halt 事件） | silent 抑制了 `Breakpoint N,` 行，`BREAKPOINT_HIT_RE` 无匹配 → bp_id=None | 块内停止路径显式 `printf "agent-stop bp=N"` 溯源 |
| 8 | `--expr "/8wx ..."` 报 `No symbol "C"` | Git-Bash MSYS 路径转换把 `/` 开头参数改写成 `C:/Program Files/Git/...` | `restore_msys_expr()` 自动还原（jlink_gdb 与 agent 均已内置） |
| 9 | 事件 ts 同秒无法排序 | `now_iso` 秒级 | `now_iso_ms()` 毫秒时间戳 |
| 10 | 断言 `value-range` 全空采集 | 无证据 = 不通过（防假阳性） | 语义：range=全部在界，equals=任一命中，count=在 [min,max] |

## 6. 限制与路线图

- **单板独占**：一个 J-Link/SWD 一块板；多板并行需探针注册表 + 分配器（硬件多板场景再做，当前预留 `--board` 语义空间）
- **断点上限**：DWT 4 个硬件断点（超出设置失败会告警，不静默）
- **RTT/日志与断点事件时间对齐**：目前日志（RAM 镜像/串口）与事件时间线未融合，可做统一时间轴
- **面板只读**：serve 面板是监督视图，操作仍走 CLI（防误触）
- **回归场景断言类型**：当前 3 类（count/equals/range），可扩展 `none-crash`（run 超时即 fail）、`time-range`（命中时间窗）等

## 7. 相关文件

| 文件 | 说明 |
|---|---|
| `gdb_agent.py` | 本工具主体（命令分发/状态管理/断言引擎/HTTP 服务） |
| `jlink_gdb.py` / `jlink_gdb_common.py` | 底层 one-shot GDB 与解析器（复用，不修改） |
| `jlink_runtime.py` | 运行时工具（make_result/参数解析/状态读写） |
| `debug_panel.html` | 人类监督面板（读 events.jsonl/breakpoints.json 渲染） |
| `scripts/dev.py` | 入口封装（`dev.py debug` 转发） |
| `tools/scenarios/sysmon-heartbeat.json` | 回归场景示例 |
| `build/debug/` | 运行时状态目录（不入库） |