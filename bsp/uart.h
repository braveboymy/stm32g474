#ifndef UART_H
#define UART_H

#include <stdint.h>

/* 初始化调试串口（USART2，引脚见 board.h） */
void uart_init(void);

/* 非阻塞发送：入 TX 环形缓冲，中断逐字节发出；返回实际入队字节数 */
uint32_t uart_write(const uint8_t* data, uint32_t len);

/* 非阻塞读取 RX 环形缓冲；返回实际读取字节数 */
uint32_t uart_read(uint8_t* data, uint32_t len);

/* RX 缓冲中可读字节数 */
uint32_t uart_available(void);

/* 日志输出后端（适配 core/log 的 log_output_fn） */
void uart_log_output(const char* buf, uint32_t len);

#endif /* UART_H */
