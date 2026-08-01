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

## OTA 扩展方案（M6 再细化，布局已兼容）

- 方案 A（简单）：Bootloader + 单应用 + 参数区，整包校验后覆盖升级，异常回退到 Bootloader 等待重刷
- 方案 B（A/B）：双 Bank 交替升级。当前布局中 Bootloader 32KB + 应用 448KB + 参数 32KB 已按"应用可用双 Bank 对半拆"预留（Bank1 尾部 + Bank2 可拆为两个 224KB 槽位），届时只调整链接脚本
- 传输通道：UART-YModem / CAN / USB（M6 定）

## 注意事项

- 应用镜像**必须**链接在 0x08008000（首 32KB 留给 Bootloader，即使 Bootloader 尚未实现）
- 参数区放在 Bank2 尾部，方便后续做 EEPROM 仿真（需要整扇区擦除）
- 升级校验：CRC32 或 SHA-256，M6 定
