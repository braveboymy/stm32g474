# 排错手册：启动链路与常见症状

本文件来自 STM32G474 平台 M1 实测排障记录（J-Link 逐层定位），
按"症状 → 根因 → 处置"组织。排查工具：`dev.py regs / status / log`。

## 0. 快速定位流程

1. `dev.py regs`：看 PC、SP、IPSR
   - PC 在 app 区正常轮转 + PSP 非 0 → 系统在跑，问题在别处
   - PC 在 `vAssertCalled`/`assert_failed`（IPSR=0）→ 断言失败，见 §3
   - PC 在 HardFault handler（IPSR=3）→ 读 CFSR/HFSR 定位，见 §4
   - PC 在 boot 区 → boot 未跳转，见 §1
2. 断言/故障消息在 UART TX 环形缓冲里（若 log_init 前崩溃则无）
3. 按症状表对照

## 1. 启动链路关键点（改动前先核对）

```
复位 → 0x08000000(boot 向量表) → boot main（校验 app 向量表）→ 跳 0x08008000
     → app Reset_Handler（拷 .data/清 .bss/__libc_init_array）→ main → 调度器
```

| 环节 | 要求 | 违反后果 |
|---|---|---|
| 0x08000000 有 boot | 即使不用 OTA，M1 也必须有 bootloader 占位 | 复位后从空 Flash 取向量 → lockup |
| boot 跳转 | `__disable_irq(); __set_MSP(app_sp); SCB->VTOR=APP_BASE;` 用 naked 汇编跳转 | 栈切换后局部变量 UB → HardFault |
| 链接脚本 `.init/.fini` | **必须 `KEEP(*(.init))` / `KEEP(*(.fini))`** | `--gc-sections` 裁掉 crtn.o 结尾 → `_init` fall-through 执行数据 → UNDEFINSTR |
| app 向量表 SVC/PendSV/SysTick | FreeRTOS v11 要求**直连** `vPortSVCHandler`/`xPortPendSVHandler`/`xPortSysTickHandler`（v11 移除了宏替换机制） | 中断进 Default_Handler 死循环或启动断言 |
| `configCHECK_HANDLER_INSTALLATION` | GNU 下**必须设 0**：`.word` 引用 Thumb 函数链接器置位 0（条目=符号\|1），与符号比较恒不等 | 启动即断言（port.c:343） |
| 覆盖 HAL weak 符号的文件 | `hal_timebase.c`（HAL_InitTick）/`msp.c`（MspInit）/`retarget.c`（_write）**必须编入 app 可执行文件**，不能放静态库 | 静态库顺序导致 weak 实现胜出（HAL 默认 SysTick 版 HAL_InitTick 被链接）→ TIM6 不跑 |
| `HAL_InitTick` 覆盖实现 | **必须写 `uwTickPrio = TickPriority;`**（hal.c 内部状态） | HAL_RCC_ClockConfig 用初始值 16 调 HAL_InitTick → NVIC 优先级断言（hal_cortex.c:191） |
| HAL 模块 ex 拆分 | `hal_uart_ex.c`/`hal_tim_ex.c`/`hal_dma.c` 必须编译（HAL 主文件无条件引用 Ex 回调） | 链接 undefined reference：HAL_UARTEx_*/HAL_TIMEx_* |

## 2. J-Link 工具注意事项

- **count 参数按十六进制解析**（V9.64 实测）：`mem32 addr, 128` 实际读 0x128=296 字；
  `savebin f, addr, 2048` 实际读 0x2048 字节。需要精确字节数时写成 `0x800` 形式。
- **`mem32` 大读取不可靠**：读取大量内存优先用 `savebin <file>, <addr>, <0xNNN>` 落盘再解析。
- **stdout 缓冲**：git-bash/mintty 下直接运行 JLink.exe 输出可能不可见（全缓冲+挂起），
  用 Python subprocess 管道或 cmd 重定向；必要时加 `-Log <file>`。
- **首次连接慢**：J-Link 首次连接可能触发固件更新（可长达 2 分钟），超时给足。
- 断点调试：`run-to <addr>` 前先复位（否则目标已卡死时断点永不命中）。

## 3. 断言（IPSR=0，PC 在 vAssertCalled / assert_failed）

定位断言消息：读 UART TX 环形缓冲（bsp/uart.c 的 s_tx_buf，地址从 app.map 查），
消息格式 `assert @ <文件>:<行>`。常见断言：

| 断言位置 | 根因 |
|---|---|
| `FreeRTOS port.c:343`（Tmr Svc 任务） | v11 向量表检查，见 §1 第 4/5 行 |
| `hal_cortex.c:191`（NVIC 优先级） | uwTickPrio 未同步（初始 16），见 §1 第 6 行 |
| `freertos_hooks.c`（stack overflow / malloc failed） | 任务栈过小 / 堆不足，增大 config 或栈 |

## 4. HardFault（IPSR=3）

读故障寄存器：`mem32 0xE000ED28, 4`（CFSR/HFSR/DFSR/MMFAR）：
- `UFSR=0x00080000`（UNDEFINSTR）→ 执行了非代码（空 Flash / 数据区 / 残缺函数），
  配合栈帧（SP 处 8 字：r0-r3/r12/LR/PC/XPSR）定位 fault 现场 PC
- `BFSR=0x00040000`（IBUSERR）→ 取指总线错误（空 Flash 典型）
- 栈帧解码：`[SP+0x18]` 为 fault 时 PC，`[SP+0x14]` 为 LR

## 5. 症状速查表

| 症状 | 根因 |
|---|---|
|---|---|
| TIM6 CNT=0 且不变 | HAL_InitTick 覆盖未生效（weak 链接顺序）或 uwTickPrio 断言前崩溃 |
| GPIOA ODR=0xFFFFFFAB 类垃圾值 | core 异常（lockup/HardFault）时的总线读 |
| `T-bit of XPSR is 0` 警告 | 复位后 core 未正常执行（向量表问题） |
| ODR 三次采样相同 | J-Link connect 时 halt 了 CPU（任务暂停），属正常现象 |
| `Verify successful` 但程序不跑 | 复位向量问题（boot 缺失/向量表错误） |
| IWDG 复位比预期快/慢 | LSI 本板实测 ≈96kHz（标称 32kHz 的 3 倍），换板/量产必须重新实测 LSI 并回来改 `bsp/board.h` 的 IWDG 分频/重载 |
| J-Link `Out of sync, resynchronizing` | 仿真器固件与 DLL 不兼容（兼容版 J-Link 常见），换老版本软件或更新固件 |
