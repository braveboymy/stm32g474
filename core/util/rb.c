#include "rb.h"

void rb_init(rb_t* rb, uint8_t* buf, uint32_t size)
{
    rb->buf = buf;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
}

static uint32_t rb_mask(const rb_t* rb)
{
    return rb->size - 1; /* 调用方保证 size 为 2 的幂 */
}

uint32_t rb_used(rb_t* rb)
{
    return (rb->head - rb->tail) & rb_mask(rb);
}

uint32_t rb_free(rb_t* rb)
{
    return rb->size - 1 - rb_used(rb); /* 留一格以区分空/满 */
}

uint32_t rb_write(rb_t* rb, const uint8_t* data, uint32_t len)
{
    uint32_t free = rb_free(rb);
    if (len > free) {
        len = free;
    }
    for (uint32_t i = 0; i < len; i++) {
        rb->buf[rb->head] = data[i];
        rb->head = (rb->head + 1) & rb_mask(rb);
    }
    return len;
}

uint32_t rb_read(rb_t* rb, uint8_t* data, uint32_t len)
{
    uint32_t used = rb_used(rb);
    if (len > used) {
        len = used;
    }
    for (uint32_t i = 0; i < len; i++) {
        data[i] = rb->buf[rb->tail];
        rb->tail = (rb->tail + 1) & rb_mask(rb);
    }
    return len;
}

uint32_t rb_peek(rb_t* rb, uint8_t* data, uint32_t len)
{
    uint32_t used = rb_used(rb);
    if (len > used) {
        len = used;
    }
    uint32_t t = rb->tail;
    for (uint32_t i = 0; i < len; i++) {
        data[i] = rb->buf[t];
        t = (t + 1) & rb_mask(rb);
    }
    return len;
}

void rb_skip(rb_t* rb, uint32_t len)
{
    uint32_t used = rb_used(rb);
    if (len > used) {
        len = used;
    }
    rb->tail = (rb->tail + len) & rb_mask(rb);
}
