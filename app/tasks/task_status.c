#include "tasks.h"

#include "face_data.h"
#include "led.h"
#include "log.h"
#include "oled.h"
#include "osal.h"
#include "uart.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ============================================================================
 * agent 状态指示灯任务
 *
 * 协议（PC 桥 bridge.py 下发的 ASCII 行协议，\n 结尾）：
 *   <agent>,<STATE>   状态更新：agent = pi/cc/cx(≤4字符)，STATE = IDLE/RUN/WAIT/DONE/FAIL
 *   HBT               链路心跳（桥每 2s 发）
 *   SNAP,END          快照结束标记（桥响应 STATUS? 后发；本任务当前不需要处理）
 * 上行（本任务发出）：
 *   STATUS?           复位同步查询（启动时 + 每 30s 一次）
 * 断链保护：>6s 未收到任何下行 → 全部显示失效（LED1 常亮）
 *
 * 指示灯约定（板载双 LED，DevEBox：PC13=D1、PD2=D2）：
 *   LED1 快闪(300ms)  = 有 agent 运行中    LED2 慢闪(400ms) = 有 agent 等待用户
 *   双灯常亮 3s       = 全部完成           双灯交替 200ms    = 有 agent 失败
 *   LED1 常亮         = 断链（桥没跑/串口丢失）
 *
 * OLED 表情（SSD1306 128×64，见 bsp/oled.c）：
 *   IDLE 😴 z | RUN 专注眨眼 | WAIT 眼珠扫+冒泡 | DONE ^^笑 | FAIL X眼反闪 | LINK LOST ✗
 *   下方的两行 6×8 文本显示各 agent 状态（如 "pi:RUN cc:WAIT"）；LED 保留作断链兜底。
 * 表情资产由 tools/oled/gen_faces.py 参数化生成（bsp/face_data.c）。
 * ==========================================================================*/

#define STATUS_MAX_AGENTS 4U
#define STATUS_NAME_MAX 4U
#define STATUS_LINE_MAX 24U
#define STATUS_POLL_MS   10U
#define STATUS_QUERY_PERIOD_MS 30000U
#define STATUS_LINK_TIMEOUT_MS 6000U
#define STATUS_DONE_HOLD_MS    3000U
#define STATUS_RUN_FLASH_MS    300U
#define STATUS_WAIT_FLASH_MS   400U
#define STATUS_FAIL_FLASH_MS   200U

/* OLED 布局：表情居中，下方两行状态文本（6×8 字体，每行最多 3 个 agent 条目） */
#define FACE_X 56U
#define FACE_Y 6U
#define TEXT1_Y 30U
#define TEXT2_Y 40U
#define TEXT_MAX 22U   /* 21 字符×6px=126px + NUL */
#define TEXT_ITEMS_PER_LINE 3U

enum status_state {
    STATUS_IDLE = 0,
    STATUS_RUN,
    STATUS_WAIT,
    STATUS_DONE,
    STATUS_FAIL
};

struct status_agent {
    char name[STATUS_NAME_MAX];
    uint8_t name_len;
    uint8_t state;
};

static struct status_agent s_agents[STATUS_MAX_AGENTS];
static uint8_t s_agent_count;
static uint32_t s_last_rx_ms;
static uint32_t s_done_since_ms;
static char s_line[STATUS_LINE_MAX];
static uint8_t s_line_len;

/* OLED 表情动画状态 */
static const struct face_anim* s_face_anim;
static uint32_t s_face_step_ms;
static uint8_t s_face_frame;
static bool s_text_dirty;
static bool s_link_lost;
static char s_text1[TEXT_MAX];
static char s_text2[TEXT_MAX];
static uint8_t s_text1_len;
static uint8_t s_text2_len;

static void status_send_query(void)
{
    static const char q[] = "STATUS?\n";
    (void)uart_write((const uint8_t*)q, (uint32_t)(sizeof(q) - 1U));
}

