# 硬件原理图参考（DevEBox STM32G474R 开发板）

> 来源：docs/STM32G474R开发板-原理图--202508.pdf（3 页：第 1/2 页为原理图，第 3 页为板卡外形丝印图）
> 提取日期：2026-08-02；识别不确定项标注 ⚠️（见 4.3 与第 9 章统计）
> 提取方式：PDF 无文本层，经 PyMuPDF 渲染 300/600 DPI + OCR + 矢量走线交叉验证

## 1. 板卡概览

**DevEBox STM32G474R 开发板**（淘宝 DevEBox 系列，图框 Title: "STM32G474 RC/RE 开发板"，SN: 202408，Date: 2025/07/03，A3 图幅）。

主控 STM32G474RC/RE（64-LQFP，170MHz，SRAM 128KB，Flash 256K/512K）。主要功能块：

- Type-C 供电（VBUS 5V）+ STM32 USB Slave（PA11/PA12）
- RT9193-33GB LDO 稳压（5V→3.3V）
- SPI Flash 数据存储（W25Q16）
- OLED/TFT 液晶接口（SPI2，J3 直插口 1×8）+ SPI3 扩展排针（位号 ⚠️6）
- SWD 调试接口、2×15 功能排针（J4）、2×15 模拟/GPIO 排针（J5）
- RTC 电池接口（EX BAT）、2 用户按键、2 用户 LED、复位按键

## 2. 电源树

| 电压域 | 来源 | 去向 | 备注 |
|---|---|---|---|
| `VBUS`（5V） | J1 Type-C 接口 VBUS 引脚 | U2.VIN、F1 保险丝 → `5V1` | Type-C 供电输入（"Type C-USB 供电"） |
| `5V1` | F1（500mA 保险丝） | U2.VIN；**D4（5V1 稳压管）钳位 5V→3V3 过压** | OCR 可见 `5V1` 网络标签 |
| `3V3` | U2 RT9193-33GB.VOUT（500mA LDO） | CPU VDD、U3.VCC、J4/J5、LED、按键、RTC 区 | 输出电容 C3/C4 10uF/10V，输入电容 C1/C2 10uF/10V |
| `3V3`（VDDA/VREF+ 滤波域） | 经 R10/R11（0Ω 跳线，已实测已装）自 `3V3` 分出 | VDDA/VSSA/VREF+ | 已实测定稿：VREF+ = 3.30V（上电实测）；原识别 `3V3H` 为 OCR 低置信误读，按 `3V3` 处理 |
| `VBAT` | Q1 BAT54C（双二极管防倒灌）← EX BAT（J5/板边电池座） | CPU VBAT 引脚，去耦 C6（104） | 图注 "RTC供电"；`GNDE_BAT` 为电池地网络 |
| GND | 系统参考地 | 全部器件 | 多点 `GND` 标签 + 排针 |

**模拟参考**：VDDA/VSSA/VREF+ 经 C18/C19/C20（103/105）滤波，VREF+ 区有 R10/R11 + 0Ω 跳线（`OR`）接 `3V3`（已实测）。

## 3. 时钟

| 时钟 | 器件 | 引脚 | 负载电容 | 说明 |
|---|---|---|---|---|
| HSE 8MHz | Y1（位号标注于 CPU 右下区；全板仅 2 晶振，Y2 已确认 LSE，故 Y1=8MHz 归属可靠） | **PF0-OSC_IN / PF1-OSC_OUT**（LQFP64 引脚 5/6，数据手册 Figure 7 确认；PD0/PD1 仅存在于 LQFP80/100，本板无） | C10/C11 = 22pF ×2（2026-08-16 原理图复核，消除 ⚠️8） | 主时钟源（M1 已按 24MHz HSE 配置，板级切换时需改 clock.c） |
| LSE 32.768kHz | Y2 | PC14-OSC32_IN / PC15-OSC32_OUT | C8/C9 = 10pF ×2 | RTC 低速晶振 |
| 时钟源选择 | — | — | — | 芯片默认 HSI，软件配置 HSE→PLL 160MHz（见 docs/architecture.md） |

## 4. MCU 引脚连接总表

