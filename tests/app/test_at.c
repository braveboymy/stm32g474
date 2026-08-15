#include "at.h"

#include "test_fw.h"

#include <string.h>

/* ============================================================================
 * at_ 线协议引擎 PC 单测（host gcc，无 RTOS/HAL）
 * 覆盖：最终码判定、URC 分发（含命令在途）、期望前缀、超时、回显剥离、
 *       队列顺序/满、透明模式暂停/恢复、看门狗复位、行溢出、终结符变体、大小写
 * ==========================================================================*/

static at_engine_t s_eng;
static uint8_t s_txbuf[512];
static uint32_t s_txlen;
static uint32_t s_reset_calls;

/* ---- 测试桩 ---- */

static uint32_t tx_mock(const uint8_t* data, uint32_t len, void* ctx)
{
    (void)ctx;
    if ((s_txlen + len) <= sizeof(s_txbuf)) {
        (void)memcpy(&s_txbuf[s_txlen], data, len);
        s_txlen = s_txlen + len;
    }
    return len;
}

static void reset_hook(at_engine_t* h, void* ctx)
{
    (void)h;
    (void)ctx;
    s_reset_calls = s_reset_calls + 1U;
}

static void feed_str(at_engine_t* h, const char* s)
{
    at_feed(h, (const uint8_t*)s, (uint32_t)strlen(s));
}

static at_cfg_t make_cfg(void)
{
    at_cfg_t cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.tx = tx_mock;
    cfg.term = AT_TERM_CRLF;
    cfg.echo = false;
    cfg.case_insensitive = false;
    cfg.line_max = 64U;
    cfg.resp_max = 256U;
    cfg.queue_max = 4U;
    cfg.urc_max = 4U;
    cfg.wdg_interval_ms = 0U;
    cfg.wdg_fail_max = 0U;
    cfg.reset = NULL;
    return cfg;
}

/* ---- 捕获回调 ---- */

static at_done_reason_t s_reason;
static char s_resp[256];
static uint32_t s_resp_len;
static uint32_t s_done_count;
static uint32_t s_urc_count;
static char s_urc_line[128];
static uint32_t s_urc_len;

static void done_cb(at_engine_t* h, at_done_reason_t reason, const char* resp, uint32_t resp_len, void* ctx)
{
    (void)h;
    (void)ctx;
    s_reason = reason;
    s_done_count = s_done_count + 1U;
    s_resp_len = resp_len;
    if (resp_len < sizeof(s_resp)) {
        (void)memcpy(s_resp, resp, resp_len);
        s_resp[resp_len] = '\0';
    }
}

static void urc_cb(at_engine_t* h, const char* line, uint32_t len, void* ctx)
{
    (void)h;
    (void)ctx;
    s_urc_count = s_urc_count + 1U;
    s_urc_len = len;
    if (len < sizeof(s_urc_line)) {
        (void)memcpy(s_urc_line, line, len);
        s_urc_line[len] = '\0';
    }
}

static void data_cb(at_engine_t* h, const uint8_t* data, uint32_t len, void* ctx)
{
    (void)h;
    (void)ctx;
    s_urc_count = s_urc_count + 1U; /* 复用计数：透明数据到达 */
    s_urc_len = len;
    if (len < sizeof(s_urc_line)) {
        (void)memcpy(s_urc_line, data, len);
        s_urc_line[len] = '\0';
    }
}

static void reset_capture(void)
{
    s_reason = AT_DONE_OK;
    s_resp_len = 0U;
    s_resp[0] = '\0';
    s_done_count = 0U;
    s_urc_count = 0U;
    s_urc_len = 0U;
    s_urc_line[0] = '\0';
    s_txlen = 0U;
}

static void open_engine(at_cfg_t* cfg)
{
    TEST_ASSERT_EQ(at_open(&s_eng, cfg), AT_OK);
    reset_capture();
}

TEST_GROUP(test_at);

/* ---- 用例 ---- */

TEST_CASE(at_basic_ok)
{
    at_cfg_t cfg = make_cfg();

    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT", NULL, 1000U, done_cb, NULL), AT_OK);
    TEST_ASSERT_EQ(s_txlen, 4U); /* "AT\r\n" */
    TEST_ASSERT_MEM_EQ(s_txbuf, "AT\r\n", 4U);

    feed_str(&s_eng, "OK\r\n");
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_OK);
    TEST_ASSERT_EQ(s_resp_len, 3U); /* "OK\n" */
    TEST_ASSERT_MEM_EQ(s_resp, "OK\n", 3U);
    TEST_ASSERT(!at_busy(&s_eng));
}

TEST_CASE(at_cme_error)
{
    at_cfg_t cfg = make_cfg();

    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT+FOO", NULL, 1000U, done_cb, NULL), AT_OK);
    feed_str(&s_eng, "+CME ERROR: 100\r\n");
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_ERROR);
    TEST_ASSERT(strstr(s_resp, "+CME ERROR: 100") != NULL);
}

