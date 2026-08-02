#include "FreeRTOS.h"
#include "task.h"

#include "cmsis_compiler.h"
#include "fault.h"
#include "log.h"

/* ============================================================================
 * FreeRTOS 平台钩子：
 *  - idle/timer 任务静态内存（configSUPPORT_STATIC_ALLOCATION）
 *  - 栈溢出 / malloc 失败 / 断言：登记故障（core/fault）+ fail-fast 停机，
 *    IWDG 兜底复位，复位后 fault_report_previous() 上报
 * ==========================================================================*/

static StaticTask_t s_idle_tcb;
static StackType_t s_idle_stack[configMINIMAL_STACK_SIZE];
static StaticTask_t s_timer_tcb;
static StackType_t s_timer_stack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                   StackType_t** ppxIdleTaskStackBuffer,
                                   uint32_t* pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &s_idle_tcb;
    *ppxIdleTaskStackBuffer = s_idle_stack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer,
                                    StackType_t** ppxTimerTaskStackBuffer,
                                    uint32_t* pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &s_timer_tcb;
    *ppxTimerTaskStackBuffer = s_timer_stack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    (void)xTask;
    LOG_E("rtos", "stack overflow: %s", pcTaskName);
    fault_freeze(FAULT_STACK_OVERFLOW);
}

void vApplicationMallocFailedHook(void)
{
    fault_freeze(FAULT_MALLOC_FAILED);
}

void vAssertCalled(const char* file, int line)
{
    LOG_E("rtos", "assert @ %s:%d", file, line);
    fault_freeze(FAULT_RTOS_ASSERT);
}
