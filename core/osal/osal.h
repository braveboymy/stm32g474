#ifndef OSAL_H
#define OSAL_H

/* ============================================================================
 * OS 抽象层：业务代码只依赖本接口，不直接接触 FreeRTOS。
 * 若将来更换内核（裸机调度器等），只需重写 osal_freertos.c。
 * ==========================================================================*/

#include <stdbool.h>
#include <stdint.h>

typedef void* osal_task_t;
typedef void* osal_mutex_t;

/* 任务：栈大小单位 = 字（4 字节），优先级 0(最低)~7 */
osal_task_t
osal_task_create(const char* name, void (*entry)(void* arg), void* arg, uint32_t stack_words, uint32_t priority);
void osal_task_delay_ms(uint32_t ms);
void osal_task_delay_until_ms(uint32_t* last_wake_ms, uint32_t period_ms);
uint32_t osal_task_high_water_mark(osal_task_t task); /* NULL = 当前任务，单位：字 */
const char* osal_task_current_name(void);

osal_mutex_t osal_mutex_create(void);
void osal_mutex_lock(osal_mutex_t m);
void osal_mutex_unlock(osal_mutex_t m);

/* 临界区：屏蔽优先级 ≤ configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 的中断。
 * 任务/中断上下文均可调用，token 必须成对保存/恢复（可嵌套）。 */
uint32_t osal_critical_enter(void);
void osal_critical_exit(uint32_t token);

uint32_t osal_tick_ms(void);
bool osal_in_isr(void);
bool osal_scheduler_running(void);
uint32_t osal_heap_free(void);

#endif /* OSAL_H */
