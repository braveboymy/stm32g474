# 里程碑路线图

| 阶段 | 内容 | 状态 |
|---|---|---|
| M1 | 工程骨架：CMake + FreeRTOS + log + UART + LED + sysmon | ✅ 进行中 |
| M2 | 健壮性：HardFault 现场保存/崩溃报告、看门狗、故障管理框架（core/fault） | ⬜ |
| M3 | 参数存储（Flash 模拟 EEPROM + 版本/校验/恢复默认）、CLI | ⬜ |
| M4 | 通信：通用帧协议 + UART/CAN + USB-CDC | ⬜ |
| M5 | 信号链（电机方向重点）：ADC+HRTIM 同步采样、CORDIC/FMAC 封装、PID、PWM 框架 | ⬜ |
| M6 | OTA：Bootloader + 分区升级 + 回滚 | ⬜ |
| M7 | PC 端单元测试（core/ 层）+ CI（编译/单测/静态检查） | ⬜ |

## M1 验收标准

- [x] CMake 交叉编译通过，产物 elf/bin/hex/map
- [x] 时钟 170MHz（HSE 24MHz → PLL）
- [x] LED 闪烁（500ms，任务驱动）
- [x] 串口分级日志（含时间戳/任务名）
- [x] sysmon 周期输出任务表与堆水位
- [x] 板卡实测（J-Link 烧录 + RAM 日志验证：时钟 170MHz、任务调度、堆稳定）
