#include "fault.h"

#include "stm32g474xx.h" /* CMSIS 设备定义：SCB/__get_MSP 等 */

#include <string.h>

/* ============================================================================
 * core/fault：Cortex-M 现场采集（fault_arm）
 *  - HardFault_Handler 覆盖 startup weak 符号（必须编入 app 可执行文件，
 *    与 hal_timebase/msp 等 weak 覆盖文件同一纪律，勿移入静态库）
 *  - 栈帧解码：EXC_RETURN bit2 选 MSP/PSP，bit4 处理 FPU 扩展帧偏移
 * ==========================================================================*/

void HardFault_Handler(void)
{
    uint32_t exc_return = 0U;
    __asm volatile("mov %0, lr" : "=r"(exc_return));

    const uint32_t msp = __get_MSP();
    const uint32_t psp = __get_PSP();

    /* EXC_RETURN bit2=1：任务上下文（PSP）；=0：异常上下文（MSP） */
    const uint32_t sp = ((exc_return & 0x4U) != 0U) ? psp : msp;
    /* 通用寄存器帧（r0-r3/r12/lr/pc/xpsr）始终位于栈帧起点（SP 处）；
     * M4F 若压了 FPU 扩展帧（EXC_RETURN bit4=1），扩展帧在其上方，无需偏移 */
    const uint32_t* const frame = (const uint32_t*)sp;

    fault_record_t rec;
    (void)memset(&rec, 0, sizeof(rec));
    rec.id = (uint32_t)FAULT_HARDFAULT;
    rec.r0 = frame[0];
    rec.r1 = frame[1];
    rec.r2 = frame[2];
    rec.r3 = frame[3];
    rec.r12 = frame[4];
    rec.lr = frame[5];
    rec.pc = frame[6];
    rec.xpsr = frame[7];
    rec.sp = sp;
    rec.msp = msp;
    rec.psp = psp;
    /* 栈快照：故障发生栈的 SP 起 128B（调用链回溯，M2 扩展） */
    fault_snap_stack(&rec, sp);
    rec.cfsr = SCB->CFSR;
    rec.hfsr = SCB->HFSR;
    rec.dfsr = SCB->DFSR;
    rec.mmfar = SCB->MMFAR;
    rec.bfar = SCB->BFAR;

    fault_register(&rec);

    /* 停机，等 IWDG 复位（现场已在固定区） */
    for (;;) {
        __NOP();
    }
}

void fault_self_test(void)
{
    /* UDF：未定义指令，触发 UsageFault → 强制 HardFault（INVSTATE 类） */
    __asm volatile("udf #0");
}
