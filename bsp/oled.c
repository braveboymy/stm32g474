#include "oled.h"

#include "board.h"
#include "face_data.h"

#include "stm32g4xx_hal.h"

/* ============================================================================
 * SSD1306 128×64 OLED 驱动（I2C 阻塞轮询，任务上下文调用）
 *
 * framebuffer 布局：页外列内（index = col + page*OLED_W，LSB=页顶行），
 * 与生成器 pack 顺序及 SSD1306 水平寻址一致。
 * 脏区矩形记录后按页整写，动画只刷 16×16=32B。
 * ==========================================================================*/

#define OLED_CTRL_CMD  0x00U
#define OLED_CTRL_DATA 0x40U
#define OLED_I2C_TIMEOUT_MS 50U

static I2C_HandleTypeDef s_i2c;
static uint8_t s_fb[OLED_FB_BYTES]; /* 1024B 静态分配 */
static bool s_dirty;
static uint8_t s_dirty_x0;
static uint8_t s_dirty_y0;
static uint8_t s_dirty_x1;
static uint8_t s_dirty_y1;

static void oled_mark_dirty(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    if (!s_dirty) {
        s_dirty_x0 = x0;
        s_dirty_y0 = y0;
        s_dirty_x1 = x1;
        s_dirty_y1 = y1;
        s_dirty = true;
        return;
    }
    if (x0 < s_dirty_x0) {
        s_dirty_x0 = x0;
    }
    if (y0 < s_dirty_y0) {
        s_dirty_y0 = y0;
    }
    if (x1 > s_dirty_x1) {
        s_dirty_x1 = x1;
    }
    if (y1 > s_dirty_y1) {
        s_dirty_y1 = y1;
    }
}

/* 发命令字节序列（控制字节 0x00） */
static bool oled_write_cmd(const uint8_t* cmds, uint8_t n)
{
    return HAL_I2C_Mem_Write(&s_i2c, BOARD_OLED_I2C_ADDR, OLED_CTRL_CMD,
                             I2C_MEMADD_SIZE_8BIT, (uint8_t*)cmds, n,
                             OLED_I2C_TIMEOUT_MS) == HAL_OK;
}

void oled_init(void)
{
    s_i2c.Instance = BOARD_OLED_I2C;
    s_i2c.Init.Timing = BOARD_OLED_I2C_TIMING; /* 400kHz @ PCLK1=160MHz，board.h 说明 */
    s_i2c.Init.OwnAddress1 = 0U;
    s_i2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    s_i2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    s_i2c.Init.OwnAddress2 = 0U;
    s_i2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    s_i2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    s_i2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&s_i2c);

    /* 标准 26 字节初始化序列 */
    static const uint8_t init_seq[] = {
        0xAE,             /* display off */
        0xD5, 0x80,       /* clock div */
        0xA8, 0x3F,       /* multiplex 64 */
        0xD3, 0x00,       /* display offset */
        0x40,             /* start line 0 */
        0x8D, 0x14,       /* charge pump on */
        0x20, 0x00,       /* horizontal addressing */
        0xA1,             /* segment remap */
        0xC8,             /* com scan dir */
        0xDA, 0x12,       /* com pins */
        0x81, 0xCF,       /* contrast */
        0xD9, 0xF1,       /* precharge */
        0xDB, 0x40,       /* vcomh */
        0xA4,             /* resume to RAM content */
        0xA6,             /* normal (非反显) */
        0xAF,             /* display on */
    };
    (void)oled_write_cmd(init_seq, (uint8_t)(sizeof(init_seq) / sizeof(init_seq[0])));

    oled_clear();
    (void)oled_flush_dirty();
}

void oled_clear(void)
{
    uint16_t i;
    for (i = 0U; i < OLED_FB_BYTES; i++) {
        s_fb[i] = 0U;
    }
    oled_mark_dirty(0U, 0U, (uint8_t)(OLED_W - 1U), (uint8_t)(OLED_HEIGHT - 1U));
}

/* 清矩形区域（文本重绘前擦残留） */
void oled_clear_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    uint8_t page;
    uint8_t col;
    for (page = y0 / 8U; page <= (y1 / 8U); page++) {
        for (col = x0; col <= x1; col++) {
            s_fb[col + (uint16_t)page * OLED_W] = 0U;
        }
    }
    oled_mark_dirty(x0, y0, x1, y1);
}

