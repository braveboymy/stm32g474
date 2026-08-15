# Flash 分区（第一天定死，OTA 后置但布局不再变）

STM32G474RET6：Flash 512KB（双 Bank，每 Bank 32 × 8KB 扇区），起始 0x08000000。

## 当前布局

| 区域 | 地址范围 | 大小 | 说明 |
|---|---|---|---|
| Bootloader | 0x08000000 – 0x08007FFF | 32KB | 预留（M6 实现，UART/CAN/USB 升级） |
| **应用** | 0x08008000 – 0x08077FFF | **448KB** | 本工程，链接脚本 `stm32g474ret6_app.ld` |
| 参数区 | 0x08078000 – 0x0807FFFF | 32KB | EEPROM 仿真/参数存储（M3） |

## 与代码的对应关系

- 链接脚本 `bsp/linker/stm32g474ret6_app.ld`：FLASH 起点 0x08008000
- 向量表偏移：`VECT_TAB_OFFSET=0x00008000U`（CMake 编译定义，system_stm32g4xx.c 使用）
- 烧录：app.bin 必须烧到 0x08008000（openocd 用 elf 自动定位；st-flash 需显式地址）

## OTA 扩展方案（已定：外挂 SPI Flash 暂存，见 ADR-0001）

- **方案 C（选定）**：外挂 SPI Flash（≥1MB）暂存新固件 + 单 bank 校验升级。片内分区零改动：下载到 SPI Flash → 校验 → 标记待升级 → 复位进 bootloader → 复制到 app bank → 启动校验，失败从 SPI Flash 上一版本镜像回滚
- **方案 B（否决-可复议）**：双 Bank A/B 交替。当前布局可拆 224K+224K 槽位，但需 DBANK option byte + bootloader 重构；若未来 app 逼近 224KB 或需零停机升级，可 supersede ADR-0001 重新评估
- **外挂 SPI Flash 布局（预留）**：暂存区 448KB / 上一版本镜像 448KB / 元信息区（容量 ≥1MB 的 SPI NOR，如 W25Q 系列）
- 传输通道：UART-YModem / CAN / USB（M6 定）

## 注意事项

- 应用镜像**必须**链接在 0x08008000（首 32KB 留给 Bootloader，即使 Bootloader 尚未实现）
- 参数区放在 Bank2 尾部，方便后续做 EEPROM 仿真（需要整扇区擦除）
- 升级校验：CRC32 或 SHA-256，M6 定
