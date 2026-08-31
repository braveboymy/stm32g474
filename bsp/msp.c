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

void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c)
{
    if (hi2c->Instance == BOARD_OLED_I2C) {
        GPIO_InitTypeDef g = {0};

        __HAL_RCC_I2C1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        g.Pin = BOARD_OLED_I2C_SCL_PIN | BOARD_OLED_I2C_SDA_PIN;
        g.Mode = GPIO_MODE_AF_OD; /* I2C 规范要求开漏 */
        g.Pull = GPIO_NOPULL;     /* 上拉依赖模块板载电阻 */
        g.Speed = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = BOARD_OLED_I2C_AF;
        HAL_GPIO_Init(GPIOB, &g);
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* hi2c)
{
    if (hi2c->Instance == BOARD_OLED_I2C) {
        HAL_GPIO_DeInit(BOARD_OLED_I2C_SCL_PORT, BOARD_OLED_I2C_SCL_PIN | BOARD_OLED_I2C_SDA_PIN);
        __HAL_RCC_I2C1_CLK_DISABLE();
    }
}