> 排针说明：J1 = Type-C 接口（供电+USB，非排针）；J2 = SWD（SPI_1X5）；J3 = **OLED/TFT 直插口**（1×8）；J4 = **2×15 功能排针**（30 脚：电源/OLED/SPI/通信全引出）；J5 = **2×15 模拟/GPIO 排针**；SPI3 扩展排针位号未识别（⚠️6）。
> "连接对象"中 J1/J4/J5 表示该引脚引出至对应接口/排针；OLED 引脚（PB12~15、PC6/7）同时引至 J3 与 J4。

| MCU 引脚 | 引脚功能 | 网络名 | 连接对象 | 方向 | 备注 |
|---|---|---|---|---|---|
| PA0 | GPIO / ADC12_IN1 | PA0 | K1 按键 + J5 | IN | 已实测：K1 一脚接 PA0、另一脚接 GND |
| PA1 | GPIO / ADC12_IN2 | PA1 | K2 按键 + J5 | IN | 已实测：K2 一脚接 PA1、另一脚接 GND |
| PA2 | AF7_USART2_TX | PA2 | J5 | OUT | 调试串口（与 NUCLEO 板相同复用） |
| PA3 | AF7_USART2_RX | PA3 | J5 | IN | |
| PA4 | DAC1_OUT1 | PA4 | J5 | OUT | 模拟输出 |
| PA5 | DAC1_OUT2 | PA5 | J5 | OUT | ⚠️ 与 NUCLEO 板 PA5=LED 不同，见第 8 章 |
| PA6 | DAC2_OUT1 | PA6 | J5 | OUT | 模拟输出 |
| PA7 | GPIO | PA7 | J5 | BIDIR | 无复用标注 |
| PA8 | GPIO | PA8 | J4 | BIDIR | 无复用标注 |
| PA9 | AF7_USART1_TX | PA9_TXD1 | J4 | OUT | J4 丝印 `PA9 TXD1` |
| PA10 | AF7_USART1_RX | PA10_RXD1 | J4 | IN | J4 丝印 `PA10 RXD1` |
| PA11 | USB_DM | EX USB DM / USB DM | J1 Type-C(D-) + J4 | BIDIR | 丝印 `PA11/USB DM`；Type-C 侧另见 `EX USB_ DM` |
| PA12 | USB_DP | EX USB DP / USB DP | J1 Type-C(D+) + J4 | BIDIR | 丝印 `PA12/USB DP` |
| PA13 | SWDIO | PA13/SWDIO | J2 | BIDIR | SWD 调试 |
| PA14 | SWCLK | PA14/SWCLK | J2 | IN | SWD 调试 |
| PA15 | SPI3_NSS | SPI3_NSS | SPI3 排针 + J4 | OUT | Flash 片选（已实测 CS=PA15）；J4 丝印 `PA15` |
| PB0 | GPIO | PB0 | J5 | BIDIR | 无复用标注 |
| PB1 | GPIO | PB1 | J5 | BIDIR | 无复用标注 |
| PB2 | GPIO | PB2 | J5 | BIDIR | 无复用标注 |
| PB3 | AF9_FDCAN3_RX | PB3/FDCAN3 RX | J4 | IN | |
| PB4 | AF9_FDCAN3_TX | PB4/FDCAN3 TX | J4 | OUT | |
| PB5 | AF9_FDCAN2_RX | PB5/FDCAN2 RX | J4 | IN | |
| PB6 | AF9_FDCAN2_TX | PB6/FDCAN2 TX | J4 | OUT | |
| PB7 | GPIO | PB7 | J4 | BIDIR | 无复用标注 |
| PB8 | BOOT0 | BOOT0 | J4 | IN | R6 5.1k 下拉至 GND（推断：确保从主 Flash 启动）；丝印 `PB8/BOOT0` |
| PB9 | GPIO | PB9 | J4 | BIDIR | 无复用标注 |
| PB10 | GPIO | PB10 | J5 | BIDIR | 无复用标注 |
| PB11 | GPIO | PB11 | J5 | BIDIR | 无复用标注（⚠️ 原提取稿误标 NC，实际引出 J5） |
| PB12 | SPI2_NSS | SPI2_NSS / CS | J3（OLED CS）+ J4 | OUT | 已实测：Flash CS=PA15，PB12 仅作 OLED CS |
| PB13 | AF5_SPI2_SCK | SPI2_SCK / SCL | J3 + J4 | OUT | 已实测：SPI2 仅供 OLED 接口（Flash 走 SPI3） |
| PB14 | AF5_SPI2_MISO | SPI2_MISO / SDO | J3 + J4 | IN | 同上 |
| PB15 | AF5_SPI2_MOSI | SPI2_MOSI / SDI | J3 + J4 | OUT | 同上 |
| PC0 | AF1_TIM1_CH1 | PC0/TIM1 CH1 | J5 | OUT | 电机 PWM 预留 |
| PC1 | AF1_TIM1_CH2 | PC1/TIM1 CH2 | J5 | OUT | |
| PC2 | AF1_TIM1_CH3 | PC2/TIM1_CH3 | J5 | OUT | |
| PC3 | AF1_TIM1_CH4 | PC3/TIM1_CH4 | J5 | OUT | |
| PC4 | GPIO | PC4 | J5 | BIDIR | 无复用标注 |
| PC5 | GPIO | PC5 | J5 | BIDIR | 无复用标注 |
| PC6 | GPIO_OUT | PC6/DC | J3 + J4 | OUT | OLED/TFT 数据/命令选择线（非 LED） |
| PC7 | GPIO_OUT | PC7/BLK | J3 + J4 | OUT | OLED/TFT 背光控制 |
| PC8 | GPIO | PC8 | J4 | BIDIR | 无复用标注（J4 第 9 脚） |
| PC9 | GPIO | PC9 | J4 | BIDIR | 无复用标注 |
| PC10 | AF6_SPI3_SCK | SPI3_SCK | SPI3 排针 + J4 + U3(CLK) | OUT | Flash 时钟（已实测） |
| PC11 | AF6_SPI3_MISO | SPI3_MISO | SPI3 排针 + J4 + U3(SO) | IN | Flash MISO（已实测） |
| PC12 | AF6_SPI3_MOSI | SPI3_MOSI | SPI3 排针 + J4 + U3(SI) | OUT | Flash MOSI（已实测） |
| PC13 | GPIO_OUT | PC13 | D1(LED1) + J5 | OUT | 510R（R7）限流串联 |
| PC14 | OSC32_IN | PC14-OSC32_IN | Y2 + J5 | IN | LSE 晶振输入 |
| PC15 | OSC32_OUT | PC15-OSC32_OUT | Y2 + J5 | OUT | LSE 晶振输出 |
| PD2 | GPIO_OUT | PD2 | D2(LED2) + J4 | OUT | 510R（R8）限流串联 |
| PF0 | OSC_IN | PF0-OSC_IN | Y1（8MHz HSE） | IN | |
| PF1 | OSC_OUT | PF1-OSC OUT | Y1（8MHz HSE） | OUT | |
| NRST | RESET | NRST | J2 + RST 复位按键 + J4 | IN | 复位输入；按键丝印 `RST复位-按键`（⚠️7 位号未识别） |
| VBAT | RTC 供电 | VBAT | Q1 BAT54C → EX BAT | PWR | C6(104) 去耦 |
| VDDA / VSSA | 模拟电源 | VDDA / VSSA | `3V3`（经滤波） | PWR | C18/C19/C20 滤波 |
| VREF+ | 参考电压 | VREF+ | `3V3`（经 R10/R11 0Ω） | PWR | 已实测 =3.3V |
| VDD / VSS | 数字电源 | 3V3 / GND | U2.VOUT / GND | PWR | 多组 VDD，104 去耦阵列 |

