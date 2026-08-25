---
name: stm32g474-devtools
description: >-
  STM32G474 平台开发闭环：编译固件、J-Link 烧录（bootloader+app）、仿真验证
  （寄存器级状态检查、CPU 寄存器读取）、日志获取（RAM 镜像/串口）、一键全流程
  verify。当用户要求编译/构建固件、烧录/下载程序、连接仿真调试、验证固件运行状态、
  获取板卡日志、排查启动失败/卡死，或提及 devtool/dev.py/J-Link 时使用。
---

# STM32G474 开发闭环

本 skill 提供本平台从代码到硬件验证的完整闭环：编译 → 烧录 → 仿真验证 → 日志获取。
所有操作均有脚本支撑（`scripts/dev.py`），可直接执行，也支持多步组合（`verify`）。

## 前置条件

- 工具链：arm-none-eabi-gcc、cmake、ninja（`tools/devtool.py` 自动探测，无串口场景也可用）
- J-Link 软件（SEGGER 安装目录：`C:/Program Files/SEGGER/JLink_*` 或 `(x86)/SEGGER/JLink_*`，devtool.py 自动探测取版本最大者；本机已装 JLink_V688c），目标 STM32G474RE，SWD 4MHz
- 若为克隆 J-Link：用环境变量 `JLINK_VERSION` 锁定版本（如 `JLINK_VERSION=688`），兼容版场景见 references/troubleshooting.md「Out of sync」
- Python：项目根执行过 `uv sync`（`console` 子命令需 pyserial，其余零依赖）

## 标准流程（build → flash → verify）

脚本入口：`scripts/dev.py`（内部复用项目 `tools/devtool.py`，保持单一实现）

```bash
# 1. 编译（Debug: -Og，Release: -O2）
python .pi/skills/stm32g474-devtools/scripts/dev.py build [Debug|Release]

# 2. 烧录：bootloader.bin -> 0x08000000，app.bin -> 0x08008000（自动校验）
python .pi/skills/stm32g474-devtools/scripts/dev.py flash

# 3. 一键验证：flash 后 3 秒，读寄存器确认时间基准/LED 活跃，再拉取 RAM 日志
python .pi/skills/stm32g474-devtools/scripts/dev.py verify
```

等价于分步执行 `dev.py build` → `dev.py flash` → `dev.py status` → `dev.py log`。

## 命令参考

| 命令 | 用途 | 典型输出 |
|---|---|---|
| `dev.py build [Debug\|Release]` | 编译固件 | `OK: build/bin/app.bin（N 字节）` |
| `dev.py flash` | J-Link 烧录 boot+app 并校验 | `烧录并校验成功，目标已复位运行` |
| `dev.py connect` | J-Link 连接测试 | `连接成功：目标已响应` |
| `dev.py status` | 采样 TIM6 计数 / GPIOA ODR 验证运行 | `TIM6 CNT: 运行中` |
| `dev.py log [--tail N]` | J-Link 读 RAM 日志镜像（无串口可用） | `[00035000] I/mon(mon): heap free=47400` |
| `dev.py console [--list]` | 串口日志（需 ST-LINK VCP） | 实时日志流，Ctrl+C 退出 |
| `dev.py regs` | halt 后读 CPU 寄存器 + addr2line 定位 PC | `PC = 0800BA5C` + 对应函数 |
| `dev.py verify` | 一键闭环（build→flash→status→log） | 全流程输出 |

## 日志获取（两种通道）

1. **RAM 镜像（推荐，无串口依赖）**：`dev.py log`
   - 固件侧：`log_enable_ram()`（core/log/log.c）将日志同时写入 2KB 环形缓冲
   - 工具侧：J-Link `savebin` 读缓冲 + 解析 head/tail 重组
2. **串口**：`dev.py console`（板载 ST-LINK VCP 未接时不可用，自动提示）

## 卡死/启动失败定位流程

1. `dev.py regs` → 看 PC 落在哪个函数（addr2line 自动给出）
2. 若 PC 在 `vAssertCalled`/`assert_failed`：读取 UART TX 缓冲找断言消息
   （`dev.py status` 前的 log 输出中已有线索；或按 references/troubleshooting.md 查症状表）
3. 若 PC 正常但 TIM6/ODR 无变化：按 troubleshooting.md 检查启动链路

## 参考

- [排错手册：启动链路与常见症状](references/troubleshooting.md)（本轮实测踩坑记录）
- 分区约定：`docs/flash-partition.md`（boot 32KB / app 448KB @0x08008000 / 参数 32KB）
- 底层工具：`tools/devtool.py`（uv 管理）、`tools/fetch_third_party.sh`（依赖锁定）
