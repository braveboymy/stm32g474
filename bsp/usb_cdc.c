#include "usb_cdc.h"

#include "board.h"
#include "rb.h"

#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usbd_def.h"
#include "usbd_ioreq.h"

#include <string.h>

/* ============================================================================
 * USB CDC 虚拟串口实现（参考 STM32CubeG4 CDC_Standalone 示例精简）
 *  - HAL_PCD_MspInit 等 weak 符号覆盖：必须编入 app 可执行文件（勿移入静态库）
 *  - USB 时钟：HSI48 + CRS（SystemClock_Config 已配置）
 * ==========================================================================*/

#define USB_CDC_RX_BUF_SIZE 512U /* RX 环形缓冲 */
#define USB_CDC_TX_BUF_SIZE 512U /* CDC TX 缓冲（库侧） */
#define USB_CDC_MEM_POOL_SIZE 1024U /* 库静态内存池 */

static uint8_t s_mem_pool[USB_CDC_MEM_POOL_SIZE];
static uint32_t s_mem_used;

/* 库内存分配：简单静态池（仅 USB 库初始化期少量分配） */
void* USBD_static_malloc(uint32_t size)
{
    uint32_t aligned = (size + 3U) & ~3U;
    void* p;
    if ((s_mem_used + aligned) > sizeof(s_mem_pool)) {
        return NULL;
    }
    p = &s_mem_pool[s_mem_used];
    s_mem_used = s_mem_used + aligned;
    return p;
}

void USBD_static_free(void* p)
{
    (void)p; /* 池不回收（USB 库仅初始化期分配，一次性） */
}

static PCD_HandleTypeDef s_hpcd;
static USBD_HandleTypeDef s_hdev;
static uint8_t s_user_rx[USB_CDC_RX_BUF_SIZE];
static uint8_t s_user_tx[USB_CDC_TX_BUF_SIZE];
static uint8_t s_rx_rb_buf[USB_CDC_RX_BUF_SIZE];
static rb_t s_rx_rb;

/* ---------------- HAL_PCD_MspInit（weak 覆盖） ---------------- */

void HAL_PCD_MspInit(PCD_HandleTypeDef* hpcd)
{
    if (hpcd->Instance == USB) {
        GPIO_InitTypeDef g = {0};

        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USB_CLK_ENABLE();

        /* PA11=USB_DM，PA12=USB_DP（AF10） */
        g.Pin = GPIO_PIN_11 | GPIO_PIN_12;
        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = BOARD_USB_AF;
        HAL_GPIO_Init(GPIOA, &g);

        HAL_NVIC_SetPriority(USB_LP_IRQn, 6U, 0U);
        HAL_NVIC_EnableIRQ(USB_LP_IRQn);
    }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* hpcd)
{
    if (hpcd->Instance == USB) {
        __HAL_RCC_USB_CLK_DISABLE();
        HAL_NVIC_DisableIRQ(USB_LP_IRQn);
    }
}