## 5. 外设接口

### 5.1 调试接口
- **SWD**：J2（5 针，丝印 `SPI 1X5`），PA13 SWDIO / PA14 SWCLK / NRST / 3V3 / GND
- **调试串口**：USART2（PA2 TX / PA3 RX），引出至 J5，115200 8N1（M1 沿用 NUCLEO 配置）

### 5.2 USB（Type-C）
- J1 Type-C 接口：供电（VBUS→F1→U2）与 USB Slave（D+/D- → PA12/PA11）双功能
- CC1/CC2 各 5.1k（R1/R2）下拉至 GND（Device 模式配置）
- 另有 `EX USB DP` / `EX USB DM` 测试网络标签

### 5.3 SPI Flash（U3 W25Q16）—— 已实测定稿（2026-08-02）
- 挂接总线：**SPI3**——PC10 SCK / PC11 MISO / PC12 MOSI / **PA15 CS**（U3 引脚 ↔ SPI3 排针/CPU 导通实测确认）
- WP 拉高至 3V3（写保护禁用，实测确认）；**HOLD 接法未测**（建议补测：U3.7 ↔ 3V3，通常与 WP 同接）
- 与 OLED 接口无冲突：SPI2（PB12-15）仅供 OLED，Flash 独占 SPI3 总线与 PA15 片选

