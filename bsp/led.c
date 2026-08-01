#include "board.h"
#include "led.h"

void led_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin = BOARD_LED_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BOARD_LED_PORT, &g);
    led_off();
}

void led_on(void)
{
    HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_SET);
}

void led_off(void)
{
    HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_RESET);
}

void led_toggle(void)
{
    HAL_GPIO_TogglePin(BOARD_LED_PORT, BOARD_LED_PIN);
}
