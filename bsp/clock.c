#include "board.h"

/* ============================================================================
 * 系统时钟：HSE(24MHz) -> PLL -> SYSCLK 170MHz
 *   PLLM=6 (24/6=4MHz), PLLN=85 (4*85=340MHz), PLLR=2 (340/2=170MHz)
 *   HCLK=170MHz, APB1=170MHz, APB2=170MHz（电机控制外设 TIM/ADC 用满速）
 * 配置依据：STM32CubeG4 官方示例 + RM0440
 * ==========================================================================*/

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* VOS Range1 Boost：170MHz 运行的电源档位 */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = RCC_PLLM_DIV6;
    osc.PLL.PLLN = 85;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = RCC_PLLQ_DIV2;
    osc.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    /* FLASH_LATENCY_4：170MHz 需要 4 个等待周期（HAL 内部会重算时间基准） */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }

    /* 预取缓冲 + 指令/数据 Cache */
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    __HAL_FLASH_DATA_CACHE_ENABLE();
}
