# MISRA C:2012 偏离（Deviation）管理

更新时间：2026-08-27
配套：`dev.py misra`（cppcheck addon，强制检测，见 devtool.conf）、AGENTS.md §4（强制要求）

## 1. 原则

- **目标**：豁免清单（§2）之外的 MISRA 违规必须清零
- **优先级**：修复代码 > 行内豁免（带理由）> 登记豁免表（整类场景）
- **禁止**：静默豁免——任何跳过检查的行为必须可追溯（登记在案）

## 2. 项目级豁免清单

以下规则因嵌入式/RTOS 生态原因**项目级豁免**（与 cppcheck addon 配置
`.pi/skills/stm32g474-devtools/assets/cppcheck-addons/misra.json` 一一对应）：

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

## 3. 违规处置流程

```
检出违规（dev.py misra / pre-commit）
   │
   ├─ 可修复 → 修复代码（首选），回归 build + misra
   │
   ├─ 合理行内豁免 → 加 cppcheck-suppress 注释 + 一句话理由
   │      例：// cppcheck-suppress misra-c2012-10.4  原因：与 HAL 宏类型一致
   │
   └─ 整类场景 → 评估是否进入豁免表（§2）：
        1. 在本文件 §2 登记：规则、范围、理由、日期
        2. 同步更新 cppcheck addon 配置（misra.json 的 --suppress-rules）
        3. 禁止在未登记情况下用工具排除规则
```

## 4. 存量违规登记表

> 存量代码（2026-08-15 前）中暂未整改的违规在此登记；新代码不允许新增。
> 每项须有整改计划与负责人/批次。

| # | 文件/模块 | 规则 | 违规描述 | 整改计划 | 状态 |
|---|-----------|------|----------|----------|------|
| （暂无登记） | | | | | |

## 5. 检查工具使用说明

```bash
python .pi/skills/stm32g474-devtools/scripts/dev.py misra              # 全量（core bsp app bootloader，第三方豁免）
python .pi/skills/stm32g474-devtools/scripts/dev.py misra <路径>       # 单目录/文件
```

- 第三方代码（`third_party/`、`bsp/system`、`bsp/startup`）不参与检查
- 豁免规则由 `misra.json` 承载（与 §2 表一一对应）
- 修改豁免配置必须同步更新 §2 表与 misra.json

## 6. 常见违规速查（写代码时规避）

| 违规 | 规避写法 |
|------|----------|
| 整数字面量无 U 后缀（10.4） | `size - 1U`、`+ 1U` |
| 自增/复合赋值嵌表达式（13.3） | 拆行：`s_buf[n] = 'x'; n = n + 1;` |
| 指针条件隐式（14.4） | `if (p != NULL)` |
| 指针算术（18.4） | `&buf[n]` 而非 `buf + n` |
| 优先级歧义（12.1） | 表达式加括号 |
| stdio 家族（21.6） | 仅 `snprintf/vsnprintf` 用于日志 |
| 动态内存（22.1） | FreeRTOS 堆 / 静态分配，禁止裸 malloc |