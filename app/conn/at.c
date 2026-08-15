#include "at.h"

#include <string.h>

/* ============================================================================
 * AT 线协议引擎实现：纯状态机，无 RTOS/HAL 依赖（PC 可单测）。
 * 行分发优先级见 at.h 头注释；看门狗策略（D1）在引擎内，复位钩子由适配器提供。
 * ==========================================================================*/

/* 引擎状态 */
enum { AT_ST_IDLE = 0U, AT_ST_CMD = 1U, AT_ST_PAUSED = 2U };

/* 行终结符文本（与 at_term_t 顺序一致） */
static const char* const s_term_text[] = {"\r\n", "\r", "\n"};
static const uint8_t s_term_len[] = {2U, 1U, 1U};

static const char* const s_final_ok = "OK";
static const char* const s_final_err = "ERROR";
static const char* const s_final_cme = "+CME ERROR:";
static const char* const s_final_cms = "+CMS ERROR:";
static const char* const s_ping_cmd = "AT";

/* ----------------------------------------------------------------------------
 * 字符/串比较（大小写可配置）
 * --------------------------------------------------------------------------*/

static char char_lower(char c)
{
    if ((c >= 'A') && (c <= 'Z')) {
        return (char)(c + 32); /* 'a' - 'A' */
    }
    return c;
}

static bool char_eq_ci(bool ci, char a, char b)
{
    if (ci) {
        a = char_lower(a);
        b = char_lower(b);
    }
    return a == b;
}

/* line[0..len) 与 ref（C 串）全等 */
static bool line_eq(const at_engine_t* h, const char* line, uint32_t len, const char* ref)
{
    uint32_t i;
    uint32_t ref_len = (uint32_t)strlen(ref);

    if (len != ref_len) {
        return false;
    }
    for (i = 0U; i < len; i++) {
        if (!char_eq_ci(h->cfg.case_insensitive, line[i], ref[i])) {
            return false;
        }
    }
    return true;
}

/* line[0..len) 以 prefix 开头，且前缀后是 ':'/' '/结尾（避免前缀碰撞） */
static bool line_starts_with(const at_engine_t* h, const char* line, uint32_t len, const char* prefix)
{
    uint32_t i;
    uint32_t p_len = (uint32_t)strlen(prefix);

    if (len < p_len) {
        return false;
    }
    for (i = 0U; i < p_len; i++) {
        if (!char_eq_ci(h->cfg.case_insensitive, line[i], prefix[i])) {
            return false;
        }
    }
    if (len == p_len) {
        return true;
    }
    return (line[p_len] == ':') || (line[p_len] == ' ');
}

/* ----------------------------------------------------------------------------
 * 发送
 * --------------------------------------------------------------------------*/

static void tx_bytes(at_engine_t* h, const uint8_t* data, uint32_t len)
{
    if ((len > 0U) && (h->cfg.tx != NULL)) {
        (void)h->cfg.tx(data, len, h->cfg.tx_ctx);
    }
}

static void send_line_text(at_engine_t* h, const char* text)
{
    uint32_t len = (uint32_t)strlen(text);

    tx_bytes(h, (const uint8_t*)text, len);
    tx_bytes(h, (const uint8_t*)s_term_text[h->cfg.term], (uint32_t)s_term_len[h->cfg.term]);
}

/* ----------------------------------------------------------------------------
 * 队列
 * --------------------------------------------------------------------------*/

static at_qentry_t* qentry_at(at_engine_t* h, uint32_t index)
{
    uint32_t pos = ((uint32_t)h->q_head + index) % (uint32_t)h->cfg.queue_max;
    return &h->queue[pos];
}

static void q_pop(at_engine_t* h)
{
    h->q_head = (uint8_t)(((uint32_t)h->q_head + 1U) % (uint32_t)h->cfg.queue_max);
    h->q_len = (uint8_t)((uint32_t)h->q_len - 1U);
}

/* 启动队首命令（仅 IDLE 且队列非空时） */
static void start_next(at_engine_t* h)
{
    at_qentry_t* e;

    if (h->state != AT_ST_IDLE) {
        return;
    }
    if (h->q_len == 0U) {
        return;
    }
    e = qentry_at(h, 0U);
    h->state = AT_ST_CMD;
    h->cmd_start_ms = h->now_ms;
    h->resp_len = 0U;
    h->wdg_ping = false;
    send_line_text(h, e->text);
}

/* ----------------------------------------------------------------------------
 * 命令完成（不修改 state；由调用者决定后续）
 * --------------------------------------------------------------------------*/

