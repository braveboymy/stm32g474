#include "fault.h"

#include "log.h"
#include "osal.h"

#include "stm32g474xx.h" /* CMSIS 设备定义：SCB/__get_MSP/__get_PSP 等 */

#include <string.h>

/* ============================================================================
 * core/fault：故障登记 / 现场区管理 / 复位后上报（硬件无关部分）
 *  - 硬件无关：OS 访问仅经 osal（osal_tick_ms / osal_task_current_name），
 *    PC 单测时可替换 osal 实现（core/test）
 *  - 现场记录位于 SRAM .noinit 段：软复位与 IWDG 复位保留，上电复位清除
 *  - 栈快照（M2+）：故障点栈顶 128B 随现场保存，复位后打印调用链候选地址，
 *    用 addr2line 还原函数调用序列（替代 CmBacktrace 的核心能力）
 * ==========================================================================*/

static fault_record_t s_record __attribute__((section(".noinit"), aligned(4)));

/* 防重入：登记过程中再次故障（如日志输出故障）直接放弃，避免递归 */
static volatile uint32_t s_registering;

static const char* const s_id_names[FAULT_ID_COUNT] = {"none",
                                                       "hardfault",
                                                       "memmanage",
                                                       "busfault",
                                                       "usagefault",
                                                       "rtos-assert",
                                                       "stack-overflow",
                                                       "malloc-failed",
                                                       "hal-assert",
                                                       "error-handler",
                                                       "sysmon-stall"};

/* 栈快照采集：从 sp 起拷贝 FAULT_STACK_SNAP_WORDS 字（含边界检查，RAM 外清零） */
void fault_snap_stack(fault_record_t* r, uint32_t sp)
{
    uint32_t i;
    r->stack_snap_words = 0U;
    if ((sp < 0x20000000UL) || (sp > (0x20020000UL - (FAULT_STACK_SNAP_WORDS * 4U)))) {
        return; /* SP 不在 SRAM1+SRAM2 可读范围，放弃快照 */
    }
    for (i = 0U; i < FAULT_STACK_SNAP_WORDS; i++) {
        r->stack_snap[i] = ((const uint32_t*)sp)[i];
    }
    r->stack_snap_words = FAULT_STACK_SNAP_WORDS;
}

static uint32_t fault_checksum(const fault_record_t* r)
{
    /* 从 magic/crc 之后起算，校验和数据区（含栈快照） */
    const uint32_t* w = (const uint32_t*)r;
    uint32_t sum = 0U;
    uint32_t i;
    for (i = 2U; i < (uint32_t)(sizeof(fault_record_t) / 4U); i++) {
        sum = sum + w[i];
    }
    return sum;
}

static void fault_fill_task_name(fault_record_t* r)
{
    uint32_t i;
    if (!osal_scheduler_running()) {
        return; /* 调度器未启动：任务名留空（boot 阶段故障） */
    }
    const char* name = osal_task_current_name();
    if (name == NULL) {
        return;
    }
    for (i = 0U; i < (FAULT_TASK_NAME_LEN - 1U); i++) {
        if (name[i] == '\0') {
            break;
        }
        r->task[i] = name[i];
    }
    r->task[i] = '\0';
}

const char* fault_id_name(fault_id_t id)
{
    if ((uint32_t)id >= (uint32_t)FAULT_ID_COUNT) {
        return "?";
    }
    return s_id_names[(uint32_t)id];
}

void fault_register(const fault_record_t* src)
{
    if (s_registering != 0U) {
        return;
    }
    s_registering = 1U;

    s_record = *src;
    s_record.magic = FAULT_MAGIC;
    s_record.seq = s_record.seq + 1U;
    fault_fill_task_name(&s_record);
    s_record.tick_ms = osal_tick_ms();
    s_record.crc = fault_checksum(&s_record);

    /* 实时日志（尽力而为：log 未初始化/输出失败时静默） */
    const fault_record_t* const r = &s_record;
    LOG_E("fault",
          "crash id=%lu(%s) seq=%lu tick=%lu task=%s",
          (unsigned long)r->id,
          fault_id_name((fault_id_t)r->id),
          (unsigned long)r->seq,
          (unsigned long)r->tick_ms,
          r->task);
    LOG_E("fault",
          "  pc=0x%08lx lr=0x%08lx xpsr=0x%08lx sp=0x%08lx",
          (unsigned long)r->pc,
          (unsigned long)r->lr,
          (unsigned long)r->xpsr,
          (unsigned long)r->sp);
    LOG_E("fault",
          "  r0=0x%08lx r1=0x%08lx r2=0x%08lx r3=0x%08lx r12=0x%08lx",
          (unsigned long)r->r0,
          (unsigned long)r->r1,
          (unsigned long)r->r2,
          (unsigned long)r->r3,
          (unsigned long)r->r12);
    LOG_E("fault",
          "  cfsr=0x%08lx hfsr=0x%08lx dfsr=0x%08lx mmfar=0x%08lx bfar=0x%08lx",
          (unsigned long)r->cfsr,
          (unsigned long)r->hfsr,
          (unsigned long)r->dfsr,
          (unsigned long)r->mmfar,
          (unsigned long)r->bfar);
    LOG_E("fault", "  msp=0x%08lx psp=0x%08lx", (unsigned long)r->msp, (unsigned long)r->psp);

    s_registering = 0U;
}

