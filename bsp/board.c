#include "board.h"
#include "fault.h"
#include "led.h"
#include "uart.h"

void bsp_board_init(void)
{
    led_init();
    uart_init();
}

/* 致命错误（HAL 初始化失败等）：登记故障后停机（IWDG 兜底复位） */
void Error_Handler(void)
{
    fault_freeze(FAULT_ERROR_HANDLER);
}