static void finish_cmd(at_engine_t* h, at_done_reason_t reason)
{
    if (h->wdg_ping) {
        /* 看门狗 ping 完成：更新失败计数，不回调用户 */
        h->wdg_ping = false;
        if (reason == AT_DONE_OK) {
            h->wdg_fail = 0U;
        } else {
            h->wdg_fail = (uint8_t)((uint32_t)h->wdg_fail + 1U);
            if ((uint32_t)h->wdg_fail >= (uint32_t)h->cfg.wdg_fail_max) {
                at_reset(h);
                if (h->cfg.reset != NULL) {
                    h->cfg.reset(h, h->cfg.reset_ctx);
                }
            }
        }
        return;
    }

    {
        at_qentry_t* e = qentry_at(h, 0U);
        at_done_cb_t cb = e->cb;
        void* ctx = e->ctx;
        const char* resp = h->resp;
        uint32_t resp_len = h->resp_len;

        q_pop(h);
        if (cb != NULL) {
            cb(h, reason, resp, resp_len, ctx);
        }
    }
}

/* ----------------------------------------------------------------------------
 * 行处理
 * --------------------------------------------------------------------------*/

static bool is_final_code(const at_engine_t* h, const char* line, uint32_t len, at_done_reason_t* reason)
{
    if (line_eq(h, line, len, s_final_ok)) {
        *reason = AT_DONE_OK;
        return true;
    }
    if (line_eq(h, line, len, s_final_err)) {
        *reason = AT_DONE_ERROR;
        return true;
    }
    if (line_starts_with(h, line, len, s_final_cme)) {
        *reason = AT_DONE_ERROR;
        return true;
    }
    if (line_starts_with(h, line, len, s_final_cms)) {
        *reason = AT_DONE_ERROR;
        return true;
    }
    return false;
}

static bool urc_dispatch(at_engine_t* h, const char* line, uint32_t len)
{
    uint32_t i;

    for (i = 0U; i < (uint32_t)h->urc_count; i++) {
        if (line_starts_with(h, line, len, h->urc[i].prefix)) {
            if (h->urc[i].cb != NULL) {
                h->urc[i].cb(h, line, len, h->urc[i].ctx);
            }
            return true;
        }
    }
    return false;
}

static void append_resp(at_engine_t* h, const char* line, uint32_t len)
{
    uint32_t need = (uint32_t)h->resp_len + len + 1U;

    if (need > (uint32_t)h->cfg.resp_max) {
        h->stat_resp_overflow = h->stat_resp_overflow + 1U;
        return;
    }
    (void)memcpy(&h->resp[h->resp_len], line, len);
    h->resp[h->resp_len + len] = '\n';
    h->resp_len = (uint16_t)((uint32_t)h->resp_len + len + 1U);
}

static void line_complete(at_engine_t* h)
{
    uint32_t len = (uint32_t)h->line_len;
    at_done_reason_t reason;

    if (h->discard) {
        /* 溢出行的残余：整行丢弃 */
        h->discard = false;
        h->line_len = 0U;
        return;
    }
    h->line_len = 0U;

    if (h->state == AT_ST_CMD) {
        if (h->wdg_ping) {
            /* ping 响应：只认最终码；URC 照常分发；其余算杂行 */
            if (is_final_code(h, h->line, len, &reason)) {
                finish_cmd(h, reason);
                h->state = AT_ST_IDLE;
                start_next(h);
                return;
            }
            if (urc_dispatch(h, h->line, len)) {
                return;
            }
            h->stat_stray = h->stat_stray + 1U;
            return;
        }

        {
            at_qentry_t* e = qentry_at(h, 0U);

            /* 回显剥离：echo 使能时首行等于命令文本 */
            if (h->cfg.echo && (h->resp_len == 0U) && line_eq(h, h->line, len, e->text)) {
                return;
            }
            /* 期望前缀优先（如 AT+CEREG? 的响应不被 URC 表抢走） */
            if (e->expect != NULL) {
                if (line_starts_with(h, h->line, len, e->expect)) {
                    append_resp(h, h->line, len);
                    return;
                }
            }
            if (is_final_code(h, h->line, len, &reason)) {
                append_resp(h, h->line, len);
                finish_cmd(h, reason);
                h->state = AT_ST_IDLE;
                start_next(h);
                return;
            }
            if (urc_dispatch(h, h->line, len)) {
                return;
            }
            append_resp(h, h->line, len);
        }
        return;
    }

    /* IDLE：只认 URC；杂散行计数 */
    if (urc_dispatch(h, h->line, len)) {
        return;
    }
    h->stat_stray = h->stat_stray + 1U;
}