TEST_CASE(at_urc_in_idle)
{
    at_cfg_t cfg = make_cfg();

    open_engine(&cfg);
    TEST_ASSERT_EQ(at_register_urc(&s_eng, "+NSONMI", urc_cb, NULL), AT_OK);
    feed_str(&s_eng, "+NSONMI: 5\r\n");
    TEST_ASSERT_EQ(s_urc_count, 1U);
    TEST_ASSERT(strcmp(s_urc_line, "+NSONMI: 5") == 0);
    TEST_ASSERT(!at_busy(&s_eng));
}

TEST_CASE(at_expect_beats_urc)
{
    at_cfg_t cfg = make_cfg();

    open_engine(&cfg);
    TEST_ASSERT_EQ(at_register_urc(&s_eng, "+CEREG", urc_cb, NULL), AT_OK);
    TEST_ASSERT_EQ(at_register_urc(&s_eng, "+NSONMI", urc_cb, NULL), AT_OK);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT+CEREG?", "+CEREG", 1000U, done_cb, NULL), AT_OK);

    /* 查询响应行不被 URC 表抢走 */
    feed_str(&s_eng, "+CEREG: 0,1\r\n");
    TEST_ASSERT_EQ(s_urc_count, 0U);

    /* 真实 URC 在命令在途时照常分发 */
    feed_str(&s_eng, "+NSONMI: 3\r\n");
    TEST_ASSERT_EQ(s_urc_count, 1U);
    TEST_ASSERT(strcmp(s_urc_line, "+NSONMI: 3") == 0);

    feed_str(&s_eng, "OK\r\n");
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_OK);
    TEST_ASSERT(strcmp(s_resp, "+CEREG: 0,1\nOK\n") == 0);
}

TEST_CASE(at_timeout)
{
    at_cfg_t cfg = make_cfg();

    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT", NULL, 1000U, done_cb, NULL), AT_OK);

    at_poll(&s_eng, 500U);
    TEST_ASSERT_EQ(s_done_count, 0U); /* 未到超时 */

    at_poll(&s_eng, 2000U);
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_TIMEOUT);
}

TEST_CASE(at_echo_strip)
{
    at_cfg_t cfg = make_cfg();

    cfg.echo = true;
    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT+CSQ", NULL, 1000U, done_cb, NULL), AT_OK);

    feed_str(&s_eng, "AT+CSQ\r\n"); /* 回显 */
    feed_str(&s_eng, "+CSQ: 15,99\r\n");
    feed_str(&s_eng, "OK\r\n");
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_OK);
    TEST_ASSERT(strcmp(s_resp, "+CSQ: 15,99\nOK\n") == 0); /* 无回显行 */
}

TEST_CASE(at_queue_order_and_full)
{
    at_cfg_t cfg = make_cfg();

    cfg.queue_max = 2U;
    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT+A", NULL, 1000U, done_cb, NULL), AT_OK);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT+B", NULL, 1000U, done_cb, NULL), AT_OK);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT+C", NULL, 1000U, done_cb, NULL), AT_ERR_FULL);

    /* 完成第一条 → 第二条自动发出 */
    feed_str(&s_eng, "OK\r\n");
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT(strstr((const char*)s_txbuf, "AT+B") != NULL);

    feed_str(&s_eng, "OK\r\n");
    TEST_ASSERT_EQ(s_done_count, 2U);
    TEST_ASSERT(!at_busy(&s_eng));
}

TEST_CASE(at_pause_resume)
{
    at_cfg_t cfg = make_cfg();

    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT+GO", NULL, 1000U, done_cb, NULL), AT_OK);

    at_pause(&s_eng);
    TEST_ASSERT_EQ(s_done_count, 1U); /* 在途命令以 PAUSED 结束 */
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_PAUSED);

    TEST_ASSERT_EQ(at_send(&s_eng, "AT+X", NULL, 1000U, done_cb, NULL), AT_ERR_BUSY);

    /* 透明模式：字节原样交付数据回调 */
    at_set_data_cb(&s_eng, data_cb, NULL);
    feed_str(&s_eng, "\x01\x02\x03");
    TEST_ASSERT_EQ(s_urc_count, 1U);
    TEST_ASSERT_EQ(s_urc_len, 3U);
    TEST_ASSERT_MEM_EQ(s_urc_line, "\x01\x02\x03", 3U);

    at_resume(&s_eng);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT+Y", NULL, 1000U, done_cb, NULL), AT_OK);
    feed_str(&s_eng, "OK\r\n");
    TEST_ASSERT_EQ(s_done_count, 2U);
}