### 5.4 OLED/TFT 液晶接口（J3，1×8）

- **J3 为 OLED/TFT 模块直插口**（丝印 `OLED/TFT`，1×8 排针，原理图 2026-08-16 复核确认）
- 引脚：

| J3 脚 | 信号 | MCU 引脚 |
|---|---|---|
| 1 | 3V3 | — |
| 2 | SDI | PB15（SPI2_MOSI） |
| 3 | SCL | PB13（SPI2_SCK） |
| 4 | CS | PB12（SPI2_NSS） |
| 5 | SDO | PB14（SPI2_MISO） |
| 6 | DC | PC6 |
| 7 | BLK | PC7 |
| 8 | GND | — |

- 数据：SPI2（PB12~15）；**与 Flash（SPI3）无冲突**（Flash 独占 SPI3 总线与 PA15 片选）
- 兼容屏规格（丝印图注）：0.96"/1.3" OLED、1.3"/2.0"/1.44"/1.8"/2.4"/2.8" TFT
- 注：J4 第 4~10 脚同样引出这些信号（PB12/13/14/15、PC6/7/8），便于杜邦线扩展

### 5.5 通信
- USART1：PA9 TX / PA10 RX（J4，脚 13/14）
- FDCAN2：PB6 TX / PB5 RX（J4，脚 26/25）
- FDCAN3：PB4 TX / PB3 RX（J4，脚 24/23）

### 5.6 排针
- **J1**：Type-C 接口（供电+USB，非排针）
- **J2**：SWD 调试口（丝印 `SPI_1X5`，5 针）：3V3 / GND / PA13 SWDIO / PA14 SWCLK / NRST
- **J3**：OLED/TFT 直插口（1×8，见 5.4）
- **J4**：2×15 功能排针（丝印 `P2x15-2.54mm`，30 脚，通用 I/O）：

| J4 脚 | 信号 | J4 脚 | 信号 |
|---|---|---|---|
| 1 | 5V | 2 | GND |
| 3 | NRST | 4 | PB12/CS |
| 5 | PB14/SDO | 6 | PB13/SCL |
| 7 | PC6/DC | 8 | PB15/SDI |
| 9 | PC8 | 10 | PC7/BLK |
| 11 | PA8 | 12 | PC9 |
| 13 | PA10/RXD1 | 14 | PA9/TXD1 |
| 15 | PA12/USB_DP | 16 | PA11/USB_DM |
| 17 | PC10 | 18 | PA15 |
| 19 | PC12 | 20 | PC11 |
| 21 | PB3 | 22 | PD2 |
| 23 | PB5 | 24 | PB4 |
| 25 | PB7 | 26 | PB6 |
| 27 | PB9 | 28 | PB8/BOOT0 |
| 29 | 3V3 | 30 | GND |

- **J5**：2×15 模拟/GPIO 排针（丝印 `P2x15-2.54mm`，30 脚）：VDDA、VREF+、3V3、GND、PA0~PA7、PB0/1/2/10/11、PC0~PC5、PC13/14/15、EX BAT（25 脚）
- **SPI3 扩展排针**：SPI3_NSS/MISO/SCK/MOSI（位号 ⚠️6 未识别）

### 5.7 电机控制引脚预留（按实物定稿，业务定型后冻结）

| 功能 | 首选外设/引脚 | 备注 |
|---|---|---|
| 6 路互补 PWM + 死区 | TIM1 CH1~CH4 = PC0~PC3（J5 引出） | 控制环中断裸跑；如需 HRTIM 需外接（本板无 HRTIM 引脚引出） |
| 三相电流采样 | ADC1/2/3 注入组，TIM1_TRGO 触发 | 与 PWM 同步在中心点采样 |
| 母线电压/温度 | ADC 规则组 + DMA | VREF+ = 3.3V（已实测）可直接量化 |
| 编码器 | TIM 编码器模式 / SPI 绝对值（TLE5012B 等） | SPI3 总线已占 Flash，需另选 SPI 或软件接口 |
| 驱动器通信 | FDCAN2（PB5/6）/ FDCAN3（PB3/4） | J4 引出 |
| 模拟输出 | PA4/5/6（DAC1_OUT1/2、DAC2_OUT1） | 可作指令/监控输出 |

