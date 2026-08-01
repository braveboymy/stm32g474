#include "stm32g4xx.h"

/* ============================================================================
 * 最小 Bootloader（M1 占位，链接在 0x08000000，32KB）
 * 职责：校验 app 向量表合法性后跳转。
 * M6 升级：真正的 OTA 引导（升级协议 + 双区管理 + 回滚）。
 * 分区约定见 docs/flash-partition.md
 * ==========================================================================*/

#define APP_BASE 0x08008000UL

/* app 缺失/非法：停住（M6 将进入升级模式等待固件） */
static void boot_fail(void)
{
    for (;;) {
        __NOP();
    }
}

int main(void)
{
    uint32_t app_sp = *(volatile uint32_t*)APP_BASE;        /* 初始 MSP */
    uint32_t app_pc = *(volatile uint32_t*)(APP_BASE + 4);  /* Reset_Handler */

    /* 合法性校验：SP 落在 SRAM1（0x20000000-0x2001FFFF），PC 落在 Flash 应用区 */
    if ((app_sp & 0xFFF00000UL) != 0x20000000UL) {
        boot_fail();
    }
    if ((app_pc & 0xFFF00000UL) != 0x08000000UL) {
        boot_fail();
    }

    /* 关中断后切换 MSP/VTOR 并跳转（模拟复位环境，app 自行完成初始化） */
    __disable_irq();
    __set_MSP(app_sp);
    SCB->VTOR = APP_BASE;
    ((void (*)(void))app_pc)();

    /* 不应到达 */
    boot_fail();
}
