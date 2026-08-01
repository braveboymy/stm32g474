# stm32g474-platform

基于 STM32G474 的通用嵌入式平台（方向：电机控制），当前处于 **M1：平台最小系统**。

## 快速开始

```bash
# 1. 拉取第三方依赖（FreeRTOS-Kernel V11.1.0 + STM32CubeG4 v1.6.3，约 500MB，一次性）
tools/fetch_third_party.sh

# 2. 构建（需 arm-none-eabi-gcc ≥ 12、cmake ≥ 3.20、ninja）
tools/build.sh            # Release（-O2）
tools/build.sh Debug      # 调试（-Og）

# 3. 产物
build/bin/app.elf         # 含调试信息
build/bin/app.bin         # 烧录镜像（起始地址 0x08008000）
build/bin/app.hex
build/app.map             # 链接映射

# 4. 烧录（NUCLEO-G474RE 板载 ST-LINK）
openocd -f interface/stlink.cfg -f target/stm32g4x.cfg \
        -c "program build/bin/app.elf verify reset exit"
# 或
st-flash --reset write build/bin/app.bin 0x08008000
```

串口日志：115200 8N1，经板载 ST-LINK VCP（PC 端 COM 口），格式：

```
[      27] I/mon: stm32g474-platform v0.1.0 (xxx)
[      27] I/mon: SYSCLK=170000000 HCLK=170000000 PCLK1=170000000 PCLK2=170000000
[      27] I/mon: heap total=49152 free=43664
[      28] I/led: task started
[    5028] I/mon: task led       prio=1 state=b stack_hw=...
```

## 目录结构

```
├── app/            # 业务层（业务任务，业务未定时只放平台演示任务）
├── bsp/            # 板级支持：board / clock / led / uart / msp / timebase
│   ├── startup/    # 启动文件（从 CMSIS 模板复制，项目所有）
│   ├── system/     # system_stm32g4xx.c（VECT_TAB_OFFSET=0x8000）
│   └── linker/     # 应用链接脚本（分区见 docs/flash-partition.md）
├── core/           # 平台核心（硬件无关，可移植，重点投入）
│   ├── osal/       # OS 抽象层（业务只依赖此接口）
│   ├── kernel/     # FreeRTOS 钩子（栈溢出/断言/内存失败）
│   ├── log/        # 分级日志（临界区保护，任务/中断均可用）
│   └── util/       # 环形缓冲等通用组件
├── config/         # FreeRTOSConfig.h / stm32g4xx_hal_conf.h / platform.h
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