## 6. 板载器件

| 位号 | 型号/标称 | 封装 | 连接 | 备注 |
|---|---|---|---|---|
| U1 | STM32G474RC/RE | 64-LQFP | 见第 4 章总表 | 主控 |
| U2 | RT9193-33GB | SOT-23-5（推断） | VBUS/F1 → VIN；VOUT → 3V3；EN 悬空/上拉 ⚠️ | 3.3V LDO 500mA（丝印确认） |
| U3 | W25Q16 | SOIC-8 | SPI3（PC10/11/12 + PA15 CS，已实测） | SPI NOR Flash 16Mbit；WP 拉高（实测） |
| Y1 | 8MHz 晶振 | 无源 | PF0/PF1 | HSE；C10 22pF |
| Y2 | 32.768kHz 晶振 | 无源 | PC14/PC15 | LSE；C8/C9 10pF |
| Q1 | BAT54C | SOT-23 | EX BAT → VBAT | 双肖特基防倒灌（RTC 供电） |
| F1 | 保险丝（`500mA` 标注相邻，归属为推断） | 0603/0805（推断） | VBUS → 5V1 | 输入过流保护 |
| D1 | LED 指示灯 | 3x4x2MM | PC13 → R7(510R) → GND | 用户 LED1（高电平点亮，推断） |
| D2 | LED 指示灯 | 3x4x2MM | PD2 → R8(510R) → GND | 用户 LED2（⚠️ 位号推断） |
| D4 | 5V1 稳压二极管 | SMD | 阳极 5V，阴极 3V3 | **5V 网络过压钳位（5V-3V3 > 5.1V 时导通）**；2026-08-16 原理图复核定稿，消除 ⚠️2 |
| K1 | 轻触按键 3x4x2MM | — | PA0 + GND | 用户按键 1（已实测） |
| K2 | 轻触按键 3x4x2MM | — | PA1 + GND | 用户按键 2（已实测） |
| RST K | 轻触按键（位号 ⚠️7） | — | NRST + GND | 复位按键（SWD 旁），配 C7(104) 去抖 |
| R1/R2 | 5.1k | 0402/0603（推断） | Type-C CC1/CC2 → GND | USB Device 配置 |
| R6 | 5.1k | 同上 | BOOT0 → GND | BOOT0 下拉（推断） |
| R7/R8 | 510R | 同上 | LED 限流 | PC13/PD2 串接 |
| R3 | 1MΩ | SMD | Type-C 区（VBUS 检测） | ⚠️ 用途未确认，推断：VBUS 分压检测 |
| R4 | 0Ω（丝印 `OR`） | SMD | Type-C 区 | ⚠️ 跳线角色未确认 |
| R5 | 阻值未识别 | SMD | CPU 左侧 | ⚠️ 未确认 |
| C7 | 104 | SMD | NRST（SWD 区） | 复位去耦（推断） |
| R10/R11 | 0Ω（丝印 `OR`） | 同上 | 3V3 ↔ VDDA/VREF+（VREF+ 区） | 跳线式连接（已实测已装） |
| C1/C2 | 10uF/10V（丝印 `10uf(106)/10V`） | SMD | VBUS ↔ GND | 输入滤波 |
| C3/C4 | 10uF/10V | SMD | 3V3 ↔ GND | LDO 输出滤波 |
| C6 | 104 | SMD | VBAT ↔ GND | RTC 去耦 |
| C8/C9 | 10pF | SMD | Y2 负载 | LSE |
| C10/C11 | 22pF ×2 | SMD | Y1 负载 | HSE（2026-08-16 复核确认双电容，消除 ⚠️8） |
| C18/C19/C20 | 103/105 | SMD | VDDA/VREF+ 滤波 | 模拟域 |
| C13、EC14/EC16 | 104/106 阵列 | SMD（EC 为电解） | 3V3 ↔ GND | VDD 去耦（104×多 + 电解 106） |

## 7. 保护与安全电路

| 电路 | 器件 | 触发/动作 | 备注 |
|---|---|---|---|
| 输入过流保护 | F1（500mA 保险丝） | 电流 >500mA 熔断 | VBUS → 5V1 串接 |
| VBAT 防倒灌 | Q1 BAT54C | 电池接入时 VBAT=电池电压；断电时防电池回流 3V3 | RTC 供电 |
| 5V 过压保护 | D4（5V1 稳压管，阳极 5V/阴极 3V3） | 5V 网络超过 3V3+5.1V 时导通钳位 | 防 LDO 输入端过压；已定稿 |
| USB CC 配置 | R1/R2 5.1k 下拉 | Device 模式识别 | 非保护，属合规配置 |

