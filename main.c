#include "FreeRTOS.h"
#include "task.h"

#include "board.h"
#include "log.h"
#include "osal.h"
#include "platform.h"
#include "tasks.h"
#include "uart.h"

/* ============================================================================
 * 应用入口（M1：平台最小系统）
 * 启动顺序：HAL -> 时钟 -> 板级外设 -> 日志 -> 任务 -> 调度器
 * ==========================================================================*/

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    bsp_board_init();

    log_init(uart_log_output);
    LOG_I("sys", "boot start");

    if (osal_task_create("led", task_led_entry, NULL, 128, 1) == NULL) {
        Error_Handler();
    }
    if (osal_task_create("mon", task_sysmon_entry, NULL, 256, 2) == NULL) {
        Error_Handler();
    }

    vTaskStartScheduler();

    /* 不应到达 */
    for (;;) {
    }
}

/* HAL 断言回调（USE_FULL_ASSERT） */
void assert_failed(uint8_t* file, uint32_t line)
{
    LOG_E("sys", "HAL assert: %s:%lu", (char*)file, (unsigned long)line);
    __disable_irq();
    for (;;) {
        __NOP();
    }
}
