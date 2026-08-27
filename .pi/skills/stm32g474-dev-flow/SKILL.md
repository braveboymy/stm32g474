---
name: stm32g474-dev-flow
description: >-
  STM32G474 固件的 LLM 全自主开发闭环：环境自检 → 代码编写 → 固件编译 → 固件烧录 →
  在线验证/调试 → 单元测试 → MISRA C:2012 合规检测。当用户要求"全自动开发/编译/烧录/
  验证/跑单测/查 MISRA"、或首次接触本项目需要搭建环境、或询问"环境缺什么"时触发。
  首次使用必须先运行环境自检，缺什么补什么，然后才进入开发闭环。
argument-hint: "[env|build|flash|debug|test|misra|full] ..."
---

# STM32G474 固件 LLM 全自主开发闭环

本 skill 定义 STM32G474 项目的完整自主开发流程（由 ai_stm32_prj 的 hil_dev_flow
迁移并适配本仓库）。LLM 应依次执行各阶段，每个阶段完成后根据结果决定下一步，
实现"写代码 → 生成固件 → 烧录 → 验证 → 单测 → MISRA 检测"的无人工干预闭环。

**命令入口优先使用项目脚本**（AGENTS.md 强制）：
`stm32g474-devtools/scripts/dev.py` 为统一入口（引擎 `scripts/devtool.py` 读项目根 `devtool.conf`）。

## 0. 环境自检（首次使用必做）

**第一次拿到本项目时，必须先运行环境自检，然后按结果引导用户补齐环境。**

```bash
uv run python .pi/skills/stm32g474-devtools/scripts/dev.py info
```

输出工具链与固件信息（cmake / arm-none-eabi-gcc / Ninja / J-Link / python+uv）。
LLM 的处理规则：

1. 逐项确认工具在位，把缺失项一次性列给用户（不要装一个问一次）
2. 每个缺失项给出可复制安装命令，例如：
   - python+uv: `winget install Python.Python.3.12` / `winget install astral-sh.uv`
   - cmake: `winget install Kitware.CMake`
   - arm-none-eabi-gcc: 下载 ARM GNU Toolchain 并加入 PATH
   - ninja: `winget install Ninja-build.Ninja`
   - J-Link 软件（SEGGER 安装目录自动探测，本机已装 JLink_V688c；目标 STM32G474RE，SWD 4MHz）
   - cppcheck: `winget install Cppcheck.Cppcheck`（MISRA 检测需要；misra addon 已随仓库 `tools/cppcheck-addons/`）
3. pyserial 仅 `dev.py console`（串口日志）需要，缺则 `uv add pyserial`
4. 全部就绪后进入闭环；**必须工具**：python+uv、cmake、arm-none-eabi-gcc、ninja、
   cppcheck；J-Link 为烧录/调试所必需（本平台统一走 J-Link，无 OpenOCD 场景）

### 环境清单（dev.py info 实际情况为准）

| 类别 | 工具 | 必须/可选 | 用途 |
|------|------|----------|------|
| 运行时 | Python + uv | 必须 | 运行所有工具脚本 |
| 构建 | cmake + arm-none-eabi-gcc + ninja | 必须 | CMake 编译固件 |
| 烧录 | J-Link（SEGGER 安装目录自动探测，本机 V688c） | 必须 | SWD 4MHz 烧录/调试/读 RAM 日志 |
| 烧录 | ST-LINK VCP（板上串口） | 可选 | `dev.py console` 串口日志 |
| 单测 | host gcc | 必须 | `dev.py test` core 层 PC 单测 |
| MISRA | Cppcheck + tools/cppcheck-addons/misra.json | 必须 | MISRA C:2012 检测 |

## 1. 代码编写

- 遵循 `AGENTS.md` 分层架构（依赖方向单向，禁止反向）：
  `app/`（业务任务）→ `core/`（平台核心：osal/log/util，硬件无关）→
  `bsp/`（板级）→ `hal/`（第三方）
- 内存分区：bootloader `0x08000000`（32KB）/ app `0x08008000`（448KB）/
  参数区 `0x08078000`（32KB），修改分区/链接需同步更新 `docs/` 说明
- `core/` 禁止依赖 `bsp/` 与 HAL（硬件无关，可 PC 单测）；业务代码只经 `osal/` 访问 RTOS
- 外设寄存器统一走 HAL/LL，`core/` 层禁止直接碰寄存器（详见 AGENTS.md）
- 语言 C11；第三方代码（`third_party/`、`bsp/system`、`bsp/startup`）不改不评
- **修改代码后必须先编译通过再进入下一阶段**

## 2. 固件编译

```bash
# 推荐（项目脚本，默认 Release；也可 Debug）
uv run python .pi/skills/stm32g474-devtools/scripts/dev.py build
uv run python .pi/skills/stm32g474-devtools/scripts/dev.py build Debug
```

