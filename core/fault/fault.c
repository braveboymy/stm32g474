#include "fault.h"

#include "log.h"

#include "stm32g474xx.h" /* CMSIS 设备定义：SCB/__get_MSP 等 */

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

/* ============================================================================
 * core/fault：故障登记 / 现场区管理 / 复位后上报（硬件无关部分）
 * 现场记录位于 SRAM .noinit 段：软复位与 IWDG 复位保留，上电复位清除
 * ==========================================================================*/

static fault_record_t s_record __attribute__((section(".noinit"), aligned(4)));

/* 防重入：登记过程中再次故障（如日志输出故障）直接放弃，避免递归 */
static volatile uint32_t s_registering;

static const char* const s_id_names[FAULT_ID_COUNT] = {
    "none",
    "hardfault",
    "memmanage",
    "busfault",
    "usagefault",
    "rtos-assert",
    "stack-overflow",
    "malloc-failed",
    "hal-assert",
    "error-handler",
    "sysmon-stall"
};

static uint32_t fault_checksum(const fault_record_t* r)
{
    /* 从 magic/crc 之后起算，校验和数据区 */
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
    const TaskHandle_t h = xTaskGetCurrentTaskHandle();
    if (h == NULL) {
        return; /* 调度器未启动 */
    }
    const char* name = pcTaskGetTaskName(h);
    uint32_t i;
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
    s_record.tick_ms = (uint32_t)xTaskGetTickCount();
    s_record.crc = fault_checksum(&s_record);

    /* 实时日志（尽力而为：log 未初始化/输出失败时静默） */
    const fault_record_t* const r = &s_record;
    LOG_E("fault", "crash id=%lu(%s) seq=%lu tick=%lu task=%s",
          (unsigned long)r->id, fault_id_name((fault_id_t)r->id),
          (unsigned long)r->seq, (unsigned long)r->tick_ms, r->task);
    LOG_E("fault", "  pc=0x%08lx lr=0x%08lx xpsr=0x%08lx sp=0x%08lx",
          (unsigned long)r->pc, (unsigned long)r->lr, (unsigned long)r->xpsr,
          (unsigned long)r->sp);
    LOG_E("fault", "  r0=0x%08lx r1=0x%08lx r2=0x%08lx r3=0x%08lx r12=0x%08lx",
          (unsigned long)r->r0, (unsigned long)r->r1, (unsigned long)r->r2,
          (unsigned long)r->r3, (unsigned long)r->r12);
    LOG_E("fault", "  cfsr=0x%08lx hfsr=0x%08lx dfsr=0x%08lx mmfar=0x%08lx bfar=0x%08lx",
          (unsigned long)r->cfsr, (unsigned long)r->hfsr, (unsigned long)r->dfsr,
          (unsigned long)r->mmfar, (unsigned long)r->bfar);
    LOG_E("fault", "  msp=0x%08lx psp=0x%08lx",
          (unsigned long)r->msp, (unsigned long)r->psp);

    s_registering = 0U;
}

void fault_freeze(fault_id_t id)
{
    fault_record_t rec;
    (void)memset(&rec, 0, sizeof(rec));

    __asm volatile("mov %0, lr" : "=r"(rec.lr));
    __asm volatile("mov %0, pc" : "=r"(rec.pc));
    rec.sp = __get_MSP();
    rec.msp = rec.sp;
    rec.psp = __get_PSP();
    rec.id = (uint32_t)id;

    fault_register(&rec);

    /* fail-fast：关中断停机，等 IWDG 超时复位（若已启用） */
    portDISABLE_INTERRUPTS();
    for (;;) {
        __NOP();
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
    LOG_E("fault", "  id=%lu(%s) tick=%lu task=%s",
          (unsigned long)r->id, fault_id_name((fault_id_t)r->id),
          (unsigned long)r->tick_ms, r->task);
    LOG_E("fault", "  pc=0x%08lx lr=0x%08lx xpsr=0x%08lx sp=0x%08lx",
          (unsigned long)r->pc, (unsigned long)r->lr, (unsigned long)r->xpsr,
          (unsigned long)r->sp);
    LOG_E("fault", "  r0=0x%08lx r1=0x%08lx r2=0x%08lx r3=0x%08lx r12=0x%08lx",
          (unsigned long)r->r0, (unsigned long)r->r1, (unsigned long)r->r2,
          (unsigned long)r->r3, (unsigned long)r->r12);
    LOG_E("fault", "  cfsr=0x%08lx hfsr=0x%08lx dfsr=0x%08lx mmfar=0x%08lx bfar=0x%08lx",
          (unsigned long)r->cfsr, (unsigned long)r->hfsr, (unsigned long)r->dfsr,
          (unsigned long)r->mmfar, (unsigned long)r->bfar);
    LOG_E("fault", "=== end crash report ===");

    /* 报告后清除，避免下次复位重复上报 */
    s_record.magic = 0U;
    s_record.crc = 0U;
    return true;
}
