#include "osal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_compiler.h"

/* ============================================================================
 * OSAL 的 FreeRTOS 实现
 * ==========================================================================*/

osal_task_t osal_task_create(const char* name, void (*entry)(void* arg), void* arg,
                             uint32_t stack_words, uint32_t priority)
{
    TaskHandle_t h = NULL;
    if (xTaskCreate((TaskFunction_t)entry, name, stack_words, arg, priority, &h) != pdPASS) {
        return NULL;
    }
    return (osal_task_t)h;
}

void osal_task_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void osal_task_delay_until_ms(uint32_t* last_wake_ms, uint32_t period_ms)
{
    vTaskDelayUntil((TickType_t*)last_wake_ms, pdMS_TO_TICKS(period_ms));
}

uint32_t osal_task_high_water_mark(osal_task_t task)
{
    return (uint32_t)uxTaskGetStackHighWaterMark((TaskHandle_t)task);
}

const char* osal_task_current_name(void)
{
    return pcTaskGetTaskName(NULL);
}

osal_mutex_t osal_mutex_create(void)
{
    return (osal_mutex_t)xSemaphoreCreateMutex();
}

void osal_mutex_lock(osal_mutex_t m)
{
    (void)xSemaphoreTake((SemaphoreHandle_t)m, portMAX_DELAY);
}

void osal_mutex_unlock(osal_mutex_t m)
{
    (void)xSemaphoreGive((SemaphoreHandle_t)m);
}

uint32_t osal_critical_enter(void)
{
    /* 调度器未启动时是单线程环境，无需屏蔽中断；
     * 且 FreeRTOS 的临界区嵌套计数在启动前未初始化（0xaaaaaaaa），
     * 提前调用会导致退出后中断无法恢复。 */
    if (!osal_scheduler_running()) {
        return 0;
    }
    if (osal_in_isr()) {
        return (uint32_t)portSET_INTERRUPT_MASK_FROM_ISR();
    }
    taskENTER_CRITICAL();
    return 1;
}

void osal_critical_exit(uint32_t token)
{
    if (!osal_scheduler_running()) {
        return;
    }
    if (osal_in_isr()) {
        portCLEAR_INTERRUPT_MASK_FROM_ISR((UBaseType_t)token);
    } else {
        taskEXIT_CRITICAL();
    }
}

uint32_t osal_tick_ms(void)
{
    return (uint32_t)xTaskGetTickCount(); /* configTICK_RATE_HZ=1000，即毫秒 */
}

bool osal_in_isr(void)
{
    return __get_IPSR() != 0U;
}

bool osal_scheduler_running(void)
{
    return xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
}

uint32_t osal_heap_free(void)
{
    return (uint32_t)xPortGetFreeHeapSize();
}
