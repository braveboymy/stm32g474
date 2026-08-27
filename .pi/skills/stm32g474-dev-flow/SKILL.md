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

本 skill 定义**流程编排**（何时做什么、失败怎么处理）；**命令用法一律以
`stm32g474-devtools/SKILL.md` 命令参考为准**（怎么做），不在此重复命令表。

统一入口（AGENTS.md 强制）：`python .pi/skills/stm32g474-devtools/scripts/dev.py`
（引擎读项目根 `devtool.conf`，参数见「配置」节）。

## 0. 环境自检（首次使用必做）

**第一次拿到本项目时，必须先运行 `dev.py info`，然后按结果引导用户补齐环境。**

LLM 的处理规则：

1. 逐项确认工具在位，把缺失项一次性列给用户（不要装一个问一次）
2. 每个缺失项给出可复制安装命令：python+uv / cmake / ninja / cppcheck 用
   `winget install <包名>`；arm-none-eabi-gcc 下载 ARM GNU Toolchain 加入 PATH；
   J-Link 从 SEGGER 官网安装（安装目录自动探测，本机 V688c）
3. pyserial 仅 `dev.py console` 需要，缺则 `uv add pyserial`
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
| MISRA | Cppcheck | 必须 | `dev.py misra`（addon 随 skill 分发，无需配置） |

## 1. 代码编写

- 遵循 `AGENTS.md`：分层依赖单向（app → core → bsp → hal）；C11；`core/` 硬件无关可单测；
  外设寄存器走 HAL/LL；第三方代码（`third_party/`、`bsp/system`、`bsp/startup`）不改不评
- 分区：bootloader `0x08000000`（32KB）/ app `0x08008000`（448KB）/ 参数 `0x08078000`（32KB），
  修改分区/链接需同步更新 `docs/` 与 `devtool.conf`
- **修改代码后必须先编译通过再进入下一阶段**

## 2. 固件编译

- 命令：`dev.py build [Debug|Release]`（见 devtools 命令参考）
- 失败时：读取输出错误详情 → 定位修复 → 重编，循环直至通过
- 产物：`build/bin/app.bin` + `build/bin/bootloader.bin`
- 本工程**无 CMakePresets.json**，embeddedskills/gcc 的 preset 功能不可用，
  仅用其 `gcc_size.py analyze` 看 ELF 大小等辅助场景

## 3. 固件烧录

- 命令：`dev.py flash`（烧录 bootloader + app 并校验，复位运行）
- 失败时检查：先 `dev.py connect` 测 J-Link 连接 → 查 SWD 接线/目标供电/芯片型号 →
  核对 `devtool.conf` 的 jlink 参数

## 4. 在线调试与验证

- 命令：`dev.py verify`（一键闭环） / `status` / `log` / `regs` / `console`
- 问题定位优先级：J-Link RAM 日志 + 寄存器（`status`/`log`/`regs`）> 串口 console
- ISR 内问题注意时序，避免单步跟踪过深

## 5. 单元测试（core 层 PC 单测）

- 命令：`dev.py test`（host gcc，无需硬件；被测模块登记表在 devtool.conf `tests.sources`）
- 改完 `core/`（osal/log/util 等硬件无关层）后**必须**跑单测，回归通过再固件编译

## 6. MISRA C:2012 合规检测

- 命令：`dev.py misra [dir...]`（addon 随 skill 分发；豁免清单见
  `docs/misra_deviation.md`，规则要求见 AGENTS.md §4）
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

- 命令参考：`stm32g474-devtools/SKILL.md`（dev.py 全命令，唯一命令源）
- 平台闭环 skill：`stm32g474-devtools`
- 底层工具 skill：`embeddedskills/{gcc,jlink,keil,openocd,serial,workflow}`（通用技能包）
- 项目特定：`tools/scenarios/`（回归场景）；third_party 已裁剪入库，无拉取脚本
- 架构/规则参考：`AGENTS.md`、`CONTEXT.md`、`docs/`