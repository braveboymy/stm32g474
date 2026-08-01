#ifndef RB_H
#define RB_H

/* ============================================================================
 * 环形缓冲（单生产者单消费者原子；多生产者由调用方加临界区）
 * 注意：size 必须为 2 的幂；容量实际为 size-1
 * ==========================================================================*/

#include <stdint.h>

typedef struct {
    uint8_t* buf;
    uint32_t size;
    volatile uint32_t head; /* 写入位置（仅生产者修改） */
    volatile uint32_t tail; /* 读取位置（仅消费者修改） */
} rb_t;

void rb_init(rb_t* rb, uint8_t* buf, uint32_t size);

uint32_t rb_write(rb_t* rb, const uint8_t* data, uint32_t len); /* 满则丢弃多余，返回写入数 */
uint32_t rb_read(rb_t* rb, uint8_t* data, uint32_t len);        /* 返回读取数 */
uint32_t rb_peek(rb_t* rb, uint8_t* data, uint32_t len);        /* 读取但不推进 tail */
void rb_skip(rb_t* rb, uint32_t len);                           /* 仅推进 tail */
uint32_t rb_used(rb_t* rb);
uint32_t rb_free(rb_t* rb);

#endif /* RB_H */
