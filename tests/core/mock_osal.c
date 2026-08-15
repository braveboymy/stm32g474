#include "osal.h"

/* ============================================================================
 * osal PC 单测替身（core 层硬件无关验证用）
 * 仅实现 core/log 依赖的最小接口；行为可配置便于断言。
 * ==========================================================================*/

static uint32_t s_tick_ms;
static bool s_scheduler_running;
static bool s_in_isr;

void osal_mock_set_tick_ms(uint32_t tick)
{
    s_tick_ms = tick;
}

void osal_mock_set_scheduler_running(bool running)
{
    s_scheduler_running = running;
}

void osal_mock_set_in_isr(bool in_isr)
{
    s_in_isr = in_isr;
}

/* ---------------- osal 接口实现（mock） ---------------- */

uint32_t osal_tick_ms(void)
{
    return s_tick_ms;
}

bool osal_scheduler_running(void)
{
    return s_scheduler_running;
}

bool osal_in_isr(void)
{
    return s_in_isr;
}

const char* osal_task_current_name(void)
{
    return "test-task";
}

uint32_t osal_critical_enter(void)
{
    return 0U;
}

void osal_critical_exit(uint32_t token)
{
    (void)token;
}

/* 以下接口单测未用到，空实现满足链接 */
void osal_task_delay_ms(uint32_t ms)
{
    (void)ms;
}

void osal_task_delay_until_ms(uint32_t* last_wake_ms, uint32_t period_ms)
{
    (void)last_wake_ms;
    (void)period_ms;
}

uint32_t osal_task_high_water_mark(osal_task_t task)
{
    (void)task;
    return 0U;
}

void osal_mutex_lock(osal_mutex_t m)
{
    (void)m;
}

void osal_mutex_unlock(osal_mutex_t m)
{
    (void)m;
}

uint32_t osal_heap_free(void)
{
    return 0U;
}
