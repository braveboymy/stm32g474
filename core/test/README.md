# core/test —— PC 端 host 单测（M7 实施）

> 目录落点（M1 预留）：core/ 硬件无关模块（osal/log/util）的宿主编译测试。
> 实施里程碑：M7（见 docs/milestones.md）。

## 为什么需要 host 测试

- core/ 被设计为硬件无关（不依赖 bsp/ 与 HAL），这是它的核心承诺；
- 单测是承诺的证明：日志格式、环形缓冲边界、临界区 token 配对等
  在 PC 上可快速、可重复验证，不占用开发板与 J-Link。

## 接入方案（M7 落地时按此执行）

| 文件 | 内容 |
|---|---|
| `host_osal.c/.h` | osal.h 的 PC 实现：tick 用宿主时钟、临界区空操作、task name 固定串 |
| `test_rb.c` | 环形缓冲：init/write/read/peek/skip/used/free、满丢弃、2 的幂边界 |
| `test_log.c` | 分级日志：级别过滤、格式（tick/级别/标签/任务名）、RAM 镜像回绕 |
| `CMakeLists.txt` | 独立 host 构建（系统 CMake，**不使用** cmake/toolchain-arm-none-eabi.cmake） |

构建：`cmake -S core/test -B build-host && cmake --build build-host`
（CI 阶段接入：见 M7 里程碑）。

## 注意事项

- 测试代码不受"目标代码 MISRA 21.6 仅限 snprintf"豁免约束；
  check_misra.sh 全量扫描含本目录时，若 stdio 输出被报违规，
  对测试文件使用行内豁免并注明"host 测试代码"；
- 不得 include bsp/ 或 HAL 头文件——本目录是 core 可移植性的守卫。
