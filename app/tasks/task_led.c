#include "tasks.h"
#include "led.h"
#include "log.h"
#include "osal.h"

/* LED 闪烁任务：验证调度器 + 延时 + GPIO 通路 */
void task_led_entry(void* arg)
{
    (void)arg;
    LOG_I("led", "task started");
    for (;;) {
        led_toggle();
        osal_task_delay_ms(500);
    }
}
