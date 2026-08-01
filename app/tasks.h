#ifndef APP_TASKS_H
#define APP_TASKS_H

/* 应用任务声明（入口统一为 void (*)(void*) 以适配 OSAL） */

void task_led_entry(void* arg);
void task_sysmon_entry(void* arg);

#endif /* APP_TASKS_H */
