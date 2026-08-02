#ifndef FAULT_H
#define FAULT_H

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * core/fault：统一故障管理框架（M2 健壮性）
 *
 * 职责：
 *  - 故障登记：现场（PC/LR/寄存器/CFSR）+ 任务名 + tick 写入 SRAM 固定区
 *    （.noinit 段，软复位/IWDG 复位不清除）
 *  - fail-fast：登记后关中断停机，IWDG 超时兜底复位
 *  - 复位后 fault_report_previous() 上报上次崩溃现场
 *
 * 分层：本模块硬件无关（PC 可单测）；Cortex-M 现场采集在 fault_arm.c；
 * 看门狗外设在 bsp/iwdg.c。
 * ==========================================================================*/

typedef enum {
    FAULT_NONE = 0,
    FAULT_HARDFAULT,
    FAULT_MEM_MANAGE,
    FAULT_BUS_FAULT,
    FAULT_USAGE_FAULT,
    FAULT_RTOS_ASSERT,
    FAULT_STACK_OVERFLOW,
    FAULT_MALLOC_FAILED,
    FAULT_HAL_ASSERT,
    FAULT_ERROR_HANDLER,
    FAULT_SYSMON_STALL,
    FAULT_ID_COUNT
} fault_id_t;

#define FAULT_TASK_NAME_LEN 12U
#define FAULT_MAGIC         0xF41C7E11UL /* "Fault" */

/* 崩溃现场记录（固定布局，跨复位保留） */
typedef struct {
    uint32_t magic;   /* FAULT_MAGIC，有效标志 */
    uint32_t crc;     /* 数据区校验和（offset 8 起） */
    uint32_t seq;     /* 崩溃序号（自增） */
    uint32_t id;      /* fault_id_t */
    uint32_t tick_ms; /* 崩溃时系统 tick */
    char     task[FAULT_TASK_NAME_LEN];
    uint32_t pc;   /* 故障点 PC */
    uint32_t lr;   /* 故障点 LR（HardFault 时为栈帧 LR，freeze 时为调用者） */
    uint32_t xpsr; /* xPSR（HardFault 栈帧） */
    uint32_t sp;   /* 故障时栈指针（HardFault 为发生栈的 SP） */
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t cfsr;  /* SCB->CFSR（HardFault 有效） */
    uint32_t hfsr;  /* SCB->HFSR */
    uint32_t dfsr;  /* SCB->DFSR */
    uint32_t mmfar; /* SCB->MMFAR */
    uint32_t bfar;  /* SCB->BFAR */
    uint32_t msp;   /* 现场 MSP/PSP */
    uint32_t psp;
} fault_record_t;

/* 登记故障现场（写固定区 + 实时日志）。src 可为栈上临时记录；
 * 任务名与 tick 由本模块自动补齐。可在中断/任务/调度器启动前调用。 */
void fault_register(const fault_record_t* src);

/* fail-fast 停机：采集当前 PC/LR/SP 登记后关中断死循环（由 IWDG 兜底复位） */
void fault_freeze(fault_id_t id);

/* 复位后调用：固定区有有效记录则打印崩溃报告并清除；返回是否有报告 */
bool fault_report_previous(void);

/* 故障 id 短名（日志用） */
const char* fault_id_name(fault_id_t id);

/* 自测：执行 UDF 指令制造 HardFault（验证崩溃采集/上报链路，生产勿用） */
void fault_self_test(void);

#endif /* FAULT_H */
