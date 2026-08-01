#include "stm32g4xx_hal.h"

/* ============================================================================
 * HAL 时间基准：TIM6（FreeRTOS 占用 SysTick，HAL 改用 TIM6）
 *  - HAL_Init 时以当前 PCLK1(16MHz) 配置
 *  - HAL_RCC_ClockConfig 切换时钟后会自动重算（见 stm32g4xx_hal_rcc.c）
 * ==========================================================================*/

static TIM_HandleTypeDef s_hal_tim;

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

    __HAL_RCC_TIM6_CLK_ENABLE();

    s_hal_tim.Instance = TIM6;
    s_hal_tim.Init.Period = 999U; /* 1MHz 计数 -> 1ms 中断 */
    s_hal_tim.Init.Prescaler = (pclk1 / 1000000U) - 1U;
    s_hal_tim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_hal_tim.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_hal_tim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&s_hal_tim) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_TIM_Base_Start_IT(&s_hal_tim) != HAL_OK) {
        return HAL_ERROR;
    }
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    return HAL_OK;
}

void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&s_hal_tim);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM6) {
        HAL_IncTick();
    }
}
