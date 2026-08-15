#include "board.h"

/* ============================================================================
 * 系统时钟（对齐 ai_stm32_prj 参考实现）：
 *  - PWR 电压档 SCALE1_BOOST（160MHz 需要）
 *  - HSE 8MHz（Y1，PD0/PD1）+ HSI48（USB 专用）
 *  - PLL：8MHz/1 ×40 = 320MHz VCO → PLLR/2 = 160MHz SYSCLK（FLASH_LATENCY_4）
 *  - PLLQ/8 = 40MHz（未用），USB 时钟 = HSI48
 * ==========================================================================*/

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef per = {0};

    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    __HAL_FLASH_DATA_CACHE_ENABLE();

    /* 主稳压器电压档：SCALE1_BOOST（SYSCLK 160MHz 必需） */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK) {
        Error_Handler();
    }

    /* HSE 8MHz（板载 Y1）+ HSI48（USB）；PLL 源 = HSE */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    osc.HSEState = RCC_HSE_ON;
    osc.HSI48State = RCC_HSI48_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 1U;
    osc.PLL.PLLN = 40U;
    osc.PLL.PLLP = 2U;
    osc.PLL.PLLQ = 8U;
    osc.PLL.PLLR = 2U;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    /* SYSCLK = PLL（160MHz），HCLK/PCLK1/PCLK2 = DIV1 */
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
        Error_Handler();
    }

    /* USB 外设时钟 = HSI48 */
    per.PeriphClockSelection = RCC_PERIPHCLK_USB;
    per.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&per) != HAL_OK) {
        Error_Handler();
    }
}

void SystemClock_GetFreqs(uint32_t* sys, uint32_t* hclk, uint32_t* pclk1, uint32_t* pclk2)
{
    if (sys != NULL) {
        *sys = HAL_RCC_GetSysClockFreq();
    }
    if (hclk != NULL) {
        *hclk = HAL_RCC_GetHCLKFreq();
    }
    if (pclk1 != NULL) {
        *pclk1 = HAL_RCC_GetPCLK1Freq();
    }
    if (pclk2 != NULL) {
        *pclk2 = HAL_RCC_GetPCLK2Freq();
    }
}