TEST_CASE(at_watchdog_reset)
{
    at_cfg_t cfg = make_cfg();

    cfg.wdg_interval_ms = 100U;
    cfg.wdg_fail_max = 3U;
    cfg.reset = reset_hook;
    s_reset_calls = 0U;
    open_engine(&cfg);

    /* 第 1 次 ping：响应 OK → 失败计数清零 */
    at_poll(&s_eng, 100U);
    TEST_ASSERT(strstr((const char*)s_txbuf, "AT\r\n") != NULL);
    feed_str(&s_eng, "OK\r\n");
    at_poll(&s_eng, 150U); /* 收包时间记账 */
    at_poll(&s_eng, 250U); /* ping 超时判定窗口 */
    TEST_ASSERT_EQ(s_reset_calls, 0U);

    /* 连续 3 次 ping 超时 → 复位钩子 */
    at_poll(&s_eng, 350U); /* ping #2 */
    at_poll(&s_eng, 450U); /* 超时 → fail=1 */
    at_poll(&s_eng, 550U); /* ping #3 */
    at_poll(&s_eng, 650U); /* 超时 → fail=2 */
    at_poll(&s_eng, 750U); /* ping #4 */
    at_poll(&s_eng, 850U); /* 超时 → fail=3 ≥ 3 → 复位 */
    TEST_ASSERT_EQ(s_reset_calls, 1U);

    /* 复位后引擎立即发起活性探测 ping：收尾让它完成再断言空闲 */
    feed_str(&s_eng, "OK\r\n");
    at_poll(&s_eng, 950U);
    TEST_ASSERT(!at_busy(&s_eng));
}

TEST_CASE(at_line_overflow_drops_line)
{
    at_cfg_t cfg = make_cfg();

    cfg.line_max = 16U;
    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT", NULL, 1000U, done_cb, NULL), AT_OK);

    feed_str(&s_eng, "THIS-IS-A-VERY-LONG-JUNK-LINE\r\n");
    feed_str(&s_eng, "OK\r\n");
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_OK);
    TEST_ASSERT_EQ(at_stat_line_overflow(&s_eng), 1U);
    TEST_ASSERT(strcmp(s_resp, "OK\n") == 0);
}

TEST_CASE(at_term_variants)
{
    at_cfg_t cfg = make_cfg();

    /* LF 模式 */
    cfg.term = AT_TERM_LF;
    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT", NULL, 1000U, done_cb, NULL), AT_OK);
    feed_str(&s_eng, "OK\n");
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_OK);

    /* CR 模式（含杂散 LF 吞并） */
    cfg.term = AT_TERM_CR;
    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT", NULL, 1000U, done_cb, NULL), AT_OK);
    feed_str(&s_eng, "OK\r\n");
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_OK);
}

TEST_CASE(at_reset_aborts_and_recovers)
{
    at_cfg_t cfg = make_cfg();

    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT+SLOW", NULL, 1000U, done_cb, NULL), AT_OK);
    at_reset(&s_eng);
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_ABORTED);

    /* 复位后可继续使用 */
    TEST_ASSERT_EQ(at_send(&s_eng, "AT", NULL, 1000U, done_cb, NULL), AT_OK);
    feed_str(&s_eng, "OK\r\n");
    TEST_ASSERT_EQ(s_done_count, 2U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_OK);
}

TEST_CASE(at_case_insensitive)
{
    at_cfg_t cfg = make_cfg();

    cfg.case_insensitive = true;
    open_engine(&cfg);
    TEST_ASSERT_EQ(at_send(&s_eng, "AT", NULL, 1000U, done_cb, NULL), AT_OK);
    feed_str(&s_eng, "ok\r\n"); /* 便宜 BLE 模组风格 */
    TEST_ASSERT_EQ(s_done_count, 1U);
    TEST_ASSERT_EQ((int)s_reason, (int)AT_DONE_OK);
}

TEST_CASE(at_open_param_validation)
{
    at_cfg_t bad = make_cfg();

    TEST_ASSERT_EQ(at_open(NULL, &bad), AT_ERR_PARAM);
    TEST_ASSERT_EQ(at_open(&s_eng, NULL), AT_ERR_PARAM);

    bad.tx = NULL;
    TEST_ASSERT_EQ(at_open(&s_eng, &bad), AT_ERR_PARAM);

    bad = make_cfg();
    bad.line_max = 0U;
    TEST_ASSERT_EQ(at_open(&s_eng, &bad), AT_ERR_PARAM);

    bad = make_cfg();
    bad.queue_max = 0U;
    TEST_ASSERT_EQ(at_open(&s_eng, &bad), AT_ERR_PARAM);

    bad = make_cfg();
    bad.urc_max = 9U;
    TEST_ASSERT_EQ(at_open(&s_eng, &bad), AT_ERR_PARAM);
}

int main(void)
{
    printf("== PC 单测：at ==\n");
    test_fail_count = 0;

    test_case_at_basic_ok();
    test_case_at_cme_error();
    test_case_at_urc_in_idle();
    test_case_at_expect_beats_urc();
    test_case_at_timeout();
    test_case_at_echo_strip();
    test_case_at_queue_order_and_full();
    test_case_at_pause_resume();
    test_case_at_watchdog_reset();
    test_case_at_line_overflow_drops_line();
    test_case_at_term_variants();
    test_case_at_reset_aborts_and_recovers();
    test_case_at_case_insensitive();
    test_case_at_open_param_validation();

    if (test_fail_count == 0) {
        printf("✅ at: 全部通过\n");
        return 0;
    }
    printf("❌ at: %d 个断言失败\n", test_fail_count);
    return 1;
}
