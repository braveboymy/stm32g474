#ifndef FACE_DATA_H
#define FACE_DATA_H

#include <stdint.h>

/* 16×16 像素表情 + 6×8 点阵字体（tools/oled/gen_faces.py 自动生成，勿手改） */

#define FACE_W 16U
#define FACE_H 16U
#define FACE_BYTES 32U
#define FACE_FRAMES_MAX 4U

struct face_anim {
    const uint8_t* frames[FACE_FRAMES_MAX]; /* 每帧 FACE_BYTES 字节（列序，LSB=顶部） */
    uint8_t frame_count;
    uint16_t frame_period_ms;
};

extern const struct face_anim face_idle;
extern const struct face_anim face_run;
extern const struct face_anim face_wait;
extern const struct face_anim face_done;
extern const struct face_anim face_fail;
extern const struct face_anim face_link_lost;

/* 6×8 字体索引：FONT6X8[FONT_CHARS 中 '0'..'Z',':','-','.','?',' ' 的顺序] */
extern const uint8_t FONT6X8[41][6U];
extern const char FONT_INDEX[42];
#define FONT_GLYPHS 41U

#endif /* FACE_DATA_H */