void fault_freeze(fault_id_t id)
{
    fault_record_t rec;
    (void)memset(&rec, 0, sizeof(rec));

    __asm volatile("mov %0, lr" : "=r"(rec.lr));
    __asm volatile("mov %0, pc" : "=r"(rec.pc));
    if (osal_in_isr()) {
        rec.sp = __get_MSP(); /* ISR/异常上下文：MSP */
    } else {
        rec.sp = __get_PSP(); /* 任务上下文：PSP */
    }
    rec.msp = __get_MSP();
    rec.psp = __get_PSP();
    rec.id = (uint32_t)id;
    fault_snap_stack(&rec, rec.sp);

    fault_register(&rec);

    /* fail-fast：关中断停机，等 IWDG 超时复位（若已启用） */
    __disable_irq();
    for (;;) {
        __NOP();
    }
}

/* 打印栈快照中的调用链候选地址（Flash 区 + Thumb 位，去重）；
 * 地址可用 addr2line 还原：arm-none-eabi-addr2line -e build/bin/app -f <addr> */
static void fault_print_call_chain(const fault_record_t* r)
{
    uint32_t i;
    uint32_t j;
    LOG_E("fault", "  call-chain candidates (addr2line -e build/bin/app -f <addr>):");
    for (i = 0U; i < r->stack_snap_words; i++) {
        uint32_t w = r->stack_snap[i];
        uint32_t addr = w & ~0x1UL; /* 清 Thumb 位 */
        uint32_t dup = 0U;
        if ((addr < 0x08008000UL) || (addr > 0x0807FFFFUL)) {
            continue; /* 非 app Flash 区：不是代码地址 */
        }
        for (j = 0U; j < i; j++) {
            if ((r->stack_snap[j] & ~0x1UL) == addr) {
                dup = 1U;
                break;
            }
        }
        if (dup == 0U) {
            LOG_E("fault", "    call 0x%08lx", (unsigned long)addr);
        }
    }
}

bool fault_report_previous(void)
{
    const uint32_t magic = s_record.magic;
    const uint32_t crc = s_record.crc;

    if ((magic != FAULT_MAGIC) || (fault_checksum(&s_record) != crc)) {
        return false;
    }

    const fault_record_t* const r = &s_record;
    LOG_E("fault", "=== previous crash (seq=%lu) ===", (unsigned long)r->seq);
    LOG_E("fault",
          "  id=%lu(%s) tick=%lu task=%s",
          (unsigned long)r->id,
          fault_id_name((fault_id_t)r->id),
          (unsigned long)r->tick_ms,
          r->task);
    LOG_E("fault",
          "  pc=0x%08lx lr=0x%08lx xpsr=0x%08lx sp=0x%08lx",
          (unsigned long)r->pc,
          (unsigned long)r->lr,
          (unsigned long)r->xpsr,
          (unsigned long)r->sp);
    LOG_E("fault",
          "  r0=0x%08lx r1=0x%08lx r2=0x%08lx r3=0x%08lx r12=0x%08lx",
          (unsigned long)r->r0,
          (unsigned long)r->r1,
          (unsigned long)r->r2,
          (unsigned long)r->r3,
          (unsigned long)r->r12);
    LOG_E("fault",
          "  cfsr=0x%08lx hfsr=0x%08lx dfsr=0x%08lx mmfar=0x%08lx bfar=0x%08lx",
          (unsigned long)r->cfsr,
          (unsigned long)r->hfsr,
          (unsigned long)r->dfsr,
          (unsigned long)r->mmfar,
          (unsigned long)r->bfar);
    LOG_E("fault", "  msp=0x%08lx psp=0x%08lx", (unsigned long)r->msp, (unsigned long)r->psp);
    fault_print_call_chain(r);
    LOG_E("fault", "=== end crash report ===");

    /* 报告后清除，避免下次复位重复上报 */
    s_record.magic = 0U;
    s_record.crc = 0U;
    return true;
}
