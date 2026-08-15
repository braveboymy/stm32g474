# OTA 采用外挂 SPI Flash 暂存 + 单 bank 校验升级（预留接口，M6 实现）

OTA 是硬需求（远程升级 + 失败回滚），但 512KB 片内 Flash 无法容纳「448KB app × 2」的双 bank 布局；与其推倒已定死的分区（boot 32K / app 448K / params 32K）并改造 bootloader 与烧录链，决定**保持现有单 bank 分区不动**，OTA 走外挂 SPI Flash（≥1MB）暂存：下载到 SPI Flash → 校验（CRC/SHA）→ 标记待升级 → 复位进 bootloader → 复制到 app bank → 启动校验，失败则从 SPI Flash 中的上一版本镜像回滚。

Status: accepted (2026-08-15)

## Considered Options

- **方案 A（原文档预留）：片内双 bank A/B**。G474 原生支持（2×256KB，可拆 224K+224K 槽位），升级零停机，无新增 BOM。否决原因：需 DBANK option byte + bootloader 重构 + 链接脚本重排，而当前 app 仅 42KB、OTA 后置 M6，短期内收益低；若未来 app 逼近 224KB 或需要零停机升级，可重新评估（届时 supersede 本 ADR）。
- **方案 B：单 bank + 回出厂**。最简，但「回滚」只能回到出厂固件，不满足远程升级失败回上一版的硬需求。
- **方案 C（选定）：外挂 SPI Flash 暂存 + 单 bank 校验**。分区零改动、params 位置不变、NVM 驱动零迁移；代价是 BOM 增加一颗 SPI NOR（约 $0.1–0.3）且定制板必须为其预留引脚。

## Consequences

- **定制板硬件约束**：PCB 需为 SPI Flash 预留 SPI + CS（WP/HOLD 可省），bsp 层预留 SPI Flash 驱动接口与存储布局（暂存区 448KB / 上一版本镜像 448KB / 元信息区），升级状态记录放参数区。
- **升级窗口需停机**：bootloader 复制镜像期间设备重启——无人值守设备可接受（升级本就要复位）。
- 外挂 Flash 同样可为能量预算下的日志/事件缓冲提供空间（另议，不在本 ADR 范围）。
