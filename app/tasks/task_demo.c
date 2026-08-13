#include "tasks.h"

#include "led.h"
#include "log.h"
#include "osal.h"
#include "platform.h"
#include "usb_cdc.h"

#include <stdio.h>

/* ============================================================================
 * 基础能力验证任务（定制板 demo）：
 *  - LED1（PC13）500ms 快闪：运行指示
 *  - LED2（PD2）1s 翻转：调度/心跳指示
 *  - USB CDC：初始化 + 回显（收到什么回什么）+ 2s 心跳字符串
 * 验证方式：PC 打开 COM 口（115200 无所谓，CDC 为 USB 速率），
 * 发送任意字符应原样返回，并周期收到 "demo-alive <tick>"
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

    for (;;) {
        led1_toggle();
        osal_task_delay_ms(500U);
        led2_toggle();

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

        /* USB 心跳输出（每 2s） */
        uint32_t now = osal_tick_ms();
        if ((now - last_hb) >= DEMO_HEARTBEAT_MS) {
            last_hb = now;
            beat = beat + 1U;
            uint32_t m = (uint32_t)snprintf((char*)buf, sizeof(buf),
                                            "demo-alive %lu\r\n", (unsigned long)beat);
            (void)usb_cdc_send(buf, m);
        }
    }
}
