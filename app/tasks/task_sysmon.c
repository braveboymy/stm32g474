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

    uint32_t sys_freq = 0U;
    uint32_t hclk_freq = 0U;
    uint32_t pclk1_freq = 0U;
    uint32_t pclk2_freq = 0U;
    SystemClock_GetFreqs(&sys_freq, &hclk_freq, &pclk1_freq, &pclk2_freq);
    LOG_I("mon",
          "SYSCLK=%lu HCLK=%lu PCLK1=%lu PCLK2=%lu",
          (unsigned long)sys_freq,
          (unsigned long)hclk_freq,
          (unsigned long)pclk1_freq,
          (unsigned long)pclk2_freq);
    LOG_I("mon", "heap total=%lu free=%lu", (unsigned long)configTOTAL_HEAP_SIZE, (unsigned long)osal_heap_free());

    for (;;) {
        /* 心跳：每 1s 递增（task_wdg 每 2s 检查，停摆则故障复位）；5 拍后打印一次状态 */
        uint32_t beat;
        for (beat = 0U; beat < 5U; beat++) {
            osal_task_delay_ms(1000U);
            g_sysmon_beat = g_sysmon_beat + 1U;
        }

        UBaseType_t n = uxTaskGetNumberOfTasks();
        TaskStatus_t* st = pvPortMalloc(sizeof(TaskStatus_t) * n);
        if (st != NULL) {
            UBaseType_t total = 0;
            if (uxTaskGetSystemState(st, n, &total) == pdPASS) {
                for (UBaseType_t i = 0; i < total; i++) {
                    LOG_I("mon",
                          "task %-10s prio=%u state=%c stack_hw=%lu",
                          st[i].pcTaskName,
                          (unsigned)st[i].uxCurrentPriority,
                          state_char(st[i].eCurrentState),
                          (unsigned long)st[i].usStackHighWaterMark);
                }
            }
            vPortFree(st);
        }
        LOG_I("mon", "heap free=%lu", (unsigned long)osal_heap_free());
    }
}