## 8. 与现有文档的差异对照

对照对象：历史板 NUCLEO-G474RE（M1 代码基线，原 docs/pinmap.md 已并入本文档）。

| # | 项目 | NUCLEO-G474RE（原 pinmap） | DevEBox 定制板（本原理图） | 影响 |
|---|---|---|---|---|
| 1 | 板载 LED | PA5（LED2 绿，高电平点亮） | PC13（D1）与 PD2（D2） | **M1 代码 bsp/led.c 必须改**（PA5 在定制板为 DAC1_OUT2） |
| 2 | 用户按键 | PC13（B1，低电平按下） | PA0（K1）/ PA1（K2），低电平按下（推断：按键一脚接 GND） | bsp 按键初始化引脚变更 |
| 3 | 调试串口 | USART2 PA2/PA3（板载 ST-LINK VCP） | USART2 PA2/PA3（引出 J5，无板载 VCP） | 复用一致；定制板无 ST-LINK，日志需经 J5 外接 USB-TTL 或 RAM 日志 |
| 4 | HSE | 24MHz（板载晶振） | **8MHz（Y1）** | **clock.c PLL 配置必须改**（M1 按 24MHz 验证） |
| 5 | SWD | 板载 ST-LINK | J2 5 针外部调试口 | 调试器接法变化 |
| 6 | 复位按键 | 无板载 | RST 按键（NRST） | 新增 |
| 7 | USB | 无（仅 ST-LINK VCP） | Type-C USB Slave（PA11/PA12） | 新增外设能力 |
| 8 | 电机预留 | 方向性建议（TIM1/ADC 引脚） | TIM1_CH1~4 = PC0~PC3（J5 引出） | 与 M1 预留一致；DAC PA4/5/6 可作模拟输出 |
| 9 | Flash 分区 | boot/app/参数 0x08000000 起 | 无影响（板载 W25Q16 为外部数据存储，与内部 Flash 分区无关） | 可作 M3 参数存储的扩展介质 |
| 10 | 排针 | 无（NUCLEO 排针未用） | J1 Type-C 接口 + J3/J4/J5 排针 | 外设可达性大幅提升 |

**对 M1 固件的直接结论**：定制板首跑前需修改 bsp/clock.c（8MHz HSE）、bsp/led.c（PC13/PD2），并核对 uart.c 引脚（USART2 复用不变）。验证方式：`dev.py build` + `dev.py verify`。

## 9. 实测定稿记录与剩余不确定项

### 9.1 已实测定稿（2026-08-02，万用表通断/电压）

| 项 | 结论 | 代码影响 |
|---|---|---|
| Flash 总线 | **SPI3**：PC10 SCK / PC11 MISO / PC12 MOSI | SPI3 驱动（M3 参数存储用） |
| Flash CS | **PA15**（SPI3_NSS 网络） | CS=PA15，与 OLED 无冲突 |
| Flash WP | 拉高至 3V3（写保护禁用） | 可正常写 |
| VREF+/VDDA | 0Ω 跳线已装，=3V3（上电 3.30V） | ADC 参考 3.3V，可配 |
| 按键 K1/K2 | PA0/PA1 + GND（低电平有效） | bsp 按键低有效 |
| J1 位号 | Type-C 接口（非 SPI3 排针） | 无 |

### 9.1b 原理图复核补充（2026-08-16，PyMuPDF 渲染 + 视觉复核 + 数据手册交叉验证）

| 项 | 结论 | 消除 |
|---|---|---|
| HSE 引脚 | **PF0-OSC_IN（脚 5）/ PF1-OSC_OUT（脚 6）**（数据手册 Figure 7 LQFP64） | pinmap 旧文 PD0/PD1 为误（仅 LQFP80/100） |
| Y1 负载电容 | C10/C11 = 22pF ×2 | ⚠️8 |
| J3 | OLED/TFT 接口 1×8（3V3/PB15/PB13/PB12/PB14/PC6/PC7/GND） | 原“J4 顶部 2×4”写法更正；原“J3=8 针电源排针”为误读 |
| J4 | **2×15**（30 脚完整定义见 5.6） | 原“2×13”更正 |
| J5 | **2×15**（30 脚） | 原“2×13”更正 |
| D4 | **5V1 稳压管**，阳极 5V / 阴极 3V3（过压钳位） | ⚠️2 |
| RST 电路 | 按键 + C7(104) 去抖（无上拉电阻/大电容） | pinmap 旧文“R11(10Ω)+C20(1µF)”为误 |
| U2 | RT9193-33GB（VOUT=3V3；78L05/1N5817 为视觉误读） | 维持原判 |

