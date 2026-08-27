# stm32g474-platform

> 面向人类读者的项目导航。**LLM/agent 开发时以 [AGENTS.md](AGENTS.md) 为准则、以 `.pi/skills/stm32g474-devtools/SKILL.md` 为命令参考**，无需通读本文件。

基于 STM32G474 的通用嵌入式平台（方向：电机控制）。

**当前进度**：M1 平台最小系统 ✅ / M2 健壮性 ✅ / M4 USB-CDC ✅（帧协议待做）/ M7 PC 单测 ✅（CI 待做）
下一阶段：**M5 信号链（ADC 同步采样 / CORDIC / PID / PWM，业务核心）**，详见 [docs/milestones.md](docs/milestones.md)。

## 快速开始

```bash
# 0. 开发工具（uv 管理 Python 环境，零依赖 + pyserial）
uv sync

# 1. 构建（third_party/ 已随仓库裁剪入库，无需拉取）
python .pi/skills/stm32g474-devtools/scripts/dev.py build [Debug|Release]

# 3. 开发闭环（统一入口，参数见 devtool.conf）
python .pi/skills/stm32g474-devtools/scripts/dev.py connect   # J-Link 连接测试
python .pi/skills/stm32g474-devtools/scripts/dev.py flash     # 烧录 bootloader + app（0x08000000 + 0x08008000）
python .pi/skills/stm32g474-devtools/scripts/dev.py status    # 读寄存器验证固件运行（TIM6 tick / LED）
python .pi/skills/stm32g474-devtools/scripts/dev.py log       # J-Link 读 RAM 日志镜像（无需串口）
python .pi/skills/stm32g474-devtools/scripts/dev.py console   # 串口看日志（有 ST-LINK VCP 时）
python .pi/skills/stm32g474-devtools/scripts/dev.py test      # core 层 PC 单测（host gcc 无需硬件）
python .pi/skills/stm32g474-devtools/scripts/dev.py misra     # MISRA C:2012 检查
```

全部命令参考见 `.pi/skills/stm32g474-devtools/SKILL.md`。

## 项目级 Skill（.pi/skills/stm32g474-devtools）

编译/烧录/仿真验证/日志获取已沉淀为项目级 skill，一键闭环：

```bash
python .pi/skills/stm32g474-devtools/scripts/dev.py verify   # build -> flash -> status -> log
python .pi/skills/stm32g474-devtools/scripts/dev.py regs     # 卡死时读 CPU 寄存器 + addr2line 定位
python .pi/skills/stm32g474-devtools/scripts/dev.py debug    # LLM 调试代理（断点 + 事件采集）
```

排障经验（启动链路坑位、J-Link 注意事项、症状速查）见
`.pi/skills/stm32g474-devtools/references/troubleshooting.md`。

## 目录结构

根目录按用途分四类（`*` = 生成物/依赖，git 忽略，可随时重建）：

**代码**（嵌入式工程标准分层）
```
├── app/          # 业务层（main.c + 业务任务 tasks/）
├── bootloader/   # 引导程序（32KB @ 0x08000000）
├── bsp/          # 板级支持：board / clock / led / uart / usb / msp / timebase
│   ├── startup/  # 启动文件（项目所有）
│   ├── system/   # system_stm32g4xx.c（VECT_TAB_OFFSET=0x8000）
│   └── linker/   # 应用链接脚本
├── core/         # 平台核心（硬件无关，重点投入）
│   ├── osal/     # OS 抽象层（业务只依赖 osal.h）
│   ├── fault/    # 故障管理（M2）
│   ├── log/      # 分级日志（任务/中断均可用）
│   └── util/     # 环形缓冲等通用组件
├── config/       # 配置头：FreeRTOSConfig.h / HAL conf / platform.h / usbd_conf.h
├── cmake/        # 工具链文件（toolchain-arm-none-eabi.cmake）
├── tests/        # PC host 单测（core + app）
└── docs/         # 架构 / 分区 / 引脚 / 里程碑 / 规范 / MISRA 偏离 + 参考 PDF
```

**依赖与生成物**（git 忽略）
```
├── third_party/   # 第三方依赖，已裁剪入库（548MB → 7.8MB，仅保留本项目用到的部分，见 third_party/README.md）
├── build/        # * CMake 构建产物（.bin/.map/日志）
└── .venv/        # * Python 工具环境（uv sync 创建）
```

**工具链与配置**
```
├── tools/                     # 项目特定：scenarios（回归场景）
├── .pi/skills/                # 项目技能：stm32g474-devtools（开发闭环，含 pre-commit 钩子模板）/ embeddedskills（通用）
├── devtool.conf               # 开发工具配置（设备/分区/bin/构建/MISRA/单测）——唯一需要改的工程参数
├── pyproject.toml + uv.lock   # Python 工具环境清单（uv sync）
└── .clang-format              # 代码格式（clang-format）
```

**准则与文档**
```
├── AGENTS.md    # 项目工作准则（agent 最高准则：规范 / MISRA / 红线）
├── CONTEXT.md   # 项目上下文与领域模型
├── README.md    # 本说明
├── .gitignore / .gitattributes
└── CMakeLists.txt             # 顶层构建定义
```

## 依赖与版本锁定

| 组件 | 版本 | 状态 |
|---|---|---|
| FreeRTOS-Kernel | V11.1.0 | 随仓库提交（裁剪：内核 + ARM_CM4F + heap_4） |
| STM32CubeG4 | v1.6.3（HAL/CMSIS/USB 子模块固定 SHA） | 随仓库提交（裁剪：G4 HAL 18 源文件 + CMSIS + USB-CDC） |
| 工具链 | ARM GNU Toolchain ≥ 12 | developer.arm.com |

裁剪边界、License 与升级流程见 `third_party/README.md`。

## 文档

- [架构与决策](docs/architecture.md)
- [Flash 分区](docs/flash-partition.md)
- [硬件原理图参考（引脚/排针/器件，唯一事实源）](docs/hardware-schematic.md)
- [里程碑路线图](docs/milestones.md)
- [编码规范](docs/coding_standard.md)
- [MISRA 偏离管理](docs/misra_deviation.md)
