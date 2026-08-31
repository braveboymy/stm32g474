#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdint.h>

/* 应用任务声明（入口统一为 void (*)(void*) 以适配 OSAL） */

void task_demo_entry(void* arg);
void task_sysmon_entry(void* arg);
void task_wdg_entry(void* arg);
void task_status_entry(void* arg);

/* sysmon 心跳：task_sysmon 每 1s 递增；task_wdg 每 2s 检查，停摆则故障复位 */
extern volatile uint32_t g_sysmon_beat;

#endif /* APP_TASKS_H */