- 编译失败时：读取输出中的错误详情，自行定位修复代码后重编，循环直至通过
- 产物：`build/bin/app.bin`（还有个 `build/bin/bootloader.bin`）
- 说明：本工程**无 CMakePresets.json**，embeddedskills/gcc 的 preset 功能不可用，
  仅用其 `gcc_size.py analyze` 看 ELF 大小等辅助场景

## 3. 固件烧录

```bash
uv run python .pi/skills/stm32g474-devtools/scripts/dev.py flash
```

- J-Link SWD 4MHz，烧录 bootloader(0x08000000) + app(0x08008000) 并校验，完成后复位运行
- 失败时检查：先 `dev.py connect` 测 J-Link 连接；再查 SWD 接线、目标供电、芯片型号
- 烧录参数（设备/分区/bin 路径）见项目根 `devtool.conf`

## 4. 在线调试与验证

```bash
# 一键验证（flash 后读寄存器确认时间基准/LED + 拉取 RAM 日志）
uv run python .pi/skills/stm32g474-devtools/scripts/dev.py verify

# 寄存器级状态（TIM6 tick / LED，可调采样）
uv run python .pi/skills/stm32g474-devtools/scripts/dev.py status

# J-Link 读 RAM 日志镜像（无需串口），--tail 控制条数
uv run python .pi/skills/stm32g474-devtools/scripts/dev.py log --tail 40

# CPU 寄存器读取（卡死/启动失败定位）
uv run python .pi/skills/stm32g474-devtools/scripts/dev.py regs

# 串口实时日志（需要 ST-LINK VCP + pyserial），--list 列出可用串口
uv run python .pi/skills/stm32g474-devtools/scripts/dev.py console
```

- 问题定位优先级：J-Link RAM 日志 + 寄存器（`status`/`log`/`regs`）> 串口 console
- ISR 内问题注意时序，避免单步跟踪过深

## 5. 单元测试（core 层 PC 单测）

```bash
# host gcc 编译运行，无需硬件
uv run python .pi/skills/stm32g474-devtools/scripts/dev.py test
```

- 改完 `core/`（osal/log/util 等硬件无关层）后必须跑单测，回归通过再固件编译

## 6. MISRA C:2012 合规检测

```bash
python .pi/skills/stm32g474-devtools/scripts/dev.py misra           # 范围 core bsp app bootloader（第三方豁免）
python .pi/skills/stm32g474-devtools/scripts/dev.py misra core      # 定向目录
```

- 规则子集与豁免理由见 AGENTS.md「MISRA C:2012」章节（cppcheck --addon=misra，
  misra.json 在 `tools/cppcheck-addons/`）
- 规则：
  - **本项目代码（core/bsp/app/bootloader）违规必须处理**：
    Required/Mandatory 直接修复；Advisory 评估后修复或记录 deviation
  - **第三方/生成代码违规**（third_party/ 豁免）：写 deviation 说明，不修改
  - 修复后重跑直到本项目代码无违规（或仅剩已登记 deviation）
  - deviation 登记到 `docs/misra_deviation.md`（规则号、文件位置、理由、批准人）

## 7. 完整闭环（full）

对用户"全自动跑一遍"的要求，按以下顺序执行：

1. `dev.py info` 环境自检 → 缺失先引导安装
2. `dev.py test` 单元测试（core 层回归）
3. `dev.py build` 编译 → 失败修复循环
4. `dev.py flash` 烧录
5. `dev.py verify` 在线验证（寄存器 + RAM 日志）
6. `dev.py misra` MISRA 检测 → 违规修复循环
7. 汇总报告：环境 / 单测 / 构建 / 烧录 / 验证 / MISRA 状态与整改清单

每个阶段失败时：**定位 → 修复 → 重跑本阶段**，不要跳过。

## 配置

工程参数在项目根 `devtool.conf`（devtools 引擎统一读取）：
- `jlink.device/interface/speed`：STM32G474RE / SWD / 4000
- `bins`：bootloader@0x08000000 + app@0x08008000（flash 顺序/校验）
- `build`：Ninja + cmake/toolchain-arm-none-eabi.cmake
- `misra`/`tests`：检查范围、include/defs、单测登记表

## 相关资源

- 平台闭环 skill：`stm32g474-devtools`（scripts/dev.py 为统一入口）
- 底层工具 skill：`embeddedskills/{gcc,jlink,keil,openocd,serial,workflow}`
- 项目特定工具：`tools/fetch_third_party.sh`（依赖锁定）、`tools/scenarios/`（回归场景）
- 架构/规则参考：`AGENTS.md`、`CONTEXT.md`、`docs/`
