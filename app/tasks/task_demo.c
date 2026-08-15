#include "tasks.h"

#include "led.h"
#include "log.h"
#include "osal.h"
#include "platform.h"
#include "usb_cdc.h"

#include <stdio.h>

/* ============================================================================
 * 基础能力验证任务（DevEBox 定制板 demo）：
 *  - LED1（PC13）500ms 翻转：运行指示（共阳极 3V3，低电平点亮，toggle 无极性依赖）
 *  - LED2（PD2）1s 翻转：调度/心跳指示
 *  - USB CDC 虚拟串口（PA11/PA12）：初始化 + 回显 + 2s 心跳字符串
 * 验证方式：Type-C 接 PC → 出现 COM 口 → 串口助手打开，
 *   发送任意字符应原样返回，并每 2s 收到 "demo-alive <tick>"
 * 引脚出处：docs/pinmap.md（原理图复核：D1→R7→PC13、D2→R8→PD2，低电平点亮）
 * ==========================================================================*/

#define DEMO_ECHO_MAX 64U
#define DEMO_HEARTBEAT_MS 2000U

void task_demo_entry(void* arg)
{
    (void)arg;

    usb_cdc_init();
    LOG_I("demo", "usb cdc init done");

    uint8_t buf[DEMO_ECHO_MAX];
    uint32_t beat = 0U;
    uint32_t last_hb = 0U;
    uint32_t led2_cnt = 0U;

    for (;;) {
        /* LED 闪烁：LED1 快闪（500ms 亮/灭，周期 1s），LED2 慢闪（1s 亮/灭，周期 2s） */
        led1_toggle();
        osal_task_delay_ms(500U);
        led2_cnt = led2_cnt + 1U;
        if ((led2_cnt & 1U) == 0U) {
            led2_toggle();
        }

        /* USB 回显：收到的数据原样返回 */
        if (usb_cdc_available() > 0U) {
            uint32_t n = usb_cdc_read(buf, sizeof(buf));
            if (n > 0U) {
                /* 等上一次发送完成（非阻塞重试，最多 ~100ms） */
                uint32_t retry;
                for (retry = 0U; retry < 20U; retry++) {
                    if (usb_cdc_send(buf, n) == n) {
                        break;
                    }
                    osal_task_delay_ms(5U);
                }
                LOG_I("demo", "echo %lu bytes", (unsigned long)n);
            }
        }

        /* USB 心跳输出（每 2s）：LED 状态 + 运行节拍 */
        uint32_t now = osal_tick_ms();
        if ((now - last_hb) >= DEMO_HEARTBEAT_MS) {
            last_hb = now;
            beat = beat + 1U;
            uint32_t m = (uint32_t)snprintf((char*)buf, sizeof(buf),
                                            "demo-alive %lu (%lu ms)\r\n",
                                            (unsigned long)beat, (unsigned long)now);
            (void)usb_cdc_send(buf, m);
        }
    }
}
