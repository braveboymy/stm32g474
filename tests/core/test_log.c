#include "test_fw.h"

#include "log.h"
#include "mock_osal.h"
#include "osal.h"

#include <string.h>

/* ============================================================================
 * core/log 单元测试（PC 侧，mock osal）
 * 通过输出回调捕获格式化行，验证：
 *  - 格式：[tick] 级别/标签(任务名): 消息 + CRLF
 *  - 级别过滤（log_set_level）
 *  - RAM 缓冲（log_enable_ram）由 rb 单测覆盖写入语义，此处验证启用不崩溃
 * ==========================================================================*/

static char s_capture[256];
static uint32_t s_capture_len;
static uint32_t s_out_count;

static void capture_output(const char* line, uint32_t len)
{
    s_out_count = s_out_count + 1U;
    if (len < sizeof(s_capture)) {
        (void)memcpy(s_capture, line, (size_t)len);
        s_capture_len = len;
    }
}

TEST_GROUP(test_log);

TEST_CASE(log_format_line)
{
    /* 输出回调 + 任务上下文 */
    log_init(capture_output);
    log_enable_ram();
    log_set_level(LOG_LEVEL_DEBUG);

    s_out_count = 0U;
    osal_mock_set_tick_ms(0x1234U);
    osal_mock_set_scheduler_running(true);

    log_write(LOG_LEVEL_INFO, "demo", "hello %d", 42);

    TEST_ASSERT_EQ(s_out_count, 1U);
    TEST_ASSERT(s_capture_len > 0U);
    /* 格式：[00004660] I/demo(test-task): hello 42\r\n（0x1234 = 十进制 4660） */
    TEST_ASSERT(strstr(s_capture, "[00004660] I/demo(test-task): hello 42") != NULL);
    TEST_ASSERT_EQ(s_capture[s_capture_len - 2U], '\r');
    TEST_ASSERT_EQ(s_capture[s_capture_len - 1U], '\n');
}

TEST_CASE(log_level_filter)
{
    log_init(capture_output);
    log_set_level(LOG_LEVEL_WARN); /* INFO 被过滤 */

    s_out_count = 0U;
    log_write(LOG_LEVEL_DEBUG, "t", "hidden-debug");
    log_write(LOG_LEVEL_INFO, "t", "hidden-info");
    log_write(LOG_LEVEL_WARN, "t", "shown-warn");
    log_write(LOG_LEVEL_ERROR, "t", "shown-error");
    TEST_ASSERT_EQ(s_out_count, 2U);
    TEST_ASSERT(strstr(s_capture, "shown-error") != NULL); /* 捕获的是最后一行 */
}

TEST_CASE(log_boot_context_no_task)
{
    /* 调度器未运行：任务名显示 boot */
    log_init(capture_output);
    log_set_level(LOG_LEVEL_DEBUG);

    s_out_count = 0U;
    osal_mock_set_tick_ms(0U);
    osal_mock_set_scheduler_running(false);

    log_write(LOG_LEVEL_INFO, "sys", "boot start");
    TEST_ASSERT_EQ(s_out_count, 1U);
    TEST_ASSERT(strstr(s_capture, "I/sys(boot): boot start") != NULL);
}

TEST_CASE(log_long_message_truncated)
{
    /* 超长消息截断到缓冲上限，不越界（256 字节缓冲） */
    char big[300];
    uint32_t i;

    log_init(capture_output);
    log_set_level(LOG_LEVEL_DEBUG);
    for (i = 0U; i < sizeof(big); i++) {
        big[i] = 'x';
    }
    big[sizeof(big) - 1U] = '\0';

    s_out_count = 0U;
    log_write(LOG_LEVEL_INFO, "t", "%s", big);
    TEST_ASSERT_EQ(s_out_count, 1U);
    TEST_ASSERT(s_capture_len <= 256U);
}

TEST_CASE(log_ram_enabled_no_crash)
{
    /* RAM 缓冲启用/写入路径不崩溃（dev.py log 读取链路） */
    log_init(capture_output);
    log_enable_ram();
    log_set_level(LOG_LEVEL_DEBUG);
    s_out_count = 0U;

    uint32_t i;
    for (i = 0U; i < 100U; i++) {
        log_write(LOG_LEVEL_INFO, "t", "line %lu", (unsigned long)i);
    }
    TEST_ASSERT_EQ(s_out_count, 100U);
}

int main(void)
{
    printf("== core 单测：log ==\n");
    test_fail_count = 0;

    test_case_log_format_line();
    test_case_log_level_filter();
    test_case_log_boot_context_no_task();
    test_case_log_long_message_truncated();
    test_case_log_ram_enabled_no_crash();

    if (test_fail_count == 0) {
        printf("✅ log: 全部通过\n");
        return 0;
    }
    printf("❌ log: %d 个断言失败\n", test_fail_count);
    return 1;
}
