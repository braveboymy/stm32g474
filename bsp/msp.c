#include "board.h"
#include "stm32g4xx_hal.h"

/* ============================================================================
 * HAL 底层初始化（MSP）
 * ==========================================================================*/

void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    if (huart->Instance == BOARD_UART) {
        GPIO_InitTypeDef g = {0};

        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        g.Pin = GPIO_PIN_2 | GPIO_PIN_3;
        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        g.Alternate = BOARD_UART_AF;
        HAL_GPIO_Init(GPIOA, &g);

        HAL_NVIC_SetPriority(BOARD_UART_IRQ, BOARD_UART_IRQ_PRIO, 0);
        HAL_NVIC_EnableIRQ(BOARD_UART_IRQ);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* huart)
{
    if (huart->Instance == BOARD_UART) {
        HAL_NVIC_DisableIRQ(BOARD_UART_IRQ);
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
        __HAL_RCC_USART2_CLK_DISABLE();
    }
}
