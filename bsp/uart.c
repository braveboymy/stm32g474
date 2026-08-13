#include "board.h"
#include "uart.h"
#include "osal.h"
#include "rb.h"

/* ============================================================================
 * 调试串口驱动（USART2，IT 模式）
 *  TX：环形缓冲 + 逐字节中断发送（115200 下每字节 ~87us，M1 够用；
 *      后期接 CLI 时升级为 DMA + IDLE 检测）
 *  RX：逐字节中断入环形缓冲
 * 并发模型：
 *  - head 只被写入方（任意任务/中断）修改，tail 只在 TxCplt 回调中推进；
 *  - 多写入方由 osal_critical_enter/exit 串行化（BASEPRI 屏蔽 ≤5 级中断）
 * ==========================================================================*/

#define UART_TX_BUF_SIZE 256
#define UART_RX_BUF_SIZE 256

static UART_HandleTypeDef s_uart;
static uint8_t s_tx_buf[UART_TX_BUF_SIZE];
static uint8_t s_rx_buf[UART_RX_BUF_SIZE];
static rb_t s_tx_rb;
static rb_t s_rx_rb;
static volatile bool s_tx_busy;
static bool s_ready;
static uint8_t s_rx_byte; /* 单字节 IT 接收缓冲 */
static uint8_t s_tx_byte; /* 单字节 IT 发送缓冲 */

void uart_init(void)
{
    s_uart.Instance = BOARD_UART;
    s_uart.Init.BaudRate = BOARD_UART_BAUD;
    s_uart.Init.WordLength = UART_WORDLENGTH_8B;
    s_uart.Init.StopBits = UART_STOPBITS_1;
    s_uart.Init.Parity = UART_PARITY_NONE;
    s_uart.Init.Mode = UART_MODE_TX_RX;
    s_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&s_uart) != HAL_OK) {
        Error_Handler();
    }

    rb_init(&s_tx_rb, s_tx_buf, sizeof(s_tx_buf));
    rb_init(&s_rx_rb, s_rx_buf, sizeof(s_rx_buf));
    s_tx_busy = false;
    s_ready = true;

    if (HAL_UART_Receive_IT(&s_uart, &s_rx_byte, 1) != HAL_OK) {
        Error_Handler();
    }
}

/* 从环形缓冲取 1 字节启动发送（必须在临界区内调用） */
static void tx_start_locked(void)
{
    if (rb_peek(&s_tx_rb, &s_tx_byte, 1) == 1) {
        if (HAL_UART_Transmit_IT(&s_uart, &s_tx_byte, 1) == HAL_OK) {
            s_tx_busy = true;
        } else {
            /* HAL 非 READY（BUSY/ERROR）：字节仍在缓冲（peek 未推进 tail），
             * 不置 busy，由下次 uart_write 重试；BUSY 场景下 TxCplt 回调会接手 */
            s_tx_busy = false;
        }
    } else {
        s_tx_busy = false;
    }
}

uint32_t uart_write(const uint8_t* data, uint32_t len)
{
    if (!s_ready || data == NULL || len == 0) {
        return 0;
    }

    uint32_t token = osal_critical_enter();
    uint32_t n = rb_write(&s_tx_rb, data, len);
    if (!s_tx_busy) {
        tx_start_locked();
    }
    osal_critical_exit(token);
    return n;
}

uint32_t uart_read(uint8_t* data, uint32_t len)
{
    if (!s_ready || data == NULL || len == 0) {
        return 0;
    }
    uint32_t token = osal_critical_enter();
    uint32_t n = rb_read(&s_rx_rb, data, len);
    osal_critical_exit(token);
    return n;
}

uint32_t uart_available(void)
{
    return rb_used(&s_rx_rb);
}

void uart_log_output(const char* buf, uint32_t len)
{
    uart_write((const uint8_t*)buf, len);
}

/* ---------------- 中断与 HAL 回调 ---------------- */

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&s_uart);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == BOARD_UART) {
        rb_skip(&s_tx_rb, 1); /* 上一字节已发出 */
        if (rb_used(&s_tx_rb) > 0) {
            rb_peek(&s_tx_rb, &s_tx_byte, 1);
            (void)HAL_UART_Transmit_IT(&s_uart, &s_tx_byte, 1);
        } else {
            s_tx_busy = false;
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == BOARD_UART) {
        rb_write(&s_rx_rb, &s_rx_byte, 1);
        (void)HAL_UART_Receive_IT(&s_uart, &s_rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == BOARD_UART) {
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_ORE | UART_FLAG_NE | UART_FLAG_FE | UART_FLAG_PE);
        (void)HAL_UART_Receive_IT(&s_uart, &s_rx_byte, 1);
    }
}
