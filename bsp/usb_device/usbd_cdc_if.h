#ifndef __USBD_CDC_IF_H__
#define __USBD_CDC_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_cdc.h"

#include <stdbool.h>

/* CDC 收发缓冲大小（参考工程标准配置） */
#define APP_RX_DATA_SIZE 1024U
#define APP_TX_DATA_SIZE 1024U

/* CDC Interface callback（usb_device.c 注册用） */
extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

/* 标准发送入口（usbd_cdc_if.c） */
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

/* ============================================================================
 * 对外 API（兼容原 bsp/usb_cdc.h 接口，demo 层无感切换）
 * ==========================================================================*/

/* 初始化 USB CDC（MX_USB_Device_Init） */
void usb_cdc_init(void);

/* 发送数据；成功返回发送字节数，busy/未连接返回 0 */
uint32_t usb_cdc_send(const uint8_t* data, uint32_t len);

/* RX 环形缓冲可读字节数 */
uint32_t usb_cdc_available(void);

/* 读取 RX 缓冲；返回实际读取字节数 */
uint32_t usb_cdc_read(uint8_t* data, uint32_t len);

/* 主机是否已配置（枚举完成，可收发） */
bool usb_cdc_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H__ */
