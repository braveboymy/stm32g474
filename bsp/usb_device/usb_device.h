#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx.h"
#include "stm32g4xx_hal.h"
#include "usbd_def.h"

/* USB Device Core handle（参考工程结构，供 usbd_cdc_if/usbd_conf 使用） */
extern USBD_HandleTypeDef hUsbDeviceFS;

/* USB Device 初始化：USBD_Init + RegisterClass + Start */
void MX_USB_Device_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_DEVICE_H */