static const char* status_state_str(uint8_t st)
{
    switch (st) {
    case STATUS_RUN:
        return "RUN";
    case STATUS_WAIT:
        return "WAIT";
    case STATUS_DONE:
        return "DONE";
    case STATUS_FAIL:
        return "FAIL";
    default:
        return "IDLE";
    }
}

/* 解析 "agent,STATE" 行；失败返回 false */
static bool status_parse(const char* line, uint8_t len, char* name, uint8_t* name_len, uint8_t* state)
{
    uint8_t comma = 0U;
    while ((comma < len) && (line[comma] != ',')) {
        comma = comma + 1U;
    }
    if ((comma == 0U) || (comma >= len) || (comma > STATUS_NAME_MAX)) {
        return false;
    }
    *name_len = comma;
    (void)memcpy(name, line, comma);

    uint8_t slen = len - comma - 1U;
    const char* s = &line[comma + 1U];
    if ((slen == 4U) && (memcmp((const uint8_t*)s, (const uint8_t*)"FAIL", 4U) == 0)) {
        *state = STATUS_FAIL;
    } else if ((slen == 4U) && (memcmp((const uint8_t*)s, (const uint8_t*)"WAIT", 4U) == 0)) {
        *state = STATUS_WAIT;
    } else if ((slen == 4U) && (memcmp((const uint8_t*)s, (const uint8_t*)"DONE", 4U) == 0)) {
        *state = STATUS_DONE;
    } else if ((slen == 3U) && (memcmp((const uint8_t*)s, (const uint8_t*)"RUN", 3U) == 0)) {
        *state = STATUS_RUN;
    } else if ((slen == 4U) && (memcmp((const uint8_t*)s, (const uint8_t*)"IDLE", 4U) == 0)) {
        *state = STATUS_IDLE;
    } else {
        return false;
    }
    return true;
}

static void status_apply_agent(const char* name, uint8_t name_len, uint8_t state)
{
    uint8_t i;
    for (i = 0U; i < s_agent_count; i++) {
        if ((s_agents[i].name_len == name_len) && (memcmp((const uint8_t*)s_agents[i].name, (const uint8_t*)name, name_len) == 0)) {
            if (s_agents[i].state != state) {
                s_agents[i].state = state;
                s_text_dirty = true;
                LOG_I("st", "agent %.*s -> %s", (int)name_len, name, status_state_str(state));
            }
            return;
        }
    }
    if (s_agent_count < STATUS_MAX_AGENTS) {
        (void)memcpy(s_agents[s_agent_count].name, name, name_len);
        s_agents[s_agent_count].name_len = name_len;
        s_agents[s_agent_count].state = state;
        s_text_dirty = true;
        LOG_I("st", "agent %.*s -> %s", (int)name_len, name, status_state_str(state));
        s_agent_count = s_agent_count + 1U;
    } else {
        LOG_W("st", "agent table full, drop %.*s", (int)name_len, name);
    }
}

/* 处理一行下行协议 */
static void status_handle_line(const char* line, uint8_t len)
{
    if (len == 3U) {
        if (memcmp((const uint8_t*)line, (const uint8_t*)"HBT", 3U) == 0) {
            s_last_rx_ms = osal_tick_ms();
            return;
        }
    }
    if (len >= 8U) {
        if (memcmp((const uint8_t*)line, (const uint8_t*)"SNAP,END", 8U) == 0) {
            s_last_rx_ms = osal_tick_ms();
            return;
        }
    }
    char name[STATUS_NAME_MAX];
    uint8_t name_len = 0U;
    uint8_t state = STATUS_IDLE;
    if (status_parse(line, len, name, &name_len, &state)) {
        s_last_rx_ms = osal_tick_ms();
        status_apply_agent(name, name_len, state);
    }
    /* 未知行：忽略（保持链路状态不变） */
}

/* ============================================================================
 * OLED 表情 + 状态文本
 * ==========================================================================*/

