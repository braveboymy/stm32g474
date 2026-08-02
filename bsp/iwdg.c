#include "iwdg.h"

#include "board.h"

#include "stm32g4xx_hal.h"

/* IWDG 句柄（模块私有状态） */
static IWDG_HandleTypeDef s_iwdg;

void iwdg_init(void)
{
    s_iwdg.Instance = IWDG;
    s_iwdg.Init.Prescaler = BOARD_IWDG_PRESCALER;
    s_iwdg.Init.Reload = BOARD_IWDG_RELOAD;
    s_iwdg.Init.Window = IWDG_WINDOW_DISABLE;
    if (HAL_IWDG_Init(&s_iwdg) != HAL_OK) {
        Error_Handler();
    }
}

void iwdg_feed(void)
{
    (void)HAL_IWDG_Refresh(&s_iwdg);
}