/* 16×16 帧写入 framebuffer（帧布局：页外列内，LSB=页顶行） */
void oled_blit_frame(const uint8_t* frame, uint8_t x, uint8_t y)
{
    uint8_t page;
    uint8_t px;
    uint16_t base = x + (uint16_t)(y / 8U) * OLED_W;
    uint8_t row_off = y % 8U;

    for (page = 0U; page < (OLED_FRAME_H / 8U); page++) {
        for (px = 0U; px < OLED_FRAME_W; px++) {
            uint8_t b = frame[px + (uint16_t)page * OLED_FRAME_W];
            uint16_t idx = base + (uint16_t)px + (uint16_t)page * OLED_W;
            if (row_off == 0U) {
                s_fb[idx] = b;
            } else {
                /* 跨页放置：低行进当前页，高行进下一页 */
                uint8_t b0 = (uint8_t)(b << row_off);
                uint8_t b1 = (uint8_t)(b >> (8U - row_off));
                s_fb[idx] = (uint8_t)(s_fb[idx] | b0);
                s_fb[idx + OLED_W] = (uint8_t)(s_fb[idx + OLED_W] | b1);
            }
        }
    }
    oled_mark_dirty(x, y, (uint8_t)(x + OLED_FRAME_W - 1U), (uint8_t)(y + OLED_FRAME_H - 1U));
}

/* 6×8 文本（字符集见 face_data.h FONT_INDEX；不支持字符显示为 '?'） */
void oled_draw_text(uint8_t x, uint8_t y, const char* text, uint8_t len)
{
    const uint8_t glyph_w = 6U;
    const uint8_t glyph_h = 8U;
    uint8_t i;
    for (i = 0U; i < len; i++) {
        char c = text[i];
        if ((c >= 'a') && (c <= 'z')) {
            c = (char)(c - ('a' - 'A'));
        }
        const char* found = FONT_INDEX;
        uint8_t idx = 0U;
        bool ok = false;
        while (*found != '\0') {
            if (*found == c) {
                ok = true;
                break;
            }
            found = found + 1;
            idx = idx + 1U;
        }
        if (!ok) {
            c = '?';
            found = FONT_INDEX;
            idx = 0U;
            while (*found != '?') {
                found = found + 1;
                idx = idx + 1U;
            }
        }
        uint8_t gx;
        for (gx = 0U; gx < glyph_w; gx++) {
            uint8_t b = FONT6X8[idx][gx];
            uint16_t fb_idx = (uint16_t)(x + i * glyph_w + gx) + (uint16_t)(y / 8U) * OLED_W;
            if ((y % 8U) == 0U) {
                s_fb[fb_idx] = b;
            } else {
                s_fb[fb_idx] = (uint8_t)(s_fb[fb_idx] | (uint8_t)(b << (y % 8U)));
                s_fb[fb_idx + OLED_W] =
                    (uint8_t)(s_fb[fb_idx + OLED_W] | (uint8_t)(b >> (8U - (y % 8U))));
            }
        }
    }
    oled_mark_dirty(x, y, (uint8_t)(x + len * glyph_w - 1U), (uint8_t)(y + glyph_h - 1U));
}

/* 脏区按页窗口刷新（每页设列/页窗口后写连续字节） */
bool oled_flush_dirty(void)
{
    if (!s_dirty) {
        return true;
    }
    uint8_t page0 = s_dirty_y0 / 8U;
    uint8_t page1 = s_dirty_y1 / 8U;
    uint8_t page;
    bool ok = true;
    for (page = page0; page <= page1; page++) {
        uint8_t cmds_setcol[] = {0x21U, s_dirty_x0, s_dirty_x1};
        uint8_t cmds_setpage[] = {0x22U, page, page};
        (void)oled_write_cmd(cmds_setcol, 3U);
        (void)oled_write_cmd(cmds_setpage, 3U);
        uint16_t len = (uint16_t)(s_dirty_x1 - s_dirty_x0 + 1U);
        const uint8_t* data = &s_fb[(uint16_t)s_dirty_x0 + (uint16_t)page * OLED_W];
        if (HAL_I2C_Mem_Write(&s_i2c, BOARD_OLED_I2C_ADDR, OLED_CTRL_DATA,
                              I2C_MEMADD_SIZE_8BIT, (uint8_t*)data, len,
                              OLED_I2C_TIMEOUT_MS) != HAL_OK) {
            ok = false;
        }
    }
    s_dirty = false;
    return ok;
}