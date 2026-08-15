#include "board.h"
#include "led.h"

/* 双 LED：PC13（D1）、PD2（D2）
 * 原理图：共阳极接 3V3，阴极经 510R（R7/R8）到引脚 → 低电平点亮（on=RESET）
 * 出处：docs/STM32G474R开发板-原理图--202508.pdf，2026-08-15 复核定稿 */

static void led_pin_init(GPIO_TypeDef* port, uint16_t pin)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = pin;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &g);
    /* 初始灭：输出高（共阳极，高=灭） */
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
}

void led_init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    led_pin_init(BOARD_LED1_PORT, BOARD_LED1_PIN);
    led_pin_init(BOARD_LED2_PORT, BOARD_LED2_PIN);
}

void led1_on(void)
{
    HAL_GPIO_WritePin(BOARD_LED1_PORT, BOARD_LED1_PIN, GPIO_PIN_RESET);
}

void led1_off(void)
{
    HAL_GPIO_WritePin(BOARD_LED1_PORT, BOARD_LED1_PIN, GPIO_PIN_SET);
}

void led1_toggle(void)
{
    HAL_GPIO_TogglePin(BOARD_LED1_PORT, BOARD_LED1_PIN);
}

void led2_on(void)
{
    HAL_GPIO_WritePin(BOARD_LED2_PORT, BOARD_LED2_PIN, GPIO_PIN_RESET);
}

void led2_off(void)
{
    HAL_GPIO_WritePin(BOARD_LED2_PORT, BOARD_LED2_PIN, GPIO_PIN_SET);
}

void led2_toggle(void)
{
    HAL_GPIO_TogglePin(BOARD_LED2_PORT, BOARD_LED2_PIN);
}