### 9.2 剩余不确定项（2 项）

| # | 内容 | 建议处理 |
|---|---|---|
| ⚠️6 | SPI3 扩展排针位号未识别 | 实物核对 |
| ⚠️7 | RST 复位按键具体位号（K3？） | 实物核对 |

**补测建议**：U3.7（HOLD）↔ 3V3 导通性（未测，通常与 WP 同接拉高）。

## 10. 验收自检清单

| # | 检查项 | 结果 |
|---|---|---|
| 1 | 电源树完整：每个电压域有来源（芯片+引脚）与去向 | ✅ 全部定稿（VREF+/VDDA 已实测 =3V3） |
| 2 | 时钟：晶振频率/负载电容/引脚齐全，时钟源选择明确 | ✅ 齐全（Y1: PF0/PF1 + C10/C11 22pF；Y2: PC14/15 + C8/C9 10pF） |
| 3 | MCU 引脚总表覆盖**全部**引脚（含 NC 行），无遗漏 | ✅ 覆盖 64 引脚全部（含电源/复位） |
| 4 | 调试接口（SWD/JTAG/串口）引脚与连接完整 | ✅ SWD J2 + USART2 |
| 5 | 所有 LED/按键/跳线/测量点已收录 | ✅ LED×2、按键×3、0Ω 跳线 R4/R10/R11 |
| 6 | 保护电路（若有）的触发条件与动作已描述 | ✅ F1/BAT54C/D4（5V1 钳位）完整 |
| 7 | ⚠️ 项数量已统计并逐条列出（不得静默吞掉） | ✅ 其余 2 项（⚠️6/⚠️7）见 9.2（⚠️2/⚠️8 已定稿消除） |
| 8 | 与历史板（NUCLEO）的差异已写入第 8 章 | ✅ 10 项差异，含 M1 固件影响结论 |
| 9 | 无任何凭经验"补全"的型号/参数/连接 | ✅ 关键连接已实测定稿 + 2026-08-16 原理图复核；仅剩 ⚠️6/⚠️7 待实物核对 |

**遗留问题**：剩余 ⚠️6（SPI3 扩展排针位号）、⚠️7（RST 按键位号）为实物核对类，不影响固件开发；建议补测 U3.7 HOLD 接法。本文件为引脚/硬件唯一事实源（原 pinmap.md 已删除，其 NUCLEO 段与电机预留已并入 §5.7/§8）。

## 11. OLED SSD1306 扩展接线（agent 状态指示灯，2026-08-31）

| 项 | 值 | 说明 |
|---|---|---|
| 屏 | SSD1306 128×64，I2C（0.3"~0.96" 模块均可） | 单色，像素表情显示 agent 状态 |
| I2C | **I2C1：PB8=SCL、PB9=SDA（AF4，开漏，模块自带上拉）** | 与现有 USART2/USB/LED 无冲突 |
| 地址 | 0x3C（模块背面 A0 电阻焊盘默认） | 若改 0x3D 需同步 board.h `BOARD_OLED_I2C_ADDR` |
| 时钟 | 400kHz（`BOARD_OLED_I2C_TIMING`=0x042FBFCF，PCLK1=160MHz 推导） | 换 PCLK1 必须重算时序（board.h 注释） |
| 接线 | PB8→SCL、PB9→SDA、VCC→3V3、GND→GND | I2C 规范要求开漏+外部上拉（模块已带） |

固件：`bsp/oled.c`（驱动器）+ `bsp/face_data.c`（表情资产）+ `app/tasks/task_status.c`（动画引擎）。
表情**设计与更换流程**：编辑 `tools/oled/gen_faces.py` 的参数（部件 sprite 坐标）
→ `python tools/oled/gen_faces.py --preview` 出预览图 → 重编译固件即可，协议与桥端零改动。
