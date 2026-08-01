#include "tasks.h"
#include "board.h"
#include "log.h"
#include "osal.h"
#include "platform.h"

#include "FreeRTOS.h"
#include "task.h"

/* 系统监控任务：开机横幅 + 周期打印任务表 / 堆水位 */

static char state_char(eTaskState s)
{
    switch (s) {
    case eRunning:
        return 'R';
    case eReady:
        return 'r';
    case eBlocked:
        return 'b';
    case eSuspended:
        return 's';
    case eDeleted:
        return 'd';
    default:
        return '?';
    }
}

void task_sysmon_entry(void* arg)
{
    (void)arg;

    LOG_I("mon", "%s v%s (%s %s)", PLATFORM_NAME, PLATFORM_VERSION, __DATE__, __TIME__);
    LOG_I("mon", "SYSCLK=%lu HCLK=%lu PCLK1=%lu PCLK2=%lu",
          (unsigned long)HAL_RCC_GetSysClockFreq(), (unsigned long)HAL_RCC_GetHCLKFreq(),
          (unsigned long)HAL_RCC_GetPCLK1Freq(), (unsigned long)HAL_RCC_GetPCLK2Freq());
    LOG_I("mon", "heap total=%lu free=%lu",
          (unsigned long)configTOTAL_HEAP_SIZE, (unsigned long)osal_heap_free());

    for (;;) {
        osal_task_delay_ms(5000);

        UBaseType_t n = uxTaskGetNumberOfTasks();
        TaskStatus_t* st = pvPortMalloc(sizeof(TaskStatus_t) * n);
        if (st != NULL) {
            UBaseType_t total = 0;
            if (uxTaskGetSystemState(st, n, &total) == pdPASS) {
                for (UBaseType_t i = 0; i < total; i++) {
                    LOG_I("mon", "task %-10s prio=%u state=%c stack_hw=%lu",
                          st[i].pcTaskName, (unsigned)st[i].uxCurrentPriority,
                          state_char(st[i].eCurrentState),
                          (unsigned long)st[i].usStackHighWaterMark);
                }
            }
            vPortFree(st);
        }
        LOG_I("mon", "heap free=%lu", (unsigned long)osal_heap_free());
    }
}
