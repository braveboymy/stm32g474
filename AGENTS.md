# AGENTS.md — STM32G474 嵌入式平台开发准则

本文件是本项目的最高工作准则。任何代码改动必须遵守本文档的
**代码规范**、**MISRA C:2012 要求**与**嵌入式专项红线**。

## 1. 项目概述

- **硬件**：STM32G474RET6（Cortex-M4F @170MHz，512KB Flash，128KB RAM），NUCLEO-G474RE 开发板
- **软件栈**：FreeRTOS V11.1.0 + ST HAL（STM32CubeG4 v1.6.3），C11，arm-none-eabi-gcc
- **架构分层**（依赖方向单向，禁止反向）：
  ```
  app/（业务任务）→ core/（平台核心：osal/log/util，硬件无关）→ bsp/（板级）→ hal/（第三方）
  ```
- **分区**：bootloader 0x08000000（32KB）/ app 0x08008000（448KB）/ 参数 0x08078000（32KB）
- **业务方向**：电机控制（FOC 类），信号链能力优先；控制环等硬实时逻辑放中断裸跑，不经过 RTOS

## 2. 开发闭环（必须使用，禁止臆造命令）

所有编译/烧录/调试/日志操作必须走项目脚本：

```bash
python .pi/skills/stm32g474-devtools/scripts/dev.py build|flash|status|log|regs|verify|test
# 等价底层：uv run python tools/devtool.py <cmd>；烧录 J-Link SWD 4MHz
# test：core 层 PC 单测（rb/log，host gcc 无需硬件，见 tools/run_core_tests.sh）
```

- 修改代码后**必须** `dev.py build` 验证编译通过
- 涉及硬件行为（调度、外设、启动链路）的改动，有板子时用 `dev.py verify`（烧录+运行验证+日志）闭环
- 启动/调度类问题定位：`dev.py regs`（PC+addr2line）→ 排错手册
  `.pi/skills/stm32g474-devtools/references/troubleshooting.md`

## 3. 代码规范

- **语言**：C11（不引入 C++）；第三方代码（`third_party/`、`bsp/system`、`bsp/startup`）不改不评
- **命名**：函数/变量小写下划线；模块前缀（`uart_`/`rb_`/`osal_`/`log_`）；宏全大写；类型 `_t` 后缀
- **头文件**：必须 include guard（`#ifndef XXX_H`）；头文件只放声明与文档，实现细节留在 .c
- **分层纪律**：core/ 禁止依赖 bsp/ 与 HAL（硬件无关，PC 可单测）；业务代码只经 osal/ 访问 RTOS
- **静态分析**：除 MISRA 外，代码应无 `-Wall -Wextra` 警告

## 4. MISRA C:2012 强制要求（必须满足）

**所有本项目代码（app/core/bsp/bootloader，第三方豁免）必须通过 MISRA 检查：**

```bash
bash tools/check_misra.sh          # 全量
bash tools/check_misra.sh <路径>    # 单目录/文件
```

### 4.1 强制规则子集（豁免清单 = 检查工具自动跳过）

以下规则因嵌入式/RTOS 生态原因**项目级豁免**（理由必须可追溯，见下表）：

