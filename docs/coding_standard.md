# STM32G474 平台编码规范

更新时间：2026-08-15
适用范围：`app/`、`core/`、`bsp/`（`bsp/system`、`bsp/startup`、`third_party/` 为第三方/生成代码，不强制）
依据：AGENTS.md（最高准则）、MISRA C:2012（安全规则）、BARR-C:2018（风格参考）
配套工具：`tools/check_misra.sh`（强制检测）、clang-format（格式强制，见第 6 章）

> 本文档是 AGENTS.md 第 3 章的细化执行版。新代码必须遵守；存量代码按第 7 章流程逐步整改。

---

## 1. 命名规范

### 1.1 模块前缀

每个模块有唯一前缀，所有对外符号（函数、类型、宏）必须带前缀：

| 层 | 模块 | 前缀 | 示例 |
|----|------|------|------|
| core | OSAL | `osal_` | `osal_task_delay_ms` |
| core | 环形缓冲 | `rb_` | `rb_write` |
| core | 日志 | `log_` | `log_init` |
| core | 故障 | `fault_` | `fault_freeze` |
| bsp | 板级 | `bsp_` | `bsp_board_init` |
| bsp | LED | `led_` | `led1_toggle` |
| bsp | UART | `uart_` | `uart_init` |
| bsp | USB CDC | `usb_cdc_` | `usb_cdc_send` |
| app | 任务 | `task_` | `task_demo_entry` |

文件内 `static` 函数/变量不需要前缀，但命名规则同样适用。

### 1.2 函数命名

格式：`前缀 + 动词 + 名词`。

动词约定（全项目统一，禁止混用）：

| 动词 | 含义 | 示例 |
|------|------|------|
| `init` / `deinit` | 生命周期 | `led_init` |
| `get` | 读值（无副作用） | `SystemClock_GetFreqs` |
| `set` / `write` / `config` | 写值 | `log_set_level` |
| `read` | 读并返回快照 | `usb_cdc_read` |
| `send` / `feed` / `push` | 数据流输出/输入 | `usb_cdc_send` |
| `is` / `has` | 谓词，返回 0/1 | `usb_cdc_connected` |
| `handle` / `on` | 事件/中断回调 | `HAL_PCD_DataOutStageCallback` |
| `toggle` | 状态翻转 | `led1_toggle` |

规则：
- 谓词函数返回 `bool`，禁止返回"模糊整数"
- 返回状态码的函数，调用方必须检查返回值；显式忽略必须 `(void)` 并注释理由
- 函数名不超过 45 字符，过长说明职责过多，应拆分

### 1.3 变量命名

| 作用域 | 风格 | 示例 |
|--------|------|------|
| 局部变量 | 小写下划线 | `sample_rate_hz` |
| 文件内 static | `s_` 前缀 | `s_rx_rb`、`s_hpcd` |
| 全局（尽量避免） | `g_` 前缀 | `g_sysmon_beat` |

**度量单位必须进名字**（消除歧义的关键）：

```
elapsed_duration_ms      // 毫秒
timeout_ms               // 毫秒
baudrate                 // 波特
sample_rate_hz           // 赫兹
raw_code                 // 无单位原始值
```

布尔变量用谓词形式：`is_ready`、`busy`、`connected`（禁止 `flag`、`status` 这类模糊名）。

### 1.4 类型与宏

- 自定义类型：小写下划线 + `_t` 后缀：`rb_t`、`uart_cfg_t`
- 宏：全大写 + 下划线：`USB_CDC_RX_BUF_SIZE`
- 枚举成员：全大写 + 模块前缀（防冲突）
- 结构体成员：小写下划线，无前缀
- 函数指针类型：后缀 `_fn`

### 1.5 文件命名

- 小写下划线：`usb_cdc.c`、`task_demo.c`
- 头文件与源文件同名；头文件只放声明与文档，实现细节留在 .c
- 头文件必须 include guard（`#ifndef XXX_H`）

## 2. 代码风格

| 项 | 规则 |
|----|------|
| 缩进 | 4 空格（禁 Tab） |
| 行宽 | 120 列以内 |
| 大括号 | 函数定义独立一行（Allman）；`if/for/while` 同行（K&R），单语句也必须带大括号 |
| 指针 | `Type* p`（星号靠类型，PointerAlignment: Left） |
| 换行 | 二元运算符换行放行首；函数参数换行时一个一行 |
| 空白 | `if (...) ` 控制语句有空格；函数调用 `f(...)` 无空格 |
| 注释 | 中文注释；`/* */` 块注释解释"为什么"，不解释"是什么" |

## 3. 头文件纪律

- 头文件只声明与文档，实现细节留在 .c
- `#include` 顺序：自身头 → 标准库 → 第三方（HAL/FreeRTOS）→ 项目模块（core → bsp → app）
- 禁止在头文件定义变量（`static inline` 除外）；禁止头文件互相包含成环

## 4. 分层纪律（AGENTS.md 第 3 章强制）

- 依赖方向单向：`app/ → core/ → bsp/ → hal/`，禁止反向
- `core/` 禁止依赖 `bsp/` 与 HAL（硬件无关，PC 可单测）
- 业务代码只经 `osal/` 访问 RTOS，禁止直接调用 FreeRTOS API
- 外设寄存器统一走 HAL/LL；`core/` 层禁止直接碰寄存器

## 5. MISRA C:2012 高频违规点（写代码时直接规避）

- 整数字面量加 `U` 后缀：`size - 1U`、`+ 1U`（Rule 10.4）
- 复合赋值/自增不要嵌在表达式中（Rule 13.3）
- 指针条件必须显式比较：`if (p != NULL)`（Rule 14.4）
- 指针算术用下标+取址：`&buf[n]` 而非 `buf + n`（Rule 18.4）
- 表达式加括号明确优先级（Rule 12.1）
- 禁止 `printf/scanf` 家族（仅允许 `snprintf/vsnprintf` 用于日志，Rule 21.6）

豁免清单见 AGENTS.md 4.1 表（必须可追溯），新增豁免必须登记理由。

## 6. clang-format（格式强制）

- 配置：`.clang-format`（与本规范第 2 章一致，`BreakBeforeBraces: Linux`）
- 格式化：`clang-format -i <file>`（或 `pip install clang-format` 后使用）
- 检查：pre-commit 钩子对改动文件执行 `clang-format --dry-run --Werror`
- 存量代码：首次全量格式化（一次性基准提交），此后新改动必须格式合规

## 7. 存量代码整改流程

1. 新代码（新文件/新增函数）必须完全符合本文档
2. 修改存量函数时，顺手整改该函数的格式与明显命名违规
3. 涉及面大的违规（如模块重命名）登记到 `docs/session-summary.md`，分批整改
4. 不允许"为整改而整改"引入行为变化——格式与重构分开提交

## 8. 提交要求

- pre-commit 自动执行：`dev.py build` + `tools/check_misra.sh` + clang-format 检查
- 提交信息：`<type>(<scope>): <summary>`，type 用 `fix/feat/docs/refactor/chore/test`
- 硬件行为改动必须注明验证方式（`dev.py verify` 输出 / RAM 日志证据）
