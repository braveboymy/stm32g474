#include <stdint.h>
#include "uart.h"

/* ============================================================================
 * newlib-nano 系统调用重定向
 * 仅实现 _write：printf 系列输出到调试串口（日志模块走独立通道，不依赖本文件）
 * ==========================================================================*/

int _write(int fd, const char* buf, int len)
{
    (void)fd;
    uart_write((const uint8_t*)buf, (uint32_t)len);
    return len;
}
