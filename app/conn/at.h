#ifndef AT_H
#define AT_H

/* ============================================================================
 * AT 线协议引擎：只懂「发一行、收多行、判最终码、分 URC」。
 *
 * 设计边界（docs/architecture.md §2.2，D3）：
 * - 只交原始响应行，响应解析归适配器（引擎不做任何命令语义）
 * - 无 RTOS/HAL 依赖：字节经 at_feed 喂入，时间经 at_poll 注入，纯状态机
 * - 支持透明模式 pause/resume（BLE 透传、+++ 退出）
 * - 不强制 URC：适配器可轮询（便宜 BLE 模组无 URC）
 * - 定长容量：线 128B / 响应 512B / 队列 8 / URC 8（每实例约 2KB，静态分配）
 *
 * 使用模型（任务上下文，勿在 ISR 中调用）：
 *   at_engine_t eng;  at_open(&eng, &cfg);
 *   RX 字节（UART 接收任务）→ at_feed；周期任务 → at_poll(now_ms)
 *   at_send（异步，完成走 at_done_cb_t 回调）
 *   at_pause / at_resume（透明模式）→ at_reset（清状态）
 *
 * 行分发优先级（命令在途时）：
 *   1) 最终码（OK/ERROR/+CME ERROR:）→ 命令完成
 *   2) 回显行（echo 使能时首行且等于命令文本）→ 剥离
 *   3) 匹配 expect 前缀 → 进响应
 *   4) 匹配已注册 URC 前缀 → 分发 URC（不进响应）
 *   5) 其余 → 进响应
 * ==========================================================================*/

#include <stdbool.h>
#include <stdint.h>

typedef struct at_engine at_engine_t;

/* 定长容量上限（配置不得超过） */
#define AT_LINE_MAX_MAX 128U
#define AT_RESP_MAX_MAX 512U
#define AT_QUEUE_MAX_MAX 8U
#define AT_URC_MAX_MAX 8U

/* 行终结符 */
typedef enum {
    AT_TERM_CRLF = 0U, /* "\r\n"（3GPP 标准） */
    AT_TERM_CR,        /* "\r"（部分 BLE 模组） */
    AT_TERM_LF         /* "\n" */
} at_term_t;

/* 命令完成原因 */
typedef enum {
    AT_DONE_OK = 0U, /* 收到最终码 OK */
    AT_DONE_ERROR,   /* ERROR / +CME ERROR: n / +CMS ERROR: n（最终行已入响应） */
    AT_DONE_TIMEOUT, /* 超时未收到最终码 */
    AT_DONE_PAUSED,  /* 命令期间进入透明模式 */
    AT_DONE_ABORTED  /* at_reset / 看门狗复位中止 */
} at_done_reason_t;

typedef enum {
    AT_OK = 0,
    AT_ERR_FULL, /* 命令队列满 / URC 表满 */
    AT_ERR_BUSY, /* 已暂停，拒绝新命令 */
    AT_ERR_PARAM /* 参数非法 */
} at_status_t;

/* 命令完成回调：resp 为原始响应（行间含 '\n'，含最终码行），解析归适配器 */
typedef void (*at_done_cb_t)(at_engine_t* h, at_done_reason_t reason, const char* resp, uint32_t resp_len, void* ctx);

/* URC 回调：完整行（不含终结符） */
typedef void (*at_urc_cb_t)(at_engine_t* h, const char* line, uint32_t len, void* ctx);

/* 透明模式数据回调：原样字节（不组行、不判码） */
typedef void (*at_data_cb_t)(at_engine_t* h, const uint8_t* data, uint32_t len, void* ctx);

/* 发送原语（字节管道写）；返回实际写入字节数 */
typedef uint32_t (*at_tx_fn_t)(const uint8_t* data, uint32_t len, void* ctx);

/* 看门狗复位钩子：连续 ping 失败超阈值时调用（阻塞式，宿主任务上下文）。
 * 实现方负责模组断电/重上电与重初始化；返回后引擎回到 IDLE 继续服务 */
typedef void (*at_reset_cb_t)(at_engine_t* h, void* ctx);

