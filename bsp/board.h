#ifndef BOARD_H
#define BOARD_H

#include "stm32g4xx_hal.h"

/* ============================================================================
 * DevEBox STM32G474R 定制板板级定义
 * 引脚出处：docs/hardware-schematic.md（原理图 OCR + 万用表实测定稿）
 * ==========================================================================*/

/* 用户 LED：PC13（D1）、PD2（D2）
 * 原理图（2026-08-15 复核）：共阳极接 3V3，阴极经 510Ω 到引脚 → 低电平点亮
 * 出处：docs/STM32G474R开发板-原理图--202508.pdf（D1→R7→PC13、D2→R8→PD2） */
#define BOARD_LED1_PORT  GPIOC
#define BOARD_LED1_PIN   GPIO_PIN_13
#define BOARD_LED2_PORT  GPIOD
#define BOARD_LED2_PIN   GPIO_PIN_2

/* 调试串口 USART2（PA2=TX，PA3=RX，引出 J5，无板载 VCP；115200 8N1） */
#define BOARD_UART         USART2
#define BOARD_UART_BAUD    115200U
#define BOARD_UART_IRQ     USART2_IRQn
#define BOARD_UART_IRQ_PRIO 5U /* 等于 configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY */
#define BOARD_UART_AF      GPIO_AF7_USART2

/* USB Device FS：PA11=DM，PA12=DP（Type-C，CC1/CC2 5.1k 下拉） */
/* PA11=USB_DM、PA12=USB_DP 复用 AF10（HAL 头未定义 GPIO_AF10_USB 宏，数值 0x0A） */
#define BOARD_USB_AF      0x0AU

/* IWDG：LSI≈32kHz 标称，但本板实测 ≈96kHz（约 3 倍标称！）→ /64 → 实测 ~1.5kHz；
 * 重载 4095 → 标称 8.2s / 实测 ~2.7s 超时（保守化，喂狗周期 1s）。
 * 换板/量产必须重新实测 LSI（见 docs/session-summary.md M2 排坑②） */
#define BOARD_IWDG_PRESCALER IWDG_PRESCALER_64
#define BOARD_IWDG_RELOAD    4095U


/* 板级初始化：外设上电、时钟、引脚 */
void bsp_board_init(void);

/* 系统时钟：HSE(8MHz) -> PLL -> SYSCLK 170MHz（bsp/clock.c） */
void SystemClock_Config(void);

/* 时钟频率查询（封装 HAL_RCC_Get*Freq，业务层经 bsp 访问，不直接依赖 HAL） */
void SystemClock_GetFreqs(uint32_t* sys, uint32_t* hclk, uint32_t* pclk1, uint32_t* pclk2);

/* 致命错误处理（HAL Error_Handler）：关中断挂起 */
void Error_Handler(void);

#endif /* BOARD_H */
