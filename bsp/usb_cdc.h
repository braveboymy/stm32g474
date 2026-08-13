#ifndef USB_CDC_H
#define USB_CDC_H

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * USB CDC 虚拟串口（DevEBox 定制板 PA11/PA12，Type-C Device）
 *  - 枚举为 PC 串口（COM 口），115200 波特率无效（CDC 是 USB 速率）
 *  - RX：中断回调入环形缓冲（usb_cdc_available/usb_cdc_read 读取）
 *  - TX：非阻塞，busy 时返回 0（调用方稍后重试）
 * 注意：本模块覆盖 HAL_PCD_MspInit 等 weak 符号，必须编入 app 可执行文件
 * ==========================================================================*/

/* 初始化 USB CDC（时钟已由 SystemClock_Config 配好 HSI48+CRS） */
void usb_cdc_init(void);

/* 发送数据；成功返回发送字节数，busy/未连接返回 0 */
uint32_t usb_cdc_send(const uint8_t* data, uint32_t len);

/* RX 环形缓冲可读字节数 */
uint32_t usb_cdc_available(void);

/* 读取 RX 缓冲；返回实际读取字节数 */
uint32_t usb_cdc_read(uint8_t* data, uint32_t len);

/* 主机是否已配置（枚举完成，可收发） */
bool usb_cdc_connected(void);

#endif /* USB_CDC_H */
