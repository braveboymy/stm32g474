#ifndef USBD_CONF_H
#define USBD_CONF_H

/* ============================================================================
 * USB Device Library 配置（usbd_core.c 等 include "usbd_conf.h"）
 * 第三方库配置头，内容为 ST 模板精简版
 * ==========================================================================*/

#include "stm32g4xx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USBD_MAX_NUM_INTERFACES     1U
#define USBD_MAX_NUM_CONFIGURATION  1U
#define USBD_MAX_STR_DESC_SIZ       0x100U
#define USBD_SELF_POWERED           1U
#define USBD_DEBUG_LEVEL            0U

/* CDC Class Config */
#define USBD_SUPPORT_USER_STRING_DESC  1U
#define USBD_CDC_INTERVAL              2000U

/* USBD_Init 的 speed 参数（FS 设备） */
#define DEVICE_FS 0

/* 静态内存分配（usb_cdc.c 实现，避免裸 malloc） */
#define USBD_malloc    (void*)USBD_static_malloc
#define USBD_free      USBD_static_free

void* USBD_static_malloc(uint32_t size);
void  USBD_static_free(void* p);

#endif /* USBD_CONF_H */
