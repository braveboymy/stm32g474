#include "log.h"
#include "osal.h"
#include "rb.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_BUF_SIZE 256
#define LOG_RAM_BUF_SIZE 2048 /* devtool.py 中需与此保持一致 */

static log_output_fn s_out;
static volatile log_level_t s_level = LOG_LEVEL_INFO;
static char s_buf[LOG_BUF_SIZE];
static rb_t s_ram_rb;
static uint8_t s_ram_buf[LOG_RAM_BUF_SIZE];
static volatile bool s_ram_enabled;

static char level_char(log_level_t lvl)
{
    switch (lvl) {
    case LOG_LEVEL_DEBUG:
        return 'D';
    case LOG_LEVEL_INFO:
        return 'I';
    case LOG_LEVEL_WARN:
        return 'W';
    default:
        return 'E';
    }
}

void log_init(log_output_fn out)
{
    s_out = out;
}

void log_enable_ram(void)
{
    rb_init(&s_ram_rb, s_ram_buf, sizeof(s_ram_buf));
    s_ram_enabled = true;
}

void log_set_level(log_level_t lvl)
{
    s_level = lvl;
}

void log_write(log_level_t lvl, const char* tag, const char* fmt, ...)
{
    if ((lvl < s_level) || (s_out == NULL)) {
        return;
    }

    /* 临界区：串行化多任务输出，并保护共享缓冲不被中断抢占 */
    uint32_t token = osal_critical_enter();

    int n = snprintf(s_buf, sizeof(s_buf), "[%08lu] %c/%s(%s): ",
                     (unsigned long)osal_tick_ms(), level_char(lvl), tag,
                     osal_scheduler_running() ? osal_task_current_name() : "boot");
    if (n < 0) {
        n = 0;
    }
    if ((size_t)n < sizeof(s_buf)) {
        va_list ap;
        va_start(ap, fmt);
        int written = vsnprintf(&s_buf[n], (sizeof(s_buf) - (size_t)n), fmt, ap);
        va_end(ap);
        n = n + written;
    }

    if (n > 0) {
        if (n > (int)sizeof(s_buf) - 2) {
            n = (int)sizeof(s_buf) - 2;
        }
        s_buf[n] = '\r';
        n = n + 1;
        s_buf[n] = '\n';
        n = n + 1;
        /* RAM 镜像优先：诊断主通道（dev.py log），不受 uart 后端异常影响 */
        if (s_ram_enabled) {
            rb_write(&s_ram_rb, (const uint8_t*)s_buf, (uint32_t)n);
        }
        if (s_out != NULL) {
            s_out(s_buf, (uint32_t)n);
        }
    }

    osal_critical_exit(token);
}
