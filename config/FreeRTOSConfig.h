#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ============================================================================
 * STM32G474 平台 FreeRTOS 配置
 * 配套文档：docs/architecture.md
 * 原则：静态分配内核任务 + heap_4 动态分配业务任务
 * ==========================================================================*/

/* ---------------- 基础 ---------------- */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TIME_SLICING                  1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_TICKLESS_IDLE                 0

#define configCPU_CLOCK_HZ                      170000000UL
#define configTICK_RATE_HZ                      1000U
#define configMAX_PRIORITIES                    8
#define configMINIMAL_STACK_SIZE                128
#define configMAX_TASK_NAME_LEN                 12
#define configTOTAL_HEAP_SIZE                   (48UL * 1024UL)
#define configUSE_16_BIT_TICKS                  0

/* ---------------- 内存 ---------------- */
#define configSUPPORT_STATIC_ALLOCATION         1   /* idle/timer 任务静态分配 */
#define configSUPPORT_DYNAMIC_ALLOCATION        1   /* heap_4 */

/* ---------------- 同步原语 ---------------- */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_QUEUE_SETS                    0

/* ---------------- 软件定时器 ---------------- */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               4
#define configTIMER_QUEUE_LENGTH                8
#define configTIMER_TASK_STACK_DEPTH            256

/* ---------------- 调试 / 健壮性 ---------------- */
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* ---------------- 中断优先级（Cortex-M4，4 位，0~15） ----------------
 * 约定：
 *  0~4   硬实时（电机控制环、保护），不调用任何 RTOS API
 *  5     RTOS 可调用中断（UART 等）的临界值
 *  6~14  普通外设中断
 *  15    最低
 */
#define configPRIO_BITS                                 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY         (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* ---------------- 端口特性（Cortex-M4F） ---------------- */
#define configENABLE_FPU                        1
#define configENABLE_MPU                        0
#define configENABLE_TRUSTZONE                  0

/* v11 的向量表安装检查（configCHECK_HANDLER_INSTALLATION）在 GNU 工具链下
 * 必然误报：.word 引用 Thumb 函数时链接器会置位 0（条目 = 符号地址|1），
 * 与符号值比较恒不相等。向量表已通过 bsp/startup 直接指向
 * vPortSVCHandler / xPortPendSVHandler / xPortSysTickHandler（实测验证），
 * 故关闭该检查。 */
#define configCHECK_HANDLER_INSTALLATION       0

/* ---------------- API 裁剪 ---------------- */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskResume                     1
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_pcTaskGetTaskName               1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          1

/* ---------------- 断言 ---------------- */
void vAssertCalled(const char* file, int line);
#define configASSERT(x)    if ((x) == 0) { vAssertCalled(__FILE__, __LINE__); }

#endif /* FREERTOS_CONFIG_H */
