#include "FreeRTOS.h"
#include "task.h"
#include "log.h"
#include "cmsis_compiler.h"

/* ============================================================================
 * FreeRTOS 平台钩子：
 *  - idle/timer 任务静态内存（configSUPPORT_STATIC_ALLOCATION）
 *  - 栈溢出 / malloc 失败 / 断言：记录日志后停机（fail-fast）
 * M2 将接入统一故障管理框架（core/fault）
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

static void platform_freeze(const char* why, const char* extra)
{
    LOG_E("rtos", "%s%s%s", why, extra ? ": " : "", extra ? extra : "");
    portDISABLE_INTERRUPTS();
    for (;;) {
        __NOP();
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    (void)xTask;
    platform_freeze("stack overflow", pcTaskName);
}

void vApplicationMallocFailedHook(void)
{
    platform_freeze("malloc failed", NULL);
}

void vAssertCalled(const char* file, int line)
{
    LOG_E("rtos", "assert @ %s:%d", file, line);
    portDISABLE_INTERRUPTS();
    for (;;) {
        __NOP();
    }
}
