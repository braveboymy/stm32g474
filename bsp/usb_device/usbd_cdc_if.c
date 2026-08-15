#include "usbd_cdc_if.h"

#include "usb_device.h"
#include "usbd_cdc.h"
#include "usbd_def.h"
#include "usbd_ioreq.h"

#include "rb.h"

#include <stdbool.h>
#include <string.h>

/* ============================================================================
 * USB CDC 虚拟串口（参考 ai_stm32_prj 标准实现移植）
 *  - 枚举为 PC 串口（COM 口）：设备描述符 bDeviceClass=0x02/0x02（CDC ACM）
 *  - RX：CDC_Receive_FS 回调入环形缓冲（usb_cdc_available/read 读取）
 *  - TX：CDC_Transmit_FS 非阻塞，busy 返回 USBD_BUSY（调用方稍后重试）
 *  - 时钟：HSI48（SystemClock_Config + HAL_PCD_MspInit 配置）
 * ==========================================================================*/

#define USB_CDC_RX_RB_SIZE 1024U /* RX 环形缓冲（数据侧） */
/* USBD_static_malloc/free 由 usbd_conf.c 提供 */

/* 接收/发送缓冲（USB 端点 DMA 侧，需 4 字节对齐） */
__ALIGN_BEGIN uint8_t UserRxBufferFS[APP_RX_DATA_SIZE] __ALIGN_END;
__ALIGN_BEGIN uint8_t UserTxBufferFS[APP_TX_DATA_SIZE] __ALIGN_END;

/* RX 环形缓冲（业务侧） */
static uint8_t s_rx_rb_buf[USB_CDC_RX_RB_SIZE];
static rb_t s_rx_rb;

/* ---------------- CDC 类接口（USBD_CDC_ItfTypeDef） ---------------- */

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0U);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    return USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
    return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
    (void)pbuf;
    (void)length;
    switch (cmd) {
    case CDC_SEND_ENCAPSULATED_COMMAND:
    case CDC_GET_ENCAPSULATED_RESPONSE:
    case CDC_SET_COMM_FEATURE:
    case CDC_GET_COMM_FEATURE:
    case CDC_CLEAR_COMM_FEATURE:
    case CDC_SET_LINE_CODING:
    case CDC_GET_LINE_CODING:
    case CDC_SET_CONTROL_LINE_STATE:
    case CDC_SEND_BREAK:
    default:
        break;
    }
    return USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t* Len)
{
    (void)rb_write(&s_rx_rb, Buf, *Len);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

static int8_t CDC_TransmitCplt_FS(uint8_t* Buf, uint32_t* Len, uint8_t epnum)
{
    (void)Buf;
    (void)Len;
    (void)epnum;
    return USBD_OK;
}

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS,
    CDC_TransmitCplt_FS
};

/* ---------------- 标准发送入口 ---------------- */

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
    uint8_t result = USBD_OK;
    USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
    if (hcdc != NULL) {
        if (hcdc->TxState != 0U) {
            return USBD_BUSY;
        }
    }
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
    return result;
}

/* ---------------- 对外 API（兼容原 usb_cdc.h） ---------------- */

void usb_cdc_init(void)
{
    rb_init(&s_rx_rb, s_rx_rb_buf, sizeof(s_rx_rb_buf));
    MX_USB_Device_Init();
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
}

uint32_t usb_cdc_send(const uint8_t* data, uint32_t len)
{
    uint32_t n;

    if (data == NULL || len == 0U) {
        return 0U;
    }
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
        return 0U;
    }
    n = len;
    if (n > (APP_TX_DATA_SIZE - 1U)) {
        n = APP_TX_DATA_SIZE - 1U;
    }
    (void)memcpy(UserTxBufferFS, data, (size_t)n);
    if (CDC_Transmit_FS(UserTxBufferFS, (uint16_t)n) != USBD_OK) {
        return 0U;
    }
    return n;
}

uint32_t usb_cdc_available(void)
{
    return rb_used(&s_rx_rb);
}

uint32_t usb_cdc_read(uint8_t* data, uint32_t len)
{
    return rb_read(&s_rx_rb, data, len);
}

bool usb_cdc_connected(void)
{
    return (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED);
}
