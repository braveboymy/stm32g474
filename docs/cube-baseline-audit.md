# CubeMX 官方基线审计（挽救记录）

> 目的：将 LLM 手写/拼装的芯片基础设施件（链接脚本/启动/系统/配置）对齐到
> ST 官方基线，使每个文件可追溯为「官方模板 + 显式修改」，并修复审计中发现的缺陷。
> 日期：2026-08 ｜ 参考工程：`C:\Users\bot\Desktop\demo_prj\demo`
> （STM32CubeMX 生成，FW_G4 V1.6.3，CMake 工具链，NUCLEO-G474RE 板卡模板）

## 1. 参考工程基线文件

| 官方文件 | 用途 |
|---|---|
| `STM32G474xx_FLASH.ld` | 链接脚本基线（含 .tdata/.tbss TLS、.RamFunc、_sstack、__data_* PROVIDE） |
| `startup_stm32g474xx.s` | 启动文件基线（与项目文件仅差 3 处，见下） |
| `Core/Src/system_stm32g4xx.c` | 系统初始化（与项目文件 **0 差异**，同版本） |
| `Core/Inc/stm32g4xx_hal_conf.h` | HAL 模块使能基线（按需裁剪风格） |
| `Core/Src/main.c` | SystemClock_Config 官方写法（HSI→PLL 170MHz，VOS BOOST + FLASH_LATENCY_4） |
| `cmake/gcc-arm-none-eabi.cmake` | 官方编译选项（-mcpu/-mfpu/-fstack-usage/-print-memory-usage） |
| `cmake/stm32cubemx/CMakeLists.txt` | 官方 CMake 组织方式（INTERFACE + OBJECT 库） |

## 2. 逐件审计结论

| 文件 | 结论 | 处置 |
|---|---|---|
| `bsp/startup/startup_stm32g474xx.s` | ✅ 官方件 + **3 处**差异：SVC/PendSV/SysTick 向量表直连 `vPortSVCHandler`/`xPortPendSVHandler`/`xPortSysTickHandler`（FreeRTOS V11 必需，AGENTS.md 红线）；另 1 处 SystemInit 调用顺序差异（CMSIS 标准顺序，无害） | 保持 |
| `bsp/system/system_stm32g4xx.c` | ✅ 与官方 0 差异 | 保持 |
| `bsp/linker/stm32g474ret6_app.ld` | ⚠️ 手写，缺官方段（.RamFunc/.tdata/.tbss/_sstack/__data_* 等） | **已重写**（官方基线 + 项目修改点，见文件头注释） |
| `bootloader/linker/stm32g474ret6_boot.ld` | ⚠️ 同上 | **已重写** |
| `config/stm32g4xx_hal_conf.h` | ⚠️ 全模块使能（与官方裁剪风格不符） | **已裁剪**：使能集 = CMake 实际编译源文件（配置=源文件一致性） |
| `CMakeLists.txt` | ⚠️ 发现 VTOR 宏缺陷（见 §3） | **已修复**：app 加 `USER_VECT_TAB_ADDRESS`；补 `-fstack-usage`（官方选项） |

## 3. 审计发现的缺陷（已修复）

### 3.1 VTOR 静默失效（严重）
- **现象**：`system_stm32g4xx.c`（v1.6.3）用 `USER_VECT_TAB_ADDRESS` 宏保护
  `SCB->VTOR` 写入，而工程只定义了旧的 `VECT_TAB_OFFSET` → **SystemInit 不写 VTOR**。
- **后果**：app 向量表留在 0x08000000，仅靠 bootloader 跳转前设置 VTOR 兜底；
  调试器直接烧 app / 跳过 bootloader / M6 改造 bootloader 时，中断向量表全部错位
  （症状：能运行，一进中断就 HardFault）。
- **修复**：`CMakeLists.txt` app 目标加 `USER_VECT_TAB_ADDRESS`。
- **验证**：objdump 确认 `SystemInit` 现编译出 `str r1,[r3,#8]`（VTOR=0x08008000）。

### 3.2 链接脚本与官方基线偏差（中等）
- 手写 .ld 缺：`.glue_7/.glue_7t/.eh_frame`、`.RamFunc`、`.tdata/.tbss`（TLS）、
  `_sstack` 符号、`__data_start/__data_size` 等 PROVIDE。
- **处置**：以官方 `STM32G474xx_FLASH.ld` 为基线重写，保留项目 4 个显式修改点：
  1) FLASH 起点 0x08008000/448KB（boot 分区）；2) RAM 拆 SRAM1 96K + SRAM2 32K；
  3) 新增 `.sram2` 段（电机 DMA 预留）；4) `_Min_Stack_Size` = 8KB（中断嵌套+控制环）。
  每个修改点都在 .ld 头部注释中登记。

### 3.3 hal_conf 全模块使能（低）
- 使能集改为 = CMake 编译源文件（GPIO/DMA/RCC/FLASH/PWR/CORTEX/UART/TIM/IWDG/PCD），
  其余模块注释并标注启用时机（M3 参数存储 / M5 信号链 / M6 OTA）。
- 规则：**启用模块必须同时满足** ① CMakeLists 已编译对应 Src；② 业务已使用。
  启用新模块时同步在 CMakeLists 增加源文件（如 EXTI：加 `stm32g4xx_hal_exti.c`）。

## 4. 与官方基线的持久差异清单（有意保留，勿"修正"）

| 差异 | 理由 |
|---|---|
| app .ld 分区（0x08008000 / 448K / 参数区） | bootloader + OTA 布局（docs/flash-partition.md） |
| SRAM1/SRAM2 拆分 + .sram2 段 | 电机控制 DMA 缓冲隔离（docs/architecture.md §5） |
| 栈 8KB / bootloader 栈 2KB | 中断嵌套 + 控制环 ISR 预算 |
| FreeRTOS v11 向量表直连（startup 3 处） | FreeRTOS V11.1.0 必需（AGENTS.md 红线） |
| hal_timebase.c / msp.c / retarget.c / usb_cdc.c | HAL weak 符号覆盖（必须留在 app 可执行文件） |
| FreeRTOSConfig.h / osal | 自建 V11.1.0 集成，不用 CubeMX 的 CMSIS-RTOS 封装 |
| 时钟：当前 HSI 16MHz 定位模式 | 定制板 bring-up 中间态；正式方案见 §5 |

## 5. 时钟恢复路径（bring-up 完成后）

参考工程官方写法（HSI→PLL 170MHz）与 git 历史方案（HSE→PLL 170MHz）结构一致
（VOS SCALE1_BOOST + FLASH_LATENCY_4 + 预取/缓存）。恢复顺序建议：
1. `HSI 16MHz + PLL → 170MHz`（参考工程官方配置，不依赖 HSE，可先全速验证调度/外设）
2. `HSE 8MHz（DevEBox）→ PLL → 170MHz`（M=4,N=85,R=2：8/4=2MHz，2×85=170MHz，R=DIV2 需改 N=170 或 M=2；以 CubeMX 图形界面自动计算为准）

## 6. 验证记录

- `dev.py build`：✅ app.bin 35556B / bootloader 884B（重链接成功）
- `bash tools/check_misra.sh`：✅ 0 违规
- 布局符号（app.map）：`_estack=0x20018000`、`_sstack=0x20016000`、
  `.sram2@0x20018000`、`SystemInit` 写 VTOR=0x08008000 ✅
- 板卡实测：待 `dev.py verify`（有板时）
