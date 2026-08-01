#ifndef LOG_H
#define LOG_H

/* ============================================================================
 * 分级日志：任务/中断上下文均可调用。
 * 格式：[tick_ms] 级别/标签(任务名): 消息
 * 输出后端在 log_init 时注册（通常为 uart_log_output），M2 可加 RTT/文件通道。
 * ==========================================================================*/

#include <stdint.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
} log_level_t;

typedef void (*log_output_fn)(const char* buf, uint32_t len);

void log_init(log_output_fn out);
void log_set_level(log_level_t lvl);
void log_write(log_level_t lvl, const char* tag, const char* fmt, ...);

/* 开启 RAM 镜像：日志同时写入内部环形缓冲（devtool.py log 子命令可读取）。
 * 用途：无串口通道时用 J-Link 直接读日志（调试闭环）。 */
void log_enable_ram(void);

#define LOG_D(tag, ...) log_write(LOG_LEVEL_DEBUG, tag, __VA_ARGS__)
#define LOG_I(tag, ...) log_write(LOG_LEVEL_INFO, tag, __VA_ARGS__)
#define LOG_W(tag, ...) log_write(LOG_LEVEL_WARN, tag, __VA_ARGS__)
#define LOG_E(tag, ...) log_write(LOG_LEVEL_ERROR, tag, __VA_ARGS__)

#endif /* LOG_H */