/* ---------------- PCD 回调桥接（weak 覆盖，转到 USBD 栈） ---------------- */

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd)
{
    USBD_LL_SetupStage((USBD_HandleTypeDef*)hpcd->pData, (uint8_t*)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum)
{
    USBD_LL_DataOutStage((USBD_HandleTypeDef*)hpcd->pData, epnum,
                         hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum)
{
    USBD_LL_DataInStage((USBD_HandleTypeDef*)hpcd->pData, epnum,
                        hpcd->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef* hpcd)
{
    USBD_LL_SOF((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef* hpcd)
{
    USBD_SpeedTypeDef speed = USBD_SPEED_FULL;
    if (hpcd->Init.speed != PCD_SPEED_FULL) {
        speed = USBD_SPEED_HIGH;
    }
    USBD_LL_SetSpeed((USBD_HandleTypeDef*)hpcd->pData, speed);
    USBD_LL_Reset((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef* hpcd)
{
    USBD_LL_Suspend((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef* hpcd)
{
    USBD_LL_Resume((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum)
{
    USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum);
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum)
{
    USBD_LL_IsoINIncomplete((USBD_HandleTypeDef*)hpcd->pData, epnum);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef* hpcd)
{
    USBD_LL_DevConnected((USBD_HandleTypeDef*)hpcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef* hpcd)
{
    USBD_LL_DevDisconnected((USBD_HandleTypeDef*)hpcd->pData);
}

void USB_LP_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&s_hpcd);
}

/* ---------------- USBD 底层 LL 桥接（usbd_conf 角色） ---------------- */

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef* pdev)
{
    s_hpcd.pData = pdev;
    pdev->pData = &s_hpcd;

    s_hpcd.Instance = USB;
    s_hpcd.Init.dev_endpoints = 8U;
    s_hpcd.Init.speed = PCD_SPEED_FULL;
    s_hpcd.Init.phy_itface = PCD_PHY_EMBEDDED;
    s_hpcd.Init.Sof_enable = DISABLE;
    s_hpcd.Init.low_power_enable = DISABLE;
    s_hpcd.Init.lpm_enable = DISABLE;
    s_hpcd.Init.battery_charging_enable = DISABLE;

    if (HAL_PCD_Init(&s_hpcd) != HAL_OK) {
        return USBD_FAIL;
    }

    HAL_PCDEx_PMAConfig(&s_hpcd, 0x00U, PCD_SNG_BUF, 0x14U);
    HAL_PCDEx_PMAConfig(&s_hpcd, 0x80U, PCD_SNG_BUF, 0x54U);
    HAL_PCDEx_PMAConfig(&s_hpcd, CDC_IN_EP, PCD_SNG_BUF, 0x94U);
    HAL_PCDEx_PMAConfig(&s_hpcd, CDC_OUT_EP, PCD_SNG_BUF, 0xD4U);
    HAL_PCDEx_PMAConfig(&s_hpcd, CDC_CMD_EP, PCD_SNG_BUF, 0x114U);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef* pdev)
{
    (void)pdev;
    HAL_PCD_DeInit(&s_hpcd);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef* pdev)
{
    (void)pdev;
    HAL_PCD_Start(&s_hpcd);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef* pdev)
{
    (void)pdev;
    HAL_PCD_Stop(&s_hpcd);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr,
                                   uint8_t ep_type, uint16_t ep_mps)
{
    (void)pdev;
    HAL_PCD_EP_Open(&s_hpcd, ep_addr, ep_mps, ep_type);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr)
{
    (void)pdev;
    HAL_PCD_EP_Close(&s_hpcd, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr)
{
    (void)pdev;
    HAL_PCD_EP_Flush(&s_hpcd, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr)
{
    (void)pdev;
    HAL_PCD_EP_SetStall(&s_hpcd, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr)
{
    (void)pdev;
    HAL_PCD_EP_ClrStall(&s_hpcd, ep_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef* pdev, uint8_t dev_addr)
{
    (void)pdev;
    HAL_PCD_SetAddress(&s_hpcd, dev_addr);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef* pdev, uint8_t ep_addr,
                                    uint8_t* pbuf, uint32_t size)
{
    (void)pdev;
    HAL_PCD_EP_Transmit(&s_hpcd, ep_addr, pbuf, size);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef* pdev, uint8_t ep_addr,
                                          uint8_t* pbuf, uint32_t size)
{
    (void)pdev;
    HAL_PCD_EP_Receive(&s_hpcd, ep_addr, pbuf, size);
    return USBD_OK;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef* pdev, uint8_t ep_addr)
{
    (void)pdev;
    return HAL_PCD_EP_GetRxCount(&s_hpcd, ep_addr);
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef* pdev, uint8_t ep_addr)
{
    PCD_HandleTypeDef* hpcd = (PCD_HandleTypeDef*)pdev->pData;
    if ((ep_addr & 0x80U) == 0x80U) {
        return hpcd->IN_ep[ep_addr & 0x7FU].is_stall;
    }
    return hpcd->OUT_ep[ep_addr & 0x7FU].is_stall;
}

void USBD_LL_Delay(uint32_t Delay)
{
    (void)Delay;
    /* 低层延时（仅 USB 复位时序用，本项目不依赖） */
}

/* ---------------- 设备描述符 ---------------- */

#define USBD_VID 0x0483U
#define USBD_PID 0x5741U

static uint8_t s_dev_desc[18U]; /* 标准设备描述符长度 */
static uint8_t s_str_desc[USBD_MAX_STR_DESC_SIZ];

static void usb_get_string(const uint8_t* src, uint16_t* len)
{
    USBD_GetString((uint8_t*)src, s_str_desc, len);
}

static uint8_t* usbd_get_device_desc(USBD_SpeedTypeDef speed, uint16_t* length)
{
    (void)speed;
    static const uint8_t desc[18U] = {
        /* bLength, bDescriptorType, bcdUSB */
        0x12U, USB_DESC_TYPE_DEVICE, 0x00U, 0x02U,
        /* bDeviceClass, bDeviceSubClass, bDeviceProtocol, bMaxPacketSize0 */
        0x00U, 0x00U, 0x00U, 0x40U,
        /* idVendor */
        0x83U, 0x04U,
        /* idProduct */
        0x41U, 0x57U,
        /* bcdDevice */
        0x00U, 0x02U,
        /* iManufacturer, iProduct, iSerialNumber, bNumConfigurations */
        0x01U, 0x02U, 0x03U, 0x01U
    };
    (void)memcpy(s_dev_desc, desc, sizeof(desc));
    *length = sizeof(desc);
    return s_dev_desc;
}

static uint8_t* usbd_get_langid_desc(USBD_SpeedTypeDef speed, uint16_t* length)
{
    (void)speed;
    s_str_desc[0] = 4U;
    s_str_desc[1] = USB_DESC_TYPE_STRING;
    s_str_desc[2] = 0x09U; /* LANGID: English (US) */
    s_str_desc[3] = 0x04U;
    *length = 4U;
    return s_str_desc;
}

static uint8_t* usbd_get_manufacturer_desc(USBD_SpeedTypeDef speed, uint16_t* length)
{
    (void)speed;
    usb_get_string((const uint8_t*)"DevEBox", length);
    return s_str_desc;
}

static uint8_t* usbd_get_product_desc(USBD_SpeedTypeDef speed, uint16_t* length)
{
    (void)speed;
    usb_get_string((const uint8_t*)"STM32G474R Virtual ComPort", length);
    return s_str_desc;
}

static uint8_t* usbd_get_serial_desc(USBD_SpeedTypeDef speed, uint16_t* length)
{
    (void)speed;
    usb_get_string((const uint8_t*)"G474CDC001", length);
    return s_str_desc;
}

static uint8_t* usbd_get_config_str_desc(USBD_SpeedTypeDef speed, uint16_t* length)
{
    (void)speed;
    usb_get_string((const uint8_t*)"CDC Config", length);
    return s_str_desc;
}

static uint8_t* usbd_get_interface_str_desc(USBD_SpeedTypeDef speed, uint16_t* length)
{
    (void)speed;
    usb_get_string((const uint8_t*)"CDC Interface", length);
    return s_str_desc;
}

static USBD_DescriptorsTypeDef s_desc = {
    usbd_get_device_desc,
    usbd_get_langid_desc,
    usbd_get_manufacturer_desc,
    usbd_get_product_desc,
    usbd_get_serial_desc,
    usbd_get_config_str_desc,
    usbd_get_interface_str_desc,
};

/* ---------------- CDC 类接口 ---------------- */

static int8_t cdc_init_fs(void)
{
    USBD_CDC_SetTxBuffer(&s_hdev, s_user_tx, 0U);
    USBD_CDC_SetRxBuffer(&s_hdev, s_user_rx);
    return 0;
}

static int8_t cdc_deinit_fs(void)
{
    return 0;
}

static int8_t cdc_control_fs(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
    (void)cmd;
    (void)pbuf;
    (void)length;
    return 0; /* CDC 控制命令（SET_LINE_CODING 等）直接确认 */
}

static int8_t cdc_receive_fs(uint8_t* buf, uint32_t* len)
{
    (void)rb_write(&s_rx_rb, buf, *len);
    USBD_CDC_SetRxBuffer(&s_hdev, &s_user_rx[0]);
    USBD_CDC_ReceivePacket(&s_hdev);
    return 0;
}

static int8_t cdc_transmit_cplt_fs(uint8_t* buf, uint32_t* len, uint8_t epnum)
{
    (void)buf;
    (void)len;
    (void)epnum;
    return 0;
}

static USBD_CDC_ItfTypeDef s_cdc_fops = {
    cdc_init_fs,
    cdc_deinit_fs,
    cdc_control_fs,
    cdc_receive_fs,
    cdc_transmit_cplt_fs
};

/* ---------------- 对外 API ---------------- */

void usb_cdc_init(void)
{
    rb_init(&s_rx_rb, s_rx_rb_buf, sizeof(s_rx_rb_buf));

    if (USBD_Init(&s_hdev, &s_desc, DEVICE_FS) != USBD_OK) {
        return;
    }
    if (USBD_RegisterClass(&s_hdev, USBD_CDC_CLASS) != USBD_OK) {
        return;
    }
    if (USBD_CDC_RegisterInterface(&s_hdev, &s_cdc_fops) != USBD_OK) {
        return;
    }
    if (USBD_Start(&s_hdev) != USBD_OK) {
        return;
    }

    USBD_CDC_SetRxBuffer(&s_hdev, &s_user_rx[0]);
    USBD_CDC_ReceivePacket(&s_hdev);
}

uint32_t usb_cdc_send(const uint8_t* data, uint32_t len)
{
    USBD_CDC_HandleTypeDef* hcdc;
    uint8_t result;
    uint32_t n;

    if (data == NULL || len == 0U) {
        return 0U;
    }
    if (s_hdev.dev_state != USBD_STATE_CONFIGURED) {
        return 0U;
    }
    hcdc = (USBD_CDC_HandleTypeDef*)s_hdev.pClassData;
    if (hcdc == NULL || hcdc->TxState != 0U) {
        return 0U; /* busy：调用方稍后重试 */
    }
    n = len;
    if (n > (USB_CDC_TX_BUF_SIZE - 1U)) {
        n = USB_CDC_TX_BUF_SIZE - 1U;
    }
    (void)memcpy(s_user_tx, data, (size_t)n);
    USBD_CDC_SetTxBuffer(&s_hdev, s_user_tx, (uint16_t)n);
    result = USBD_CDC_TransmitPacket(&s_hdev);
    if (result != USBD_OK) {
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
    if (data == NULL || len == 0U) {
        return 0U;
    }
    return rb_read(&s_rx_rb, data, len);
}

bool usb_cdc_connected(void)
{
    return s_hdev.dev_state == USBD_STATE_CONFIGURED;
}
