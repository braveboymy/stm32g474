#include "board.h"
#include "led.h"
#include "uart.h"

void bsp_board_init(void)
{
    led_init();
    uart_init();
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
        __NOP();
    }
}
