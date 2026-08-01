#ifndef BOARD_H
#define BOARD_H

#include "stm32g4xx_hal.h"

/* ============================================================================
 * NUCLEO-G474RE 板级定义
 * 引脚出处：ST 官方 BSP（stm32g4xx-nucleo-bsp）与板卡手册 UM2870
 * ==========================================================================*/

/* LED2（绿色，高电平点亮） */
#define BOARD_LED_PORT  GPIOA
#define BOARD_LED_PIN   GPIO_PIN_5

/* 调试串口 USART2（PA2=TX，PA3=RX，经 ST-LINK VCP 到 PC） */
#define BOARD_UART         USART2
#define BOARD_UART_BAUD    115200U
#define BOARD_UART_IRQ     USART2_IRQn
#define BOARD_UART_IRQ_PRIO 5U /* 等于 configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY */
#define BOARD_UART_AF      GPIO_AF7_USART2

/* 用户按键 B1（PC13，低电平按下） */
#define BOARD_BTN_PORT  GPIOC
#define BOARD_BTN_PIN   GPIO_PIN_13

/* 板级初始化：外设上电、时钟、引脚 */
void bsp_board_init(void);

/* 系统时钟：HSE(24MHz) -> PLL -> SYSCLK 170MHz（bsp/clock.c） */
void SystemClock_Config(void);

/* 致命错误处理（HAL Error_Handler）：关中断挂起，M2 升级为故障记录 */
void Error_Handler(void);

#endif /* BOARD_H */
