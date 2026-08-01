# 平台架构

## 1. 设计决策（已确认）

| 决策点 | 结论 | 说明 |
|---|---|---|
| 内核 | FreeRTOS（V11.1.0） | 业务任务跑 RTOS；控制环等硬实时逻辑放中断裸跑，不经过 RTOS |
| 业务方向 | 电机控制（FOC 类） | 信号链能力（ADC 同步采样、PWM、CORDIC/FMAC）优先建设 |
| 开发板 | NUCLEO-G474RE | 平台先跑通，定制板只改 bsp/ 引脚与时钟 |
| OTA | M6 后置 | 但 Flash 分区第一天定死（见 flash-partition.md） |

## 2. 分层

```
┌─────────────────────────────────────────────┐
│ app/        业务任务（业务未定，先放平台演示）   │
├─────────────────────────────────────────────┤
│ core/       平台核心：osal/log/util/fault(规划)│  ← 硬件无关，PC 可单测
├─────────────────────────────────────────────┤
│ bsp/        板级：clock/gpio/uart/led/...     │  ← 换板只改这里
├─────────────────────────────────────────────┤
│ hal/        ST HAL（第三方）  freertos/ 内核   │
└─────────────────────────────────────────────┘
```

依赖方向：`app → core → bsp → hal/freertos`，禁止反向。

## 3. 中断优先级约定（Cortex-M4，4 位优先级 0~15）

| 优先级 | 用途 | 是否可调 RTOS API |
|---|---|---|
| 0~4 | 电机控制环、硬件保护（COMP→PWM 刹车走硬件，不进中断） | 否 |
| 5 | UART 等普通外设中断（= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY） | 可 |
| 6~14 | 预留 | 可 |
| 15 | SysTick / PendSV / TIM6(HAL 时间基准) | 内核管理 |

## 4. 核心模块

### core/osal（OS 抽象）
业务代码只依赖：任务创建/延时/延时到点、互斥锁、临界区、tick、堆水位。
换内核只改 `osal_freertos.c`。

### core/log（日志）
- 分级：DEBUG < INFO < WARN < ERROR（编译期可裁剪到只留 ERROR）
- 格式：`[tick_ms] 级别/标签(任务名): 消息`
- 临界区保护（BASEPRI），任务与中断上下文均可调用
- 输出后端可插拔（当前 UART，未来可加 SEGGER RTT / 环形缓冲异步刷盘）

### 健壮性（fail-fast 原则）
- `configASSERT` → 记录 + 停机
- 栈溢出（configCHECK_FOR_STACK_OVERFLOW=2）→ 记录 + 停机
- malloc 失败 → 记录 + 停机
- HAL Error_Handler → 停机
- M2 升级：HardFault 现场保存 + 复位后崩溃报告 + 统一故障管理（core/fault）

## 5. 内存布局

- SRAM1 96KB：.data/.bss/堆（FreeRTOS heap_4，48KB）/主栈（8KB）
- SRAM2 32KB：预留 `.sram2` 段（电机控制 ADC DMA 缓冲等高优先级数据）
- FreeRTOS：idle/timer 任务静态分配，业务任务 heap_4 动态分配

## 6. 已知取舍（M1）

- UART 发送为逐字节中断模式（115200 下 ~87µs/字节），接 CLI 后升级 DMA
- 停机策略优先于恢复策略（业务定了再按故障分级调整）
- 日志临界区最长屏蔽中断约 10µs（256B vsnprintf @170MHz），控制环 ISR 在 0~4 级不受影响
