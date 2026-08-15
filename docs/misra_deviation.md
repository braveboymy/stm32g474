# MISRA C:2012 偏离（Deviation）管理

更新时间：2026-08-15
配套：`tools/check_misra.sh`（cppcheck addon，强制检测）、AGENTS.md §4（豁免清单）

## 1. 原则

- **目标**：豁免清单（AGENTS.md §4.1）之外的 MISRA 违规必须清零
- **优先级**：修复代码 > 行内豁免（带理由）> 登记豁免表（整类场景）
- **禁止**：静默豁免——任何跳过检查的行为必须可追溯（登记在案）

## 2. 违规处置流程

```
检出违规（check_misra.sh / pre-commit）
   │
   ├─ 可修复 → 修复代码（首选），回归 build + misra
   │
   ├─ 合理行内豁免 → 加 cppcheck-suppress 注释 + 一句话理由
   │      例：// cppcheck-suppress misra-c2012-10.4  原因：与 HAL 宏类型一致
   │
   └─ 整类场景 → 评估是否进入豁免表（AGENTS.md §4.1）：
        1. 在本文件 §3 登记：规则、范围、理由、日期
        2. 同步更新 AGENTS.md §4.1 表
        3. 禁止在未登记情况下用工具排除规则
```

## 3. 存量违规登记表

> 存量代码（2026-08-15 前）中暂未整改的违规在此登记；新代码不允许新增。
> 每项须有整改计划与负责人/批次。

| # | 文件/模块 | 规则 | 违规描述 | 整改计划 | 状态 |
|---|-----------|------|----------|----------|------|
| （暂无登记） | | | | | |

## 4. 检查工具使用说明

```bash
bash tools/check_misra.sh           # 全量（core bsp app bootloader，第三方豁免）
bash tools/check_misra.sh <路径>    # 单目录/文件
```

- 第三方代码（`third_party/`、`bsp/system`、`bsp/startup`）不参与检查
- 豁免规则由 `tools/cppcheck-addons/misra.json` 承载（与 AGENTS.md §4.1 表一一对应）
- 修改豁免配置必须同步更新 AGENTS.md §4.1 表与本文件 §3

## 5. 常见违规速查（写代码时规避）

| 违规 | 规避写法 |
|------|----------|
| 整数字面量无 U 后缀（10.4） | `size - 1U`、`+ 1U` |
| 自增/复合赋值嵌表达式（13.3） | 拆行：`s_buf[n] = 'x'; n = n + 1;` |
| 指针条件隐式（14.4） | `if (p != NULL)` |
| 指针算术（18.4） | `&buf[n]` 而非 `buf + n` |
| 优先级歧义（12.1） | 表达式加括号 |
| stdio 家族（21.6） | 仅 `snprintf/vsnprintf` 用于日志 |
| 动态内存（22.1） | FreeRTOS 堆 / 静态分配，禁止裸 malloc |
