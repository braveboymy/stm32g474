# 项目会话总结（第 1 期）

> **本文档用途**：记录本项目的对话历史、关键决策、已解决问题与后续计划。
> 下次继续对话前先读本文档，可快速恢复上下文。最后更新：2026-08-01。

## 1. 项目定位与已确认决策

**STM32G474 通用嵌入式平台（业务方向：电机控制/FOC）**

| 决策点 | 结论 | 状态 |
|---|---|---|
| 内核 | FreeRTOS V11.1.0（业务任务走 RTOS；控制环等硬实时逻辑放中断裸跑） | ✅ 已落地 |
| 业务方向 | 电机控制（信号链能力：ADC 同步采样/PWM/CORDIC/FMAC 优先建设） | ✅ 方向已定，M5 实施 |
| 开发板 | NUCLEO-G474RE（PA5=LED，USART2 调试串口，HSE 24MHz） | ✅ |
| 调试器 | J-Link SWD 4MHz（软件 V9.64，安装于 `C:\Program Files\SEGGER\JLink_V964\`） | ✅ |
| OTA | 后置（M6），但 Flash 分区第一天定死 | ✅ |
| 工具链 | ARM GCC 14.2（winget）、CMake、Ninja、uv（Python）、cppcheck 2.21、J-Link V9.64 | ✅ 全部本机安装 |
| 静态规范 | MISRA C:2012 强制（项目代码），pre-commit 自动检查 | ✅ |

**Flash 分区**（docs/flash-partition.md）：boot 0x08000000(32KB) / app 0x08008000(448KB) / 参数 0x08078000(32KB)。

## 2. 架构与目录

```
app/（业务任务：task_led、task_sysmon）
core/（平台核心，硬件无关）：osal/（OS 抽象）log/（分级日志+RAM 镜像）util/（环形缓冲）kernel/（FreeRTOS 钩子）
bsp/（板级）：board/clock/led/uart/msp/hal_timebase/retarget + startup/system/linker
bootloader/（极简引导，0x08000000）
config/（FreeRTOSConfig.h / HAL conf / platform.h）
tools/（devtool.py / check_misra.sh / install_hooks.sh / githooks/ / cppcheck-addons/）
.p .pi/skills/stm32g474-devtools/（项目级 skill，scripts/dev.py）
third_party/（FreeRTOS-Kernel V11.1.0 + STM32CubeG4 v1.6.3，git 忽略，fetch 脚本拉取）
```

依赖方向：`app → core → bsp → hal/freertos`（禁止反向）；core/ 不依赖 bsp/HAL。

## 3. 关键节点与已解决问题（排坑记录）

### 3.1 启动链路 6 大坑（均已修复并有文档支撑）

| # | 问题现象 | 根因 | 修复 |
|---|---|---|---|
| 1 | 烧录后程序不跑（TIM6=0、ODR 垃圾值） | 0x08000000 空 Flash，复位后从空区取向量表 → lockup | 写极简 bootloader（校验向量表→跳转 app） |
| 2 | boot HardFault（UNDEFINSTR@0x08000368） | 链接脚本缺 `.init/.fini` 输出段，`--gc-sections` 裁掉 crtn.o 结尾 → `_init` 残缺 fall-through | 链接脚本 `.init/.fini` 加 `KEEP()` |
| 3 | FreeRTOS 断言 port.c:343 | v11 移除宏替换机制，向量表必须直连 `vPortSVCHandler`/`xPortPendSVHandler`/`xPortSysTickHandler`；且该检查在 GNU 下因 Thumb 位必然误报 | startup 向量表改直连 + `configCHECK_HANDLER_INSTALLATION 0` |
| 4 | TIM6 不跑（HAL 默认 SysTick 版被链接） | 覆盖 HAL weak 符号的文件（hal_timebase/msp/retarget）放静态库，链接顺序导致 weak 实现胜出 | 三个文件编入 app 可执行文件（注释已说明，勿移回库） |
| 5 | HAL assert hal_cortex.c:191（NVIC 优先级） | HAL_InitTick 覆盖实现漏了 `uwTickPrio = TickPriority` 同步，HAL_RCC_ClockConfig 用初始值 16 触发断言 | hal_timebase.c 补同步（注释已说明） |
| 6 | 链接 undefined（HAL_UARTEx_*/HAL_TIMEx_*） | HAL 主文件无条件引用 ex 拆分模块的回调 | 补编 hal_uart_ex.c/hal_tim_ex.c/hal_dma.c |

**红线（AGENTS.md 第 5 章，改动前必读）**：v11 向量表直连、.init/.fini KEEP、weak 覆盖文件留在 app、uwTickPrio 同步。

### 3.2 J-Link 工具注意事项（实测）

- **count 参数按十六进制解析**（V9.64）：`mem32 addr,128` 实际读 0x128 字；需要精确字节数用 `0xNNN` 形式
- **大内存读取用 `savebin`** 落盘再解析（mem32 大读取 count 语义不可靠）
- git-bash/mintty 下直接跑 JLink.exe 输出不可见（全缓冲+挂起），用 Python subprocess 或 cmd 重定向；`-Log` 辅助诊断
- 首次连接可能触发仿真器固件更新（最长 2 分钟）
- 断点调试（run-to）前先复位，否则目标已卡死时断点不命中

### 3.3 开发工具链

**统一入口 `tools/devtool.py`**（uv 管理，零依赖 + pyserial）：

```
devtool.py build [Debug|Release]   # 编译（工具链自动探测）
devtool.py flash                   # 烧录 boot+app（J-Link，双校验）
devtool.py status                  # TIM6/ODR 采样验证运行（TIM6 为硬指标）
devtool.py log [--tail N]          # J-Link 读 RAM 日志镜像（无串口可用）
devtool.py console [--list]        # 串口日志（pyserial）
devtool.py regs                    # CPU 寄存器 + addr2line 定位（dev.py 提供）
devtool.py verify                  # 一键闭环 build→flash→status→log（dev.py 提供）
```

项目级 skill：`.pi/skills/stm32g474-devtools/`（SKILL.md + scripts/dev.py + references/troubleshooting.md）。
排错手册 `references/troubleshooting.md` 是排坑记录精华，卡死/启动失败先查它。

### 3.4 MISRA C:2012（AGENTS.md 第 4 章）

- 工具：`tools/check_misra.sh`（cppcheck 2.21 + misra addon，addon 组件锁定在 `tools/cppcheck-addons/`）
- 范围：core/bsp/app/bootloader；第三方（third_party/、bsp/system、bsp/startup）豁免
- **项目级豁免 56 条**（misra.json + AGENTS.md 4.1 表，每条有理由，如 21.6 stdio 仅限 snprintf、22.1 FreeRTOS heap、17.7 返回值惯例）
- 豁免外违规必须清零：修复 → 行内豁免（带原因）→ 补充豁免表
- **已清零基线**：log.c（10.4/12.1/13.3/14.4/18.4）、rb.c（10.4）
- 高频规避点：整数字面量 `U` 后缀、自增不嵌表达式、`!= NULL` 显式比较、`&buf[n]` 替代指针算术、混合比较加括号

### 3.5 pre-commit 钩子

- `tools/githooks/pre-commit`：提交前自动跑 `dev.py build`（增量）+ `check_misra.sh`，失败阻止提交
- 安装：`bash tools/install_hooks.sh`（git core.hooksPath，随仓库分发）
- 跳过：`SKIP_CHECKS=1 git commit` 或 `--no-verify`（事后必须补跑）
- 实测：违规提交被拦截 ✅，正常提交通过 ✅
- **曾修复 check_misra.sh 假阳性 bug**（`grep -av "use --rule-texts"` 误滤违规行 → 改为匹配 `misra-c2012-` 标签）

## 4. 当前状态（M1 板卡实测通过）

`dev.py verify` 全流程验证输出：

```
[00000000] I/sys(boot): boot start
[00000000] I/mon(mon): stm32g474-platform v0.1.0
[00000000] I/mon(mon): SYSCLK=170000000 HCLK=170000000 PCLK1=170000000 PCLK2=170000000
[00000000] I/led(led): task started
[00035000] I/mon(mon): heap free=47400     ← 每 5s，堆稳定无泄漏
```

- ✅ 170MHz 时钟、FreeRTOS 调度、HAL 时间基准（TIM6）、LED 任务、UART、日志系统
- ✅ MISRA 0 违规、提交钩子生效
- ⏳ 未做：串口实测（板载 ST-LINK VCP 未接，无 COM 口；RAM 日志已覆盖验证）
- 已提交 commits：M1 骨架 → M1 实测修复 → skill 化 → AGENTS.md+MISRA → pre-commit

## 5. 后续计划

### 5.1 里程碑路线（docs/milestones.md）

| 阶段 | 内容 | 状态 |
|---|---|---|
| M1 | 平台最小系统（骨架/RTOS/日志/烧录验证） | ✅ 完成 |
| **M2** | 健壮性：HardFault 现场保存/崩溃报告、看门狗、故障管理框架（core/fault） | ⬜ 建议下一项 |
| M3 | 参数存储（Flash 模拟 EEPROM）、CLI | ⬜ |
| M4 | 通信：通用帧协议 + UART/CAN + USB-CDC | ⬜ |
| **M5** | 信号链（电机方向重点）：ADC+HRTIM 同步采样、CORDIC/FMAC、PID、PWM 框架 | ⬜ 业务核心 |
| M6 | OTA：Bootloader 升级 + 回滚 | ⬜ |
| M7 | PC 端单测（core/ 层）+ CI | ⬜ |

### 5.2 下一会话可选起点

1. **M2 健壮性**（建议）：HardFault handler 现场保存（栈帧/寄存器/CFSR 转储到 RAM 固定区）+ 复位后启动上报 + core/fault 故障管理框架（登记/锁定/恢复策略）——现有排错手册的 CFSR/栈帧知识可直接落地为代码
2. **M5 信号链**：如果电机控制需求明确，可提前。起点：ADC 多通道 DMA 采样 + TIM1/HRTIM PWM 输出框架 + CORDIC 封装
3. **待办小项**：串口实测（接 ST-LINK VCP 后 `dev.py console`）；把 MISRA 接入 `dev.py verify`；cppcheck 的 VS Code 集成

### 5.3 环境备忘（本机）

- 工具：ARM GCC `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin`、CMake `C:\Program Files\CMake\bin`、Ninja（WinGet Packages 目录）、cppcheck `C:\Program Files\Cppcheck`、J-Link `C:\Program Files\SEGGER\JLink_V964`
- Python：项目 `.venv`（uv sync 创建，含 pyserial）；`uv run python tools/devtool.py <cmd>`
- 烧录：J-Link 需连接板子（SWD）；`dev.py flash` 自动烧 boot+app
- 日志：板子无串口通道时用 `dev.py log`（RAM 镜像）；接 ST-LINK VCP 后可用 `dev.py console`
- 已安装外部 skill：embeddedskills（~/.pi/agent/skills/，jlink/serial/gcc/openocd 等 12 个），jlink skill 已配置 STM32G474RE/SWD/4000

## 6. 关键文档索引

| 文档 | 内容 |
|---|---|
| AGENTS.md | 项目准则（规范/MISRA/红线/提交要求）——**每次对话必读** |
| docs/architecture.md | 架构分层、中断优先级约定、模块设计 |
| docs/flash-partition.md | Flash 分区与 OTA 扩展方案 |
| docs/pinmap.md | 引脚分配（M1 已占用 + 电机预留） |
| docs/milestones.md | 里程碑路线 |
| .pi/skills/stm32g474-devtools/references/troubleshooting.md | 排错手册（启动链路/症状速查） |
| tools/cppcheck-addons/misra.json | MISRA 豁免子集 |
