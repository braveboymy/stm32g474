#ifndef MOCK_OSAL_H
#define MOCK_OSAL_H

/* ============================================================================
 * osal PC 单测替身配置接口（实现见 mock_osal.c）
 * ==========================================================================*/

#include <stdbool.h>
#include <stdint.h>

void osal_mock_set_tick_ms(uint32_t tick);
void osal_mock_set_scheduler_running(bool running);
void osal_mock_set_in_isr(bool in_isr);

#endif /* MOCK_OSAL_H */
