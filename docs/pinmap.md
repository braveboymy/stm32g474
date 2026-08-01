# 引脚分配（NUCLEO-G474RE）

## M1 已占用

| 引脚 | 功能 | 说明 |
|---|---|---|
| PA5 | LED2（绿色，高电平点亮） | 出处：ST 官方 BSP stm32g4xx_nucleo.h |
| PA2 / PA3 | USART2 TX / RX（AF7） | 板载 ST-LINK VCP，115200 8N1 |
| PC13 | B1 用户按键（低电平按下） | 未初始化（M1 不用，M4 CLI 时启用 EXTI） |

## 电机控制预留（业务定型后核对原理图再定）

| 功能 | 首选外设/引脚 | 备注 |
|---|---|---|
| 6 路互补 PWM + 死区 | HRTIM 或 TIM1（PA8/PA9/PA10 等） | FOC 中心对齐，COMP 输出直连刹车输入 |
| 三相电流采样 | ADC1/2/3 注入组，TIM1_TRGO 触发 | 与 PWM 同步在中心点采样 |
| 母线电压/温度 | ADC 规则组 + DMA | |
| 编码器 | TIM 编码器模式 / SPI 绝对值（TLE5012B 等） | 抽象为统一位置传感器接口 |
| 驱动器通信 | FDCAN1/2 | 与上位机/主控交互 |

> 预留仅为方向性建议；定制板原理图出来后在本文档冻结最终分配。

---

# 引脚分配（DevEBox STM32G474R 定制板）

> **冻结依据**：docs/hardware-schematic.md（原理图 OCR 提取 + 二次评审 + 2026-08-02 万用表实测定稿）。
> **M1 固件上板前必须改**：bsp/clock.c（HSE 24MHz→8MHz）、bsp/led.c（PA5→PC13/PD2）。

## 板级外设（已实测）

| 功能 | 引脚 | 说明 |
|---|---|---|
| 用户 LED1 | PC13（D1） | 510R 限流，高电平点亮 |
| 用户 LED2 | PD2（D2） | 510R 限流，高电平点亮 |
| 用户按键 K1 | PA0（+GND） | 低电平有效 |
| 用户按键 K2 | PA1（+GND） | 低电平有效 |
| 复位按键 | NRST | SWD 旁，位号未识别 ⚠️ |
| 调试串口 | PA2/PA3 USART2 | 引出 J5，无板载 VCP |
| HSE | PF0/PF1 8MHz | Y1，C10=22pF |
| LSE | PC14/PC15 32.768k | Y2，C8/C9=10pF |
| SWD | PA13/PA14/NRST | J2 5 针（含 3V3/GND） |

## 总线分配（已实测）

| 总线 | 引脚 | 挂接设备 |
|---|---|---|
| SPI2 | PB12 CS / PB13 SCK / PB14 MISO / PB15 MOSI | **仅 OLED/TFT**（J4 顶部直插口，PB12=CS、PC6=DC、PC7=BLK、含 NRST/PC8） |
| SPI3 | PA15 CS / PC10 SCK / PC11 MISO / PC12 MOSI | **W25Q16 Flash**（U3，WP 拉高）+ J4 下部 + SPI3 扩展排针（位号 ⚠️） |
| USART1 | PA9 TX / PA10 RX | J4 |
| FDCAN2 | PB6 TX / PB5 RX | J4 |
| FDCAN3 | PB4 TX / PB3 RX | J4 |
| USB | PA11 DM / PA12 DP | J1 Type-C（CC1/CC2 5.1k 下拉） |
| DAC | PA4 DAC1_OUT1 / PA5 DAC1_OUT2 / PA6 DAC2_OUT1 | J5 |
| TIM1 | PC0~PC3 CH1~CH4 | J5 |

## 排针

| 接口 | 位置 | 内容 |
|---|---|---|
| J1 | Type-C | 供电 + USB（非排针） |
| J2 | SWD | PA13/PA14/NRST/3V3/GND |
| J3 | 8 针电源 | 3V3/GND（丝印 `12345678`）⚠️ |
| J4 | 2×13 功能 | OLED 直插口（NRST/PB12/13/14/15/PC6/7/8）+ USART1 + USB + SPI3 + FDCAN + BOOT0(PB8) + GPIO |
| J5 | 2×13 模拟/GPIO | VDDA/VREF+/PA0~7/PB0/1/2/10/11/PC0~5/PC13/14/15/EX BAT |
| SPI3 排针 | 扩展 | SPI3_NSS/MISO/SCK/MOSI（位号 ⚠️） |

## 电源与参考

| 网络 | 值 | 说明 |
|---|---|---|
| 3V3 | 3.3V | RT9193-33GB（Type-C 5V→F1 500mA→U2） |
| VDDA/VREF+ | =3V3（已实测） | 0Ω 跳线 R10/R11，C18/19/20 滤波 |
| VBAT | 电池 | Q1 BAT54C ← EX BAT |

## 电机预留（按实物定稿）

| 功能 | 引脚 | 备注 |
|---|---|---|
| TIM1 互补 PWM | PC0~PC3（CH1~4，J5 引出） | 控制环中断裸跑；如需 HRTIM 需外接 |
| 三相电流采样 | ADC1/2/3 注入组，TIM1_TRGO 触发 | 与 PWM 同步中心采样 |
| 母线电压/温度 | ADC 规则组 + DMA | VREF+=3.3V 可直接量化 |
| 编码器 | TIM 编码器 / SPI 绝对值 | SPI3 总线已占 Flash，需另选 SPI 或软件 |
| 驱动器通信 | FDCAN2/3 | J4 引出 |
| 模拟输出 | PA4/5/6 DAC | 可作指令/监控输出 |

> 剩余 ⚠️（实物核对类）：D4 角色、SPI3 排针位号、RST 按键位号、Y1 另一侧电容。详见 docs/hardware-schematic.md 9.2。