static const char* status_state_str_short(uint8_t st)
{
    switch (st) {
    case STATUS_RUN:
        return "RUN";
    case STATUS_WAIT:
        return "WAIT";
    case STATUS_DONE:
        return "DONE";
    case STATUS_FAIL:
        return "FAIL";
    default:
        return "IDLE";
    }
}

static void text_append(char* buf, uint8_t* len, char c)
{
    if (*len < (TEXT_MAX - 1U)) {
        buf[*len] = c;
        *len = *len + 1U;
    }
}

static void text_append_str(char* buf, uint8_t* len, const char* s)
{
    uint8_t i = 0U;
    while (s[i] != '\0') {
        text_append(buf, len, s[i]);
        i = i + 1U;
    }
}

/* 重建两行文本："pi:RUN cc:WAIT"，每行最多 TEXT_ITEMS_PER_LINE 个 agent */
static void status_text_build(bool link_now)
{
    s_text1_len = 0U;
    s_text2_len = 0U;
    if (link_now) {
        text_append_str(s_text1, &s_text1_len, "LINK LOST");
        return;
    }
    uint8_t shown = 0U;
    uint8_t i;
    for (i = 0U; i < s_agent_count; i++) {
        char* line = (shown < TEXT_ITEMS_PER_LINE) ? s_text1 : s_text2;
        uint8_t* ln = (shown < TEXT_ITEMS_PER_LINE) ? &s_text1_len : &s_text2_len;
        if (*ln > 0U) {
            text_append(line, ln, ' ');
        }
        uint8_t n;
        for (n = 0U; n < s_agents[i].name_len; n++) {
            text_append(line, ln, s_agents[i].name[n]);
        }
        text_append(line, ln, ':');
        text_append_str(line, ln, status_state_str_short(s_agents[i].state));
        shown = shown + 1U;
    }
    s_text1[s_text1_len] = '\0';
    s_text2[s_text2_len] = '\0';
}

/* 选择表情并推进动画帧（表情切换从 0 帧重播；帧按周期步进，不累误差） */
static void face_update(const struct face_anim* want, uint32_t now_ms)
{
    if (want != s_face_anim) {
        s_face_anim = want;
        s_face_frame = 0U;
        s_face_step_ms = now_ms;
    } else {
        uint32_t elapsed = now_ms - s_face_step_ms;
        uint32_t period = (uint32_t)s_face_anim->frame_period_ms;
        while (elapsed >= period) {
            elapsed = elapsed - period;
            s_face_step_ms = s_face_step_ms + period;
            s_face_frame = (uint8_t)((s_face_frame + 1U) % s_face_anim->frame_count);
        }
    }
    oled_blit_frame(s_face_anim->frames[s_face_frame], FACE_X, FACE_Y);
}

/* 状态聚合：LED 与表情共用的统一视图 */
struct status_flags {
    bool link_lost;
    bool fail;
    bool run;
    bool wait;
    bool done;
};

static void status_collect_flags(uint32_t now_ms, struct status_flags* f)
{
    f->link_lost = (now_ms - s_last_rx_ms) > STATUS_LINK_TIMEOUT_MS;
    f->fail = false;
    f->run = false;
    f->wait = false;
    f->done = false;
    uint8_t i;
    for (i = 0U; i < s_agent_count; i++) {
        switch (s_agents[i].state) {
        case STATUS_FAIL:
            f->fail = true;
            break;
        case STATUS_RUN:
            f->run = true;
            break;
        case STATUS_WAIT:
            f->wait = true;
            break;
        case STATUS_DONE:
            f->done = true;
            break;
        default:
            break;
        }
    }
}

/* 选择表情（优先级：断链 > FAIL > RUN > WAIT > DONE > IDLE） */
static void face_select(const struct status_flags* f, uint32_t now_ms)
{
    const struct face_anim* want;
    if (f->link_lost) {
        want = &face_link_lost;
    } else if (f->fail) {
        want = &face_fail;
    } else if (f->run) {
        want = &face_run;
    } else if (f->wait) {
        want = &face_wait;
    } else if (f->done) {
        want = &face_done;
    } else {
        want = &face_idle;
    }
    face_update(want, now_ms);
}