static void feed_byte(at_engine_t* h, uint8_t b)
{
    bool is_cr = (b == (uint8_t)'\r');
    bool is_lf = (b == (uint8_t)'\n');

    if (h->cfg.term == AT_TERM_CRLF) {
        if (is_cr) {
            /* CR 即行尾；下一个字节若是 LF 则吞掉 */
            line_complete(h);
            h->saw_cr = true;
            return;
        }
        if (h->saw_cr) {
            h->saw_cr = false;
            if (is_lf) {
                return; /* 吞掉 CRLF 的 LF */
            }
        } else if (is_lf) {
            /* 裸 LF：容错视为行尾 */
            line_complete(h);
            return;
        } else {
            /* 非 CR 非 LF：内容字节，走下方处理 */
        }
    } else {
        if (is_cr || is_lf) {
            if ((h->cfg.term == AT_TERM_LF) && is_cr) {
                return; /* LF 模式吞 CR */
            }
            if ((h->cfg.term == AT_TERM_CR) && is_lf) {
                return; /* CR 模式吞 LF */
            }
            line_complete(h);
            return;
        }
    }

    /* 内容字节 */
    if (h->discard) {
        return; /* 溢出行残余：丢弃到行尾 */
    }
    if ((uint32_t)h->line_len >= (uint32_t)h->cfg.line_max) {
        h->discard = true;
        h->stat_line_overflow = h->stat_line_overflow + 1U;
        h->line_len = 0U;
        return;
    }
    h->line[h->line_len] = (char)b;
    h->line_len = (uint16_t)((uint32_t)h->line_len + 1U);
}

/* ----------------------------------------------------------------------------
 * 公开 API
 * --------------------------------------------------------------------------*/

at_status_t at_open(at_engine_t* h, const at_cfg_t* cfg)
{
    if (h == NULL) {
        return AT_ERR_PARAM;
    }
    if (cfg == NULL) {
        return AT_ERR_PARAM;
    }
    if (cfg->tx == NULL) {
        return AT_ERR_PARAM;
    }
    if ((cfg->line_max < 8U) || (cfg->line_max > AT_LINE_MAX_MAX)) {
        return AT_ERR_PARAM;
    }
    if ((cfg->resp_max < 8U) || (cfg->resp_max > AT_RESP_MAX_MAX)) {
        return AT_ERR_PARAM;
    }
    if ((cfg->queue_max < 1U) || (cfg->queue_max > AT_QUEUE_MAX_MAX)) {
        return AT_ERR_PARAM;
    }
    if (cfg->urc_max > AT_URC_MAX_MAX) {
        return AT_ERR_PARAM;
    }
    if ((cfg->wdg_interval_ms > 0U) && (cfg->wdg_fail_max < 1U)) {
        return AT_ERR_PARAM;
    }

    (void)memcpy(&h->cfg, cfg, sizeof(at_cfg_t));

    h->state = AT_ST_IDLE;
    h->q_head = 0U;
    h->q_len = 0U;
    h->urc_count = 0U;
    h->line_len = 0U;
    h->resp_len = 0U;
    h->discard = false;
    h->saw_cr = false;
    h->rx_since_poll = false;
    h->wdg_ping = false;
    h->now_ms = 0U;
    h->cmd_start_ms = 0U;
    h->last_rx_ms = 0U;
    h->wdg_fail = 0U;
    h->stat_line_overflow = 0U;
    h->stat_resp_overflow = 0U;
    h->stat_stray = 0U;
    h->data_cb = NULL;
    h->data_ctx = NULL;

    return AT_OK;
}

void at_feed(at_engine_t* h, const uint8_t* data, uint32_t len)
{
    uint32_t i;

    if ((h == NULL) || (data == NULL)) {
        return;
    }
    h->rx_since_poll = true;

    if (h->state == AT_ST_PAUSED) {
        if (h->data_cb != NULL) {
            h->data_cb(h, data, len, h->data_ctx);
        }
        return;
    }
    for (i = 0U; i < len; i++) {
        feed_byte(h, data[i]);
    }
}

void at_poll(at_engine_t* h, uint32_t now_ms)
{
    at_qentry_t* e;
    uint32_t timeout_ms;

    if (h == NULL) {
        return;
    }
    h->now_ms = now_ms;
    if (h->rx_since_poll) {
        h->last_rx_ms = now_ms;
        h->rx_since_poll = false;
    }

    if (h->state == AT_ST_PAUSED) {
        return;
    }
    if (h->state == AT_ST_CMD) {
        if (h->wdg_ping) {
            timeout_ms = h->cfg.wdg_interval_ms;
        } else {
            e = qentry_at(h, 0U);
            timeout_ms = e->timeout_ms;
        }
        if ((now_ms - h->cmd_start_ms) >= timeout_ms) {
            finish_cmd(h, AT_DONE_TIMEOUT);
            h->state = AT_ST_IDLE;
            start_next(h);
        }
        return;
    }

    /* IDLE：看门狗（仅当无排队命令时，不插队） */
    if (h->cfg.wdg_interval_ms == 0U) {
        return;
    }
    if (h->q_len != 0U) {
        return;
    }
    if ((now_ms - h->last_rx_ms) >= h->cfg.wdg_interval_ms) {
        h->state = AT_ST_CMD;
        h->cmd_start_ms = now_ms;
        h->resp_len = 0U;
        h->wdg_ping = true;
        send_line_text(h, s_ping_cmd);
    }
}

