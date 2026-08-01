#include "log.h"
#include "osal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_BUF_SIZE 256

static log_output_fn s_out;
static volatile log_level_t s_level = LOG_LEVEL_INFO;
static char s_buf[LOG_BUF_SIZE];

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

void log_set_level(log_level_t lvl)
{
    s_level = lvl;
}

void log_write(log_level_t lvl, const char* tag, const char* fmt, ...)
{
    if (lvl < s_level || s_out == NULL) {
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
        n += vsnprintf(s_buf + n, sizeof(s_buf) - (size_t)n, fmt, ap);
        va_end(ap);
    }

    if (n > 0) {
        if ((size_t)n + 2 > sizeof(s_buf)) {
            n = (int)sizeof(s_buf) - 2;
        }
        s_buf[n++] = '\r';
        s_buf[n++] = '\n';
        s_out(s_buf, (uint32_t)n);
    }

    osal_critical_exit(token);
}
