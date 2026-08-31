#include "FreeRTOS.h"
#include "task.h"

#include "board.h"
#include "fault.h"
#include "log.h"
#include "osal.h"
#include "platform.h"
#include "tasks/tasks.h"
#include "uart.h"

/* ============================================================================
 * 应用入口（M2：+ 故障管理/崩溃上报 + 看门狗）
 * 启动顺序：HAL -> 时钟 -> 板级外设 -> 日志 -> 崩溃上报 -> 任务 -> 调度器
 * ==========================================================================*/

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    bsp_board_init();

    log_init(uart_log_output);
    log_enable_ram();
    fault_report_previous(); /* 上次崩溃现场（IWDG 复位后必走此路径） */
    LOG_I("sys", "boot start");

    if (osal_task_create("demo", task_demo_entry, NULL, 512, 1) == NULL) {
        Error_Handler();
    }
    if (osal_task_create("mon", task_sysmon_entry, NULL, 1024, 2) == NULL) {
        Error_Handler();
    }
    if (osal_task_create("wdg", task_wdg_entry, NULL, 512, 1) == NULL) {
        Error_Handler();
    }
    if (osal_task_create("st", task_status_entry, NULL, 512, 1) == NULL) {
        Error_Handler();
    }

    vTaskStartScheduler();

    /* 不应到达 */
    for (;;) {
    }
}

/* HAL 断言回调（USE_FULL_ASSERT）：登记故障后停机（IWDG 兜底复位） */
void assert_failed(uint8_t* file, uint32_t line)
{
    LOG_E("sys", "HAL assert: %s:%lu", (char*)file, (unsigned long)line);
    fault_freeze(FAULT_HAL_ASSERT);
}