at_status_t
at_send(at_engine_t* h, const char* cmd, const char* expect, uint32_t timeout_ms, at_done_cb_t cb, void* ctx)
{
    at_qentry_t* e;
    uint32_t clen;

    if (h == NULL) {
        return AT_ERR_PARAM;
    }
    if (h->state == AT_ST_PAUSED) {
        return AT_ERR_BUSY;
    }
    if ((cmd == NULL) || (cb == NULL)) {
        return AT_ERR_PARAM;
    }
    clen = (uint32_t)strlen(cmd);
    if ((clen == 0U) || (clen >= (uint32_t)h->cfg.line_max)) {
        return AT_ERR_PARAM;
    }
    if (timeout_ms == 0U) {
        return AT_ERR_PARAM;
    }
    if ((uint32_t)h->q_len >= (uint32_t)h->cfg.queue_max) {
        return AT_ERR_FULL;
    }

    e = qentry_at(h, (uint32_t)h->q_len);
    (void)memcpy(e->text, cmd, clen);
    e->text[clen] = '\0';
    e->expect = expect;
    e->timeout_ms = timeout_ms;
    e->cb = cb;
    e->ctx = ctx;
    h->q_len = (uint8_t)((uint32_t)h->q_len + 1U);

    if (h->state == AT_ST_IDLE) {
        start_next(h);
    }
    return AT_OK;
}

at_status_t at_register_urc(at_engine_t* h, const char* prefix, at_urc_cb_t cb, void* ctx)
{
    if (h == NULL) {
        return AT_ERR_PARAM;
    }
    if ((prefix == NULL) || (cb == NULL)) {
        return AT_ERR_PARAM;
    }
    if (strlen(prefix) == 0U) {
        return AT_ERR_PARAM;
    }
    if ((uint32_t)h->urc_count >= (uint32_t)h->cfg.urc_max) {
        return AT_ERR_FULL;
    }
    h->urc[h->urc_count].prefix = prefix;
    h->urc[h->urc_count].cb = cb;
    h->urc[h->urc_count].ctx = ctx;
    h->urc_count = (uint8_t)((uint32_t)h->urc_count + 1U);
    return AT_OK;
}

void at_set_data_cb(at_engine_t* h, at_data_cb_t cb, void* ctx)
{
    if (h == NULL) {
        return;
    }
    h->data_cb = cb;
    h->data_ctx = ctx;
}

void at_pause(at_engine_t* h)
{
    bool in_cmd;

    if (h == NULL) {
        return;
    }
    if (h->state == AT_ST_PAUSED) {
        return;
    }
    in_cmd = (h->state == AT_ST_CMD);
    h->state = AT_ST_PAUSED;
    if (in_cmd) {
        finish_cmd(h, AT_DONE_PAUSED);
    }
}

void at_resume(at_engine_t* h)
{
    if (h == NULL) {
        return;
    }
    if (h->state != AT_ST_PAUSED) {
        return;
    }
    h->state = AT_ST_IDLE;
    start_next(h);
}

void at_reset(at_engine_t* h)
{
    if (h == NULL) {
        return;
    }
    while (h->q_len > 0U) {
        at_qentry_t* e = qentry_at(h, 0U);
        at_done_cb_t cb = e->cb;
        void* ctx = e->ctx;

        q_pop(h);
        if (cb != NULL) {
            cb(h, AT_DONE_ABORTED, "", 0U, ctx);
        }
    }
    h->state = AT_ST_IDLE;
    h->line_len = 0U;
    h->resp_len = 0U;
    h->discard = false;
    h->saw_cr = false;
    h->wdg_ping = false;
    h->wdg_fail = 0U;
}

bool at_busy(at_engine_t* h)
{
    if (h == NULL) {
        return false;
    }
    return (h->state != AT_ST_IDLE) || (h->q_len > 0U);
}

uint32_t at_stat_line_overflow(at_engine_t* h)
{
    if (h == NULL) {
        return 0U;
    }
    return h->stat_line_overflow;
}

uint32_t at_stat_resp_overflow(at_engine_t* h)
{
    if (h == NULL) {
        return 0U;
    }
    return h->stat_resp_overflow;
}

uint32_t at_stat_stray(at_engine_t* h)
{
    if (h == NULL) {
        return 0U;
    }
    return h->stat_stray;
}
