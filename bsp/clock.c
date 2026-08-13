#include "board.h"

/* ============================================================================
 * 系统时钟（定位模式）：HSI 16MHz 直跑，不启用 PLL/外部晶振
 * 用于隔离定制板硬件问题（HSE 起振失败 / PLL 路径异常）
 * 正常配置见 git 历史（HSE 8MHz 或 HSI-PLL 170MHz 方案）
 * ==========================================================================*/

void SystemClock_Config(void)
{
    /* 保持复位默认：HSI 16MHz、FLASH_LATENCY_0（无需等待周期） */

    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    __HAL_FLASH_DATA_CACHE_ENABLE();
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
