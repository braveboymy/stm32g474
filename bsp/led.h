#ifndef LED_H
#define LED_H

#include <stdbool.h>

/* 双 LED（DevEBox 定制板：PC13=D1、PD2=D2，高电平点亮） */
void led_init(void);
void led1_on(void);
void led1_off(void);
void led1_toggle(void);
void led2_on(void);
void led2_off(void);
void led2_toggle(void);

#endif /* LED_H */
