#include "tasks.h"

#include "fault.h"
#include "iwdg.h"
#include "log.h"
#include "osal.h"

/* ============================================================================
 * 看门狗监控任务（M2 健壮性）
 *  - 初始化 IWDG（3.0s 超时），周期 2s 喂狗
 *  - 监控 sysmon 心跳（每 1s 递增）：心跳停摆 = 任务调度异常 → 故障停机，
 *    由 IWDG 兜底复位，复位后 fault_report_previous() 上报
 *  - 本任务为低优先级（1）：高优先级任务忙等饿死本任务时 IWDG 直接复位
 * ==========================================================================*/

volatile uint32_t g_sysmon_beat;

void task_wdg_entry(void* arg)
{
    (void)arg;

    iwdg_init();
    LOG_I("wdg", "iwdg ok (~2.7s timeout, feed 1s)");

    /* 首拍只记录基线，不判停摆（sysmon 可能尚未启动） */
    uint32_t last_beat = g_sysmon_beat;
    for (;;) {
        osal_task_delay_ms(1000U);

        const uint32_t beat = g_sysmon_beat;
        if (beat == last_beat) {
            LOG_E("wdg", "sysmon heartbeat stall");
            fault_freeze(FAULT_SYSMON_STALL); /* 不复返 */
        }
        last_beat = beat;

        iwdg_feed();
    }
}
