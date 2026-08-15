#include "test_fw.h"

#include "rb.h"

/* ============================================================================
 * rb 环形缓冲单元测试
 * 覆盖：写/读/peek/skip/used/free、环绕、满丢弃、边界（0/1/满容量）
 * ==========================================================================*/

#define RB_TEST_SIZE 16U /* 2 的幂；实际容量 15 */

static uint8_t s_buf[RB_TEST_SIZE];
static uint8_t s_data[64];
static rb_t s_rb;

TEST_GROUP(test_rb);

TEST_CASE(rb_init_reset_state)
{
    rb_init(&s_rb, s_buf, RB_TEST_SIZE);
    TEST_ASSERT_EQ(rb_used(&s_rb), 0U);
    TEST_ASSERT_EQ(rb_free(&s_rb), RB_TEST_SIZE - 1U);
}

TEST_CASE(rb_write_read_basic)
{
    const uint8_t expect[5] = {1U, 2U, 3U, 4U, 5U};

    rb_init(&s_rb, s_buf, RB_TEST_SIZE);
    TEST_ASSERT_EQ(rb_write(&s_rb, expect, 5U), 5U);
    TEST_ASSERT_EQ(rb_used(&s_rb), 5U);

    uint8_t got[5] = {0U};
    TEST_ASSERT_EQ(rb_read(&s_rb, got, 5U), 5U);
    TEST_ASSERT_MEM_EQ(got, expect, 5U);
    TEST_ASSERT_EQ(rb_used(&s_rb), 0U);
}

TEST_CASE(rb_partial_read)
{
    const uint8_t expect[8] = {0xAA, 1U, 2U, 3U, 4U, 5U, 6U, 0xBB};

    rb_init(&s_rb, s_buf, RB_TEST_SIZE);
    (void)rb_write(&s_rb, expect, 8U);
    (void)rb_read(&s_rb, s_data, 3U);
    TEST_ASSERT_EQ(rb_used(&s_rb), 5U);
    TEST_ASSERT_EQ(rb_read(&s_rb, s_data, 99U), 5U); /* 读超出 → 只读剩余 */
    TEST_ASSERT_EQ(s_data[0], expect[3]);
    TEST_ASSERT_EQ(s_data[4], expect[7]);
}

TEST_CASE(rb_ring_wraparound)
{
    /* 写满后读一半，再写：触发 head 环绕 */
    uint8_t payload[8];
    uint32_t i;

    rb_init(&s_rb, s_buf, RB_TEST_SIZE);
    for (i = 0U; i < 8U; i++) {
        payload[i] = (uint8_t)(i * 2U + 1U);
    }
    (void)rb_write(&s_rb, payload, 8U);
    (void)rb_read(&s_rb, s_data, 4U); /* 剩余 4 */

    (void)rb_write(&s_rb, payload, 8U); /* 容量 15，现有 4 → 可写 11，写 8 OK */
    TEST_ASSERT_EQ(rb_used(&s_rb), 12U);

    (void)rb_read(&s_rb, s_data, 12U); /* 全部读回，验证顺序 */
    for (i = 0U; i < 4U; i++) {
        TEST_ASSERT_EQ(s_data[i], payload[i + 4U]);
    }
    for (i = 0U; i < 8U; i++) {
        TEST_ASSERT_EQ(s_data[i + 4U], payload[i]);
    }
    TEST_ASSERT_EQ(rb_used(&s_rb), 0U);
}

TEST_CASE(rb_full_drop_overflow)
{
    rb_init(&s_rb, s_buf, RB_TEST_SIZE);
    TEST_ASSERT_EQ(rb_write(&s_rb, s_data, RB_TEST_SIZE - 1U), RB_TEST_SIZE - 1U); /* 写满 */
    TEST_ASSERT_EQ(rb_used(&s_rb), RB_TEST_SIZE - 1U);
    TEST_ASSERT_EQ(rb_free(&s_rb), 0U);
    TEST_ASSERT_EQ(rb_write(&s_rb, s_data, 3U), 0U); /* 满则丢弃 */
    TEST_ASSERT_EQ(rb_write(&s_rb, s_data, 0U), 0U); /* 零长 */
}

TEST_CASE(rb_peek_no_advance)
{
    const uint8_t expect[4] = {9U, 8U, 7U, 6U};

    rb_init(&s_rb, s_buf, RB_TEST_SIZE);
    (void)rb_write(&s_rb, expect, 4U);
    (void)rb_peek(&s_rb, s_data, 4U);
    TEST_ASSERT_MEM_EQ(s_data, expect, 4U);
    TEST_ASSERT_EQ(rb_used(&s_rb), 4U); /* peek 不推进 */

    rb_skip(&s_rb, 2U);
    TEST_ASSERT_EQ(rb_used(&s_rb), 2U);
    (void)rb_read(&s_rb, s_data, 2U);
    TEST_ASSERT_EQ(s_data[0], expect[2]);
    TEST_ASSERT_EQ(s_data[1], expect[3]);
}

TEST_CASE(rb_empty_read_returns_zero)
{
    rb_init(&s_rb, s_buf, RB_TEST_SIZE);
    TEST_ASSERT_EQ(rb_read(&s_rb, s_data, 8U), 0U);
    TEST_ASSERT_EQ(rb_peek(&s_rb, s_data, 8U), 0U);
}

TEST_CASE(rb_single_byte_roundtrip)
{
    uint8_t byte = 0x5AU;

    rb_init(&s_rb, s_buf, RB_TEST_SIZE);
    (void)rb_write(&s_rb, &byte, 1U);
    byte = 0U;
    TEST_ASSERT_EQ(rb_read(&s_rb, &byte, 1U), 1U);
    TEST_ASSERT_EQ(byte, 0x5AU);
}

int main(void)
{
    printf("== core 单测：rb ==\n");
    test_fail_count = 0;

    test_case_rb_init_reset_state();
    test_case_rb_write_read_basic();
    test_case_rb_partial_read();
    test_case_rb_ring_wraparound();
    test_case_rb_full_drop_overflow();
    test_case_rb_peek_no_advance();
    test_case_rb_empty_read_returns_zero();
    test_case_rb_single_byte_roundtrip();

    if (test_fail_count == 0) {
        printf("✅ rb: 全部通过\n");
        return 0;
    }
    printf("❌ rb: %d 个断言失败\n", test_fail_count);
    return 1;
}
