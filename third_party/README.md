# 第三方依赖（裁剪入库）

本目录随仓库提交（不再 git 忽略、无需拉取脚本），**仅保留本项目实际使用的部分**，
原始体积 548MB → 7.8MB（瘦身 98.6%）。升级依赖 = 对照下表从上游替换对应文件 + 全链路回归。

## 来源与版本锁定

| 组件 | 版本/Tag | 锁定 SHA | 用途 |
|---|---|---|---|
| FreeRTOS-Kernel | V11.1.0 | dbf70559b27d39c1fdb68dfb9a32140b6a6777a0 | RTOS 内核 |
| STM32CubeG4 | v1.6.3 | d11b194a9f05d1b143d154771f3dbc282c8052a5 | HAL/CMSIS/USB 中间件 |
| └ HAL 子模块 | — | f5929f431f9effe45fbe18f5337e4753ced9ac92 | stm32g4xx_hal_driver |
| └ CMSIS 设备子模块 | — | 25664ddc3a7624ae9627ae8c4c672073dc5b2539 | cmsis_device_g4 |
| └ USB 设备库子模块 | — | 60d163f271987fd322e22f63c095836e1dc703f6 | stm32_mw_usb_device |

## 保留内容（裁剪边界）

```
FreeRTOS-Kernel/
├── include/ + 6 个内核 .c（tasks/queue/list/timers/event_groups/stream_buffer）
└── portable/GCC/ARM_CM4F + portable/MemMang/heap_4.c
    # 已删：portable 其余 40+ 移植、examples、docs、CI 等

STM32CubeG4/
├── Drivers/STM32G4xx_HAL_Driver/
│   ├── Src/ 18 个 .c（按 CMakeLists.txt hal 目标编译集）
│   └── Inc/ 19 个头 + Legacy/（hal_def.h 依赖，勿删）
├── Drivers/CMSIS/Include/（Cortex-M4 内核头）
├── Drivers/CMSIS/Device/ST/STM32G4xx/Include/（stm32g4xx.h + stm32g474xx.h + system_stm32g4xx.h）
└── Middlewares/ST/STM32_USB_Device_Library/（Core + Class/CDC，USB-CDC 用）
    # 已删：Projects（281MB 示例）、DSP/docs/NN、Utilities、BSP、其余 Middlewares
```

已删内容的恢复方式：`git clone --depth 1 --branch v1.6.3` + 子模块初始化后拷贝。

## License

各组件许可文件随源码保留（FreeRTOS: `LICENSE.md`（MIT）；CubeG4: 根 `LICENSE.md`、
`Drivers/CMSIS/LICENSE.txt`、HAL/USB/Device 各自 `LICENSE*.md`）。再分发合规，勿删除。

## 升级流程

1. 上游切换版本后，按上表 SHA 对应的 tag 用 `diff` 找出差异文件
2. 只替换保留集内文件（或按需扩充保留集）
3. 回归：`dev.py build` + `dev.py test` + `dev.py misra`（+ 有板子时 `dev.py verify`）