typedef struct {
    at_tx_fn_t tx; /* 发送原语（必填） */
    void* tx_ctx;
    at_term_t term;           /* 行终结符（默认 CRLF） */
    bool echo;                /* 模组回显命令时置真（引擎剥离首行回显） */
    bool case_insensitive;    /* 最终码/前缀匹配大小写不敏感 */
    uint16_t line_max;        /* 单行缓冲，8~AT_LINE_MAX_MAX */
    uint16_t resp_max;        /* 命令响应缓冲，8~AT_RESP_MAX_MAX */
    uint8_t queue_max;        /* 命令队列深度，1~AT_QUEUE_MAX_MAX */
    uint8_t urc_max;          /* URC 注册表容量，0~AT_URC_MAX_MAX */
    uint32_t wdg_interval_ms; /* 看门狗 ping 间隔；0 = 关闭 */
    uint8_t wdg_fail_max;     /* 连续失败阈值（≥1）；达到后触发复位钩子 */
    at_reset_cb_t reset;      /* 复位钩子（wdg_interval_ms=0 时可 NULL） */
    void* reset_ctx;
} at_cfg_t;

/* 命令队列条目 */
typedef struct {
    char text[AT_LINE_MAX_MAX + 1U]; /* 命令文本 */
    const char* expect;              /* 期望响应前缀（静态字符串或 NULL） */
    uint32_t timeout_ms;
    at_done_cb_t cb;
    void* ctx;
} at_qentry_t;

/* URC 注册条目（prefix 须为静态/长期有效字符串，引擎不拷贝） */
typedef struct {
    const char* prefix;
    at_urc_cb_t cb;
    void* ctx;
} at_urc_t;

struct at_engine {
    at_cfg_t cfg;

    /* 缓冲（定长，静态分配） */
    char line[AT_LINE_MAX_MAX + 1U]; /* 行累积 */
    char resp[AT_RESP_MAX_MAX];      /* 命令响应 */
    at_qentry_t queue[AT_QUEUE_MAX_MAX];
    at_urc_t urc[AT_URC_MAX_MAX];
    at_data_cb_t data_cb; /* 透明模式数据回调（可动态注册） */
    void* data_ctx;

    /* 状态 */
    uint8_t state; /* 0=IDLE 1=CMD 2=PAUSED */
    uint8_t q_head;
    uint8_t q_len;
    uint8_t urc_count;
    uint16_t line_len;
    uint16_t resp_len;
    bool discard;       /* 行溢出后丢弃至行尾 */
    bool saw_cr;        /* CRLF 模式：上字节为 CR（等待/吞并 LF） */
    bool rx_since_poll; /* 自上次 at_poll 以来收到过字节 */
    bool wdg_ping;      /* 当前在途命令是看门狗 ping */
    uint32_t now_ms;    /* 最近一次 at_poll 注入的时间 */
    uint32_t cmd_start_ms;
    uint32_t last_rx_ms;
    uint8_t wdg_fail;

    /* 诊断计数 */
    uint32_t stat_line_overflow;
    uint32_t stat_resp_overflow;
    uint32_t stat_stray;
};

at_status_t at_open(at_engine_t* h, const at_cfg_t* cfg);

/* 喂入 RX 字节（每通道的接收路径调用；PAUSED 时原样交给数据回调） */
void at_feed(at_engine_t* h, const uint8_t* data, uint32_t len);

/* 周期推进（超时/看门狗）。宿主任务按需调用（如每 10ms） */
void at_poll(at_engine_t* h, uint32_t now_ms);

/* 投递命令（异步）：拷贝命令文本入队；IDLE 时立即发送。完成后回调 */
at_status_t
at_send(at_engine_t* h, const char* cmd, const char* expect, uint32_t timeout_ms, at_done_cb_t cb, void* ctx);

/* 注册 URC 前缀（prefix 须静态有效）；容量满返回 AT_ERR_FULL */
at_status_t at_register_urc(at_engine_t* h, const char* prefix, at_urc_cb_t cb, void* ctx);

/* 注册透明模式数据回调（PAUSED 时收到的字节原样交付） */
void at_set_data_cb(at_engine_t* h, at_data_cb_t cb, void* ctx);

/* 透明模式：在途命令以 AT_DONE_PAUSED 结束；期间 at_send 返回 AT_ERR_BUSY */
void at_pause(at_engine_t* h);
void at_resume(at_engine_t* h);

/* 清引擎状态与队列（在途命令以 AT_DONE_ABORTED 回调）；回到 IDLE。
 * 适配器在模组断电重开后调用。不触发复位钩子，不接触模组 */
void at_reset(at_engine_t* h);

bool at_busy(at_engine_t* h); /* 有在途命令/队列非空/已暂停 */

uint32_t at_stat_line_overflow(at_engine_t* h);
uint32_t at_stat_resp_overflow(at_engine_t* h);
uint32_t at_stat_stray(at_engine_t* h);

#endif /* AT_H */
