# stm32g474-platform

基于 STM32G474 的通用嵌入式平台（方向：电机控制），当前处于 **M1：平台最小系统**。

## 快速开始

```bash
# 0. 开发工具（uv 管理 Python 环境，零依赖 + pyserial）
uv sync

# 1. 拉取第三方依赖（FreeRTOS-Kernel V11.1.0 + STM32CubeG4 v1.6.3，约 500MB，一次性）
tools/fetch_third_party.sh

# 2. 构建
uv run python tools/devtool.py build [Debug|Release]

# 3. J-Link 操作（NUCLEO-G474RE，SWD）
uv run python tools/devtool.py connect   # 连接测试
uv run python tools/devtool.py flash     # 烧录 bootloader + app（0x08000000 + 0x08008000）
uv run python tools/devtool.py status    # 读寄存器验证固件运行（TIM6 tick / LED）
uv run python tools/devtool.py log       # J-Link 读 RAM 日志镜像（无需串口）
uv run python tools/devtool.py console   # 串口看日志（有 ST-LINK VCP 时）
```

烧录也可以直接用已安装的 embeddedskills 的 jlink skill：

```bash
python ~/.pi/agent/skills/jlink/scripts/jlink_exec.py flash --file build/bin/app.bin --address 0x08008000
```

## 项目级 Skill（.pi/skills/stm32g474-devtools）

编译/烧录/仿真验证/日志获取已沉淀为项目级 skill，一键闭环：

```bash
python .pi/skills/stm32g474-devtools/scripts/dev.py verify   # build -> flash -> status -> log
python .pi/skills/stm32g474-devtools/scripts/dev.py regs     # 卡死时读 CPU 寄存器 + addr2line 定位
python .pi/skills/stm32g474-devtools/scripts/dev.py log      # J-Link 读 RAM 日志（无串口可用）
```

排障经验（启动链路坑位、J-Link 注意事项、症状速查）见
`.pi/skills/stm32g474-devtools/references/troubleshooting.md`。

## 目录结构

```
├── app/            # 业务层（应用入口 main.c + 业务任务）
│   └── tasks/      # 业务任务（task_demo / task_sysmon）与任务声明 tasks.h
├── bsp/            # 板级支持：board / clock / led / uart / msp / timebase
│   ├── startup/    # 启动文件（从 CMSIS 模板复制，项目所有）
│   ├── system/     # system_stm32g4xx.c（VECT_TAB_OFFSET=0x8000）
│   └── linker/     # 应用链接脚本（分区见 docs/flash-partition.md）
├── core/           # 平台核心（硬件无关，可移植，重点投入）
│   ├── osal/       # OS 抽象层 + FreeRTOS 集成（业务只依赖 osal.h）
│   ├── fault/      # 故障管理：现场采集/栈回溯/复位上报（M2）
│   ├── log/        # 分级日志（临界区保护，任务/中断均可用）
│   ├── util/       # 环形缓冲等通用组件
│   └── test/       # PC 端 host 单测落点（M7 实施，见 test/README.md）
├── config/         # FreeRTOSConfig.h / stm32g4xx_hal_conf.h / platform.h / usbd_conf.h
├── docs/           # 架构 / 分区 / 引脚 / 里程碑
├── tools/          # 构建与第三方依赖脚本
└── third_party/    # git 忽略，由 fetch_third_party.sh 拉取（版本锁定）
```

## 依赖与版本锁定

| 组件 | 版本 | 来源 |
|---|---|---|
| FreeRTOS-Kernel | V11.1.0 | github.com/FreeRTOS/FreeRTOS-Kernel |
| STM32CubeG4 | v1.6.3（含 HAL/CMSIS/BSP 子模块固定 SHA） | github.com/STMicroelectronics/STM32CubeG4 |
| 工具链 | ARM GNU Toolchain ≥ 12 | developer.arm.com |

详见 `third_party/README.md`。

## 文档

- [架构与决策](docs/architecture.md)
- [Flash 分区](docs/flash-partition.md)
- [引脚分配](docs/pinmap.md)
- [里程碑路线图](docs/milestones.md)