/* 轮询拉取 RX 环形缓冲，按行切帧 */
static void status_rx_pump(void)
{
    uint8_t b;
    while (uart_read(&b, 1U) == 1U) {
        if (b == (uint8_t)'\n') {
            s_line[s_line_len] = '\0';
            status_handle_line(s_line, s_line_len);
            s_line_len = 0U;
        } else if (s_line_len < (STATUS_LINE_MAX - 1U)) {
            s_line[s_line_len] = (char)b;
            s_line_len = s_line_len + 1U;
        } else {
            /* 超长行：整行丢弃 */
            s_line_len = 0U;
        }
    }
}

static void status_apply_leds(uint32_t now_ms, const struct status_flags* f)
{
    bool l1 = false;
    bool l2 = false;

    /* 断链保护 */
    if (f->link_lost) {
        led1_on();
        led2_off();
        return;
    }

    uint32_t phase;
    if (f->fail) {
        phase = now_ms % (2U * STATUS_FAIL_FLASH_MS);
        l1 = phase < STATUS_FAIL_FLASH_MS;
        l2 = phase >= STATUS_FAIL_FLASH_MS;
    } else {
        if (f->run) {
            phase = now_ms % (2U * STATUS_RUN_FLASH_MS);
            l1 = phase < STATUS_RUN_FLASH_MS;
        }
        if (f->wait) {
            phase = now_ms % (2U * STATUS_WAIT_FLASH_MS);
            l2 = phase < STATUS_WAIT_FLASH_MS;
        }
        if ((!f->run) && (!f->wait) && f->done) {
            if (s_done_since_ms == 0U) {
                s_done_since_ms = now_ms;
            }
            l1 = true;
            l2 = true;
            if ((now_ms - s_done_since_ms) > STATUS_DONE_HOLD_MS) {
                l1 = false;
                l2 = false;
            }
        } else {
            s_done_since_ms = 0U;
        }
    }

    if (l1) {
        led1_on();
    } else {
        led1_off();
    }
    if (l2) {
        led2_on();
    } else {
        led2_off();
    }
}

void task_status_entry(void* arg)
{
    (void)arg;

    s_last_rx_ms = osal_tick_ms();
    s_face_anim = &face_idle;
    s_face_frame = 0U;
    s_text_dirty = true;
    s_link_lost = false;

    oled_init();
    LOG_I("st", "start, oled ok, query bridge");
    status_send_query();

    uint32_t last_query = osal_tick_ms();
    for (;;) {
        uint32_t now_ms = osal_tick_ms();

        status_rx_pump();

        /* 统一聚合视图：LED 与表情共用 */
        struct status_flags f;
        status_collect_flags(now_ms, &f);

        /* 断链状态翻转时强制重绘文本（“LINK LOST” ↔ 状态列表） */
        if (f.link_lost != s_link_lost) {
            s_link_lost = f.link_lost;
            s_text_dirty = true;
        }

        status_apply_leds(now_ms, &f);
        face_select(&f, now_ms);

        if (s_text_dirty) {
            status_text_build(f.link_lost);
            oled_clear_rect(0U, TEXT1_Y, (uint8_t)(OLED_W - 1U), (uint8_t)(TEXT2_Y + 7U));
            oled_draw_text(0U, TEXT1_Y, s_text1, s_text1_len);
            oled_draw_text(0U, TEXT2_Y, s_text2, s_text2_len);
            s_text_dirty = false;
        }

        (void)oled_flush_dirty();

        osal_task_delay_ms(STATUS_POLL_MS);
        if ((osal_tick_ms() - last_query) >= STATUS_QUERY_PERIOD_MS) {
            status_send_query();
            last_query = osal_tick_ms();
        }
    }
}