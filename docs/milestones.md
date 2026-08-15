# 里程碑路线图

| 阶段 | 内容 | 状态 |
|---|---|---|
| M1 | 工程骨架：CMake + FreeRTOS + log + UART + LED + sysmon | ✅ 完成（遗留：mon 任务表 malloc 失败，见下） |
| M2 | 健壮性：HardFault 现场保存/崩溃报告、看门狗、故障管理框架（core/fault） | ✅ 完成 |
| M3 | 参数存储（Flash 模拟 EEPROM + 版本/校验/恢复默认）、CLI | ⬜ |
| M4 | 通信：通用帧协议 + UART/CAN + USB-CDC | 🔶 USB-CDC 完成（2026-08-15）；帧协议/CAN 待做 |
| M5 | 信号链（电机方向重点）：ADC+HRTIM 同步采样、CORDIC/FMAC 封装、PID、PWM 框架 | ⬜ 业务核心 |
| M6 | OTA：Bootloader + 分区升级 + 回滚 | ⬜ |
| M7 | PC 端单元测试（core/ 层）+ CI（编译/单测/静态检查） | 🔶 单测完成（2026-08-15）；CI 待做 |

## M1 验收标准

- [x] CMake 交叉编译通过，产物 elf/bin/hex/map
- [x] 时钟 160MHz（HSE 8MHz → PLL，DevEBox 定制板）
- [x] LED 闪烁（D1 快闪 1s 周期 / D2 慢闪 2s 周期，共阳极低电平点亮 PC13/PD2）
- [x] 串口分级日志（含时间戳/任务名）
- [x] sysmon 周期输出任务表与堆水位
- [x] 板卡实测（J-Link 烧录 + RAM 日志验证：时钟 160MHz、任务调度、堆稳定）

## M1 遗留问题

- **mon 任务表 pvPortMalloc 失败**：heap free 充足却 malloc 失败（heap_4 链表疑被踩），任务表一直未打印。
  影响面：疑似内存踩踏，M5 信号链开发前必须排查（排查工具：dev.py regs + fault 栈回溯）。

## M4/M7 已完成增量（2026-08-15）

- **M4 · USB-CDC**：全套移植 `bsp/usb_device/`（CDC ACM、PID 0x5740、HSI48 时钟、补 USB_LP_IRQHandler），
  USB COM 心跳/回显实测通过；兼容 `usb_cdc` API
- **M7 · PC 单测**：`tests/core/` 极简零依赖框架（rb 8 用例 + log 5 用例 + mock osal），
  `dev.py test` 一键运行（host gcc，无需硬件）；pre-commit 已纳入