| 豁免规则 | 理由 |
|---|---|
| 2.1-2.7 | 未使用代码/注释：编译器 -Wall 已覆盖；HAL 死代码噪音 |
| 3.x, 4.x | 注释风格、字符集（advisory） |
| 5.1-5.9 | 标识符唯一性/长度：HAL/FreeRTOS 风格 |
| 6.x, 7.x | 位字段、常量细节（未使用/ advisory） |
| 8.4 | 框架回调原型在第三方头（FreeRTOS/HAL 约定入口） |
| 8.7 | 单 TU 函数 static 化：HAL 回调必须外部可见 |
| 8.9 | static 模块私有状态（嵌入式惯例，advisory） |
| 9.2-9.5 | 初始化细节（advisory） |
| 10.5-10.8 | 整数类型体系细节（uint32 生态惯例） |
| 11.1 | 函数指针转换：FreeRTOS 任务入口适配 |
| 11.3-11.5 | 指针↔整数：寄存器/外设地址映射惯例 |
| 14.2, 14.3 | 循环形式（advisory） |
| 15.5 | cppcheck addon 对带表达式 return 的误报（代码审查兜底） |
| 16.4-16.7 | switch/函数细节（advisory） |
| 17.1 | restrict 限定（advisory） |
| 17.3 | 隐式函数声明检测：cppcheck 宏展开误报，-Wall 兜底 |
| 17.7 | 返回值必须使用：FreeRTOS API 忽略返回值是惯例 |
| 17.8 | 不得修改参数：out 参数是嵌入式惯例 |
| 20.10 | `#`/`##` 宏运算符：HAL 大量使用 |
| 21.2 | 保留标识符：HAL/FreeRTOS 下划线前缀 |
| 21.6 | stdio：**仅允许** snprintf/vsnprintf 用于日志格式化；禁止 printf/scanf 家族 |
| 22.1 | 动态内存：FreeRTOS heap_4 是平台机制（禁止业务代码自建 malloc） |

豁免规则之外的违规**必须清零**。三条途径（按优先级）：
1. **修复代码**（首选）
2. **行内豁免**（仅限合理场景，必须带原因）：
   ```c
   // cppcheck-suppress misra-c2012-10.4  原因：<一句话理由>
   ```
3. **补充豁免表**（涉及整类场景时，需在 4.1 表中登记理由，不得静默豁免）

### 4.2 MISRA 高频违规点（本工程实测，写代码时直接规避）

- 整数字面量加 `U` 后缀：`size - 1U`、`+ 1U`（Rule 10.4）
- 复合赋值/自增不要嵌在表达式中：`s_buf[n] = 'x'; n = n + 1;`（Rule 13.3）
- 指针条件必须显式比较：`if (p != NULL)`（Rule 14.4）
- 指针算术用下标+取址：`&buf[n]` 而非 `buf + n`（Rule 18.4）
- 表达式加括号明确优先级（Rule 12.1）

## 5. 嵌入式专项红线（违反=启动失败/随机故障）

- **启动链路**（详见排错手册）：
  - `bsp/startup/startup_stm32g474xx.s` 的 SVC/PendSV/SysTick 条目**必须**直接指向
    `vPortSVCHandler`/`xPortPendSVHandler`/`xPortSysTickHandler`（FreeRTOS v11 无宏替换机制）
  - 链接脚本 `.init/.fini` 必须 `KEEP()`（`--gc-sections` 会裁掉 crtn 结尾）
  - 覆盖 HAL weak 符号的文件（`hal_timebase.c`/`msp.c`/`retarget.c`）必须留在 **app 可执行文件**里，
    不能移入静态库（链接顺序会让 weak 实现胜出）
  - `HAL_InitTick` 覆盖实现必须同步 `uwTickPrio`
- **RTOS 纪律**：中断服务程序只调用 `*FromISR` API；优先级 ≤4 的中断（控制环）不调用任何 RTOS API；
  业务代码禁止直接调用 FreeRTOS API（走 osal/）
- **内存**：优先静态分配；任务栈 512B 起步并查水位（`dev.py log` 的 sysmon 输出）；
  业务代码禁止裸 malloc（用 osal/FreeRTOS 堆）
- **硬件访问**：外设寄存器统一走 HAL/LL；core/ 层禁止直接碰寄存器

## 6. 提交要求

- **pre-commit 钩子已启用**（`tools/githooks/pre-commit`，安装：`bash tools/install_hooks.sh`）：
  提交前自动执行 `dev.py build` + `bash tools/check_misra.sh`，失败则阻止提交
- 紧急跳过：`SKIP_CHECKS=1 git commit ...` 或 `git commit --no-verify`（事后必须补跑检查）
- 硬件行为改动注明验证方式（`dev.py verify` 输出 / RAM 日志证据）
