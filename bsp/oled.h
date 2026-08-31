#ifndef OLED_H
#define OLED_H

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * SSD1306 128×64 OLED（I2C，见 board.h 引脚/时序定义）
 *
 * 用法（任务上下文调用，I2C 阻塞轮询）：
 *   oled_init();                 // I2C + 面板初始化 + 清屏
 *   oled_blit_frame(frame, x, y); // 16×16 表情帧 → framebuffer（页外列内布局）
 *   oled_draw_text(x, y, str, n); // 6×8 点阵文本（face_data.h FONT6X8）
 *   oled_flush_dirty();          // 只把脏区窗口刷到面板（动画刷新 ~1% 带宽）
 * ==========================================================================*/

#define OLED_W 128U
#define OLED_HEIGHT 64U
#define OLED_FB_BYTES (OLED_W * OLED_HEIGHT / 8U)
#define OLED_FRAME_W 16U
#define OLED_FRAME_H 16U

/* I2C 命令（控制字节 0x00 与数据字节 0x40 由驱动内部处理） */

void oled_init(void);

/* framebuffer 操作（不触发传输） */
void oled_clear(void);
void oled_clear_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void oled_blit_frame(const uint8_t* frame, uint8_t x, uint8_t y); /* 16×16 帧 */
void oled_draw_text(uint8_t x, uint8_t y, const char* text, uint8_t len); /* 6×8 字体 */

/* 把标记的脏区刷到面板；I2C 失败返回 false */
bool oled_flush_dirty(void);

#endif /* OLED_H */