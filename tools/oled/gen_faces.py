#!/usr/bin/env python3
"""face_data 生成器：16×16 像素表情 + 6×8 点阵字体的参数化设计工具。

用法：
    python tools/oled/gen_faces.py [--outdir bsp]

设计方式（方案 B：参数化）：
    表情 = 脸壳 + 眼睛/嘴/装饰 sprite 组合。每个状态 2~4 帧，
    帧由"部件函数"声明式描述（见 FACE_DEFS），改参数即改表情。
    也可把 16×16 单色 PNG 放入 tools/oled/faces/{name}_{i}.png
    （'#'=亮）覆盖同名帧——PNG 优先于参数化定义。

输出：
    bsp/face_data.h   —— 结构声明 + 表情动画表
    bsp/face_data.c   —— const 位图数据（列序字节：每列 8 像素一字节，LSB=顶部，
                         与 SSD1306 页寻址一致；勿手改，由本脚本生成）
"""

import argparse
import os
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:  # 预览功能可选，生成 C 数据不依赖 PIL
    Image = None
    ImageDraw = None

W = 16
H = 16
FACE_BYTES = W * H // 8


# ---------------------------------------------------------------- 部件库
def grid():
    return [['.'] * W for _ in range(H)]


def place(g, x, y, sprite):
    for r, row in enumerate(sprite):
        for c, ch in enumerate(row):
            if ch == '#':
                g[y + r][x + c] = '#'


def shell(g):
    """圆角脸壳：整体下移 2 行（顶部 2 行留给 LINK LOST 信号条），
    内区 rows 5..12, cols 1..14"""
    rows = [
        "................",
        "................",
        "..############..",
        ".##############.",
        "################",
    ]
    for y, s in enumerate(rows):
        for x, ch in enumerate(s):
            if ch == '#':
                g[y][x] = '#'
    for y in range(5, 13):
        g[y][0] = '#'
        g[y][15] = '#'
    rows = [
        "################",
        ".##############.",
        "..############..",
    ]
    for y, s in enumerate(rows):
        for x, ch in enumerate(s):
            if ch == '#':
                g[13 + y][x] = '#'
    # 眼睛/嘴的默认占位以 '.' 保留（部件后置入）


# 部件基准坐标（脸壳下移 2 行后内区为 rows 5..12）：眼 y6、嘴 y9
EYE_Y = 6
MOUTH_Y = 9

# 眼睛 sprite（左上角定位）
EYE_OPEN = ["##"]
EYE_BLINK = ["###"]
EYE_HAPPY = ["#.#"]
EYE_X = ["#.#", ".#.", "#.#"]
# 嘴 sprite（4×2，左上角定位）
MOUTH_SMILE = ["#..#", "####"]
MOUTH_FROWN = ["####", "#..#"]
MOUTH_FLAT = ["....", "####"]
MOUTH_O = ["#..#", "#..#"]
MOUTH_SMALL_SMILE = ["#..#", "...."]

Z_BIG = ["###", "#..", "###"]   # 3×3
DOT = ["#"]                     # 1×1（WAIT 冒泡、汗滴）


def make_shell():
    g = grid()
    shell(g)
    return g


# ---------------------------------------------------------------- 表情定义
# 每项：(帧名, {eyes: (sprite, x, y), mouth: (sprite, x, y), extra: [(sprite,x,y)...]})
# 眼睛默认位：左 (4,4)、右 (10,4)；嘴默认 (6,7)
def eyes(g, left, right):
    place(g, 4, EYE_Y, left)
    place(g, 10, EYE_Y, right)


FACE_DEFS = {
    # IDLE：闭眼 + 小微笑 + z 浮升
    "idle": [
        ("idle_0", lambda g: (eyes(g, EYE_BLINK, EYE_BLINK),
                              place(g, 6, 7, MOUTH_SMALL_SMILE),
                              place(g, 11, 9, Z_BIG))),
        ("idle_1", lambda g: (eyes(g, EYE_BLINK, EYE_BLINK),
                              place(g, 6, 7, MOUTH_SMALL_SMILE),
                              place(g, 12, 5, Z_BIG))),
        ("idle_2", lambda g: (eyes(g, EYE_BLINK, EYE_BLINK),
                              place(g, 6, 7, MOUTH_SMALL_SMILE))),
    ],
    # RUN：专注（睁眼 + 平嘴）+ 快速眨眼
    "run": [
        ("run_0", lambda g: (eyes(g, EYE_OPEN, EYE_OPEN),
                             place(g, 6, MOUTH_Y, MOUTH_FLAT))),
        ("run_1", lambda g: (eyes(g, EYE_BLINK, EYE_BLINK),
                             place(g, 6, MOUTH_Y, MOUTH_FLAT))),
    ],
    # WAIT：眼珠左→中→右扫动 + "..." 逐个冒泡
    "wait": [
        ("wait_0", lambda g: (place(g, 3, EYE_Y, EYE_OPEN), place(g, 10, EYE_Y, EYE_OPEN),
                              place(g, 6, MOUTH_Y, MOUTH_O))),
        ("wait_1", lambda g: (eyes(g, EYE_OPEN, EYE_OPEN),
                              place(g, 6, MOUTH_Y, MOUTH_O),
                              place(g, 14, 10, DOT))),
        ("wait_2", lambda g: (place(g, 5, EYE_Y, EYE_OPEN), place(g, 10, EYE_Y, EYE_OPEN),
                              place(g, 6, MOUTH_Y, MOUTH_O),
                              place(g, 14, 8, DOT), place(g, 14, 10, DOT))),
    ],
    # DONE：^ ^ 眼 + 大笑 + 眨眼
    "done": [
        ("done_0", lambda g: (eyes(g, EYE_HAPPY, EYE_HAPPY),
                              place(g, 6, MOUTH_Y, MOUTH_SMILE))),
        ("done_1", lambda g: (eyes(g, EYE_BLINK, EYE_BLINK),
                              place(g, 6, MOUTH_Y, MOUTH_SMILE))),
    ],
    # FAIL：X 眼 + 皱眉 + 汗滴；反相闪烁（帧 0/2 正相、1/3 反相）
    "fail": [
        ("fail_0", lambda g: (eyes(g, EYE_X, EYE_X),
                              place(g, 6, MOUTH_Y, MOUTH_FROWN),
                              place(g, 2, EYE_Y, DOT))),
        ("fail_1", lambda g: (eyes(g, EYE_X, EYE_X),
                              place(g, 6, MOUTH_Y, MOUTH_FROWN),
                              place(g, 2, EYE_Y, DOT)), True),  # 反相
        ("fail_2", lambda g: (eyes(g, EYE_X, EYE_X),
                              place(g, 6, MOUTH_Y, MOUTH_FROWN))),
        ("fail_3", lambda g: (eyes(g, EYE_X, EYE_X),
                              place(g, 6, 7, MOUTH_FROWN)), True),
    ],
    # LINK LOST：X 眼 + 皱眉 + 顶部信号条缺一格；反相闪烁
    "link_lost": [
        ("link_lost_0", lambda g: (
            eyes(g, EYE_X, EYE_X),
            place(g, 6, MOUTH_Y, MOUTH_FROWN),
            place(g, 2, 0, ["##", "##"]), place(g, 5, 0, ["##", "##"]),
            place(g, 8, 0, ["##", "##"]))),
        ("link_lost_1", lambda g: (
            eyes(g, EYE_X, EYE_X),
            place(g, 6, MOUTH_Y, MOUTH_FROWN),
            place(g, 2, 0, ["##", "##"]), place(g, 5, 0, ["##", "##"]),
            place(g, 8, 0, ["##", "##"])), True),
    ],
}

# 动画周期（ms）与启用状态
ANIMS = [
    ("face_idle",     "idle",     500),
    ("face_run",      "run",      250),
    ("face_wait",     "wait",     300),
    ("face_done",     "done",     400),
    ("face_fail",     "fail",     200),
    ("face_link_lost", "link_lost", 1000),
]


# ---------------------------------------------------------------- 打包
def pack_frame(rows):
    """16×16 → 32B：页在外、列在内（页每列 8 像素一字节，LSB=顶部行）
    与 SSD1306 水平寻址的页-列连续布局一致，可直接按页写入。"""
    out = []
    for page in range(H // 8):
        for x in range(W):
            b = 0
            for y in range(8):
                if rows[page * 8 + y][x] == '#':
                    b |= 1 << y
            out.append(b)
    return out


def render_frame(name, parts, invert):
    g = make_shell()
    for fn in parts:
        fn(g)
    rows = [''.join(r) for r in g]
    if invert:
        rows = [''.join('#' if ch == '.' else '.' for ch in r) for r in rows]
    return pack_frame(rows)


def c_array(indent, name, data):
    lines = ["static const uint8_t %s[FACE_BYTES] = {" % name]
    for i in range(0, len(data), 8):
        grp = ", ".join("0x%02X" % b for b in data[i:i + 8])
        lines.append(indent + grp + ",")
    lines.append("};")
    return lines


# ---------------------------------------------------------------- 字体（6×8，ASCII 子集）
FONT_ROWS = {
    ' ': ["......"] * 8,
    '0': ["01110", "10001", "10011", "10101", "11001", "10001", "01110", "......"],
    '1': ["00100", "01100", "00100", "00100", "00100", "00100", "01110", "......"],
    '2': ["01110", "10001", "00001", "00110", "01000", "10000", "11111", "......"],
    '3': ["11110", "00001", "00001", "01110", "00001", "00001", "11110", "......"],
    '4': ["00010", "00110", "01010", "10010", "11111", "00010", "00010", "......"],
    '5': ["11111", "10000", "11110", "00001", "00001", "10001", "01110", "......"],
    '6': ["00110", "01000", "10000", "11110", "10001", "10001", "01110", "......"],
    '7': ["11111", "00001", "00010", "00100", "01000", "01000", "01000", "......"],
    '8': ["01110", "10001", "10001", "01110", "10001", "10001", "01110", "......"],
    '9': ["01110", "10001", "10001", "01111", "00001", "00010", "01100", "......"],
    'A': ["01110", "10001", "10001", "11111", "10001", "10001", "10001", "......"],
    'B': ["11110", "10001", "10001", "11110", "10001", "10001", "11110", "......"],
    'C': ["01110", "10001", "10000", "10000", "10000", "10001", "01110", "......"],
    'D': ["11110", "10001", "10001", "10001", "10001", "10001", "11110", "......"],
    'E': ["11111", "10000", "10000", "11110", "10000", "10000", "11111", "......"],
    'F': ["11111", "10000", "10000", "11110", "10000", "10000", "10000", "......"],
    'G': ["01110", "10001", "10000", "10111", "10001", "10001", "01111", "......"],
    'H': ["10001", "10001", "10001", "11111", "10001", "10001", "10001", "......"],
    'I': ["01110", "00100", "00100", "00100", "00100", "00100", "01110", "......"],
    'J': ["00111", "00010", "00010", "00010", "00010", "10010", "01100", "......"],
    'K': ["10001", "10010", "10100", "11000", "10100", "10010", "10001", "......"],
    'L': ["10000", "10000", "10000", "10000", "10000", "10000", "11111", "......"],
    'M': ["10001", "11011", "10101", "10101", "10001", "10001", "10001", "......"],
    'N': ["10001", "11001", "10101", "10011", "10001", "10001", "10001", "......"],
    'O': ["01110", "10001", "10001", "10001", "10001", "10001", "01110", "......"],
    'P': ["11110", "10001", "10001", "11110", "10000", "10000", "10000", "......"],
    'Q': ["01110", "10001", "10001", "10001", "10101", "10010", "01101", "......"],
    'R': ["11110", "10001", "10001", "11110", "10100", "10010", "10001", "......"],
    'S': ["01111", "10000", "10000", "01110", "00001", "00001", "11110", "......"],
    'T': ["11111", "00100", "00100", "00100", "00100", "00100", "00100", "......"],
    'U': ["10001", "10001", "10001", "10001", "10001", "10001", "01110", "......"],
    'V': ["10001", "10001", "10001", "10001", "10001", "01010", "00100", "......"],
    'W': ["10001", "10001", "10001", "10101", "10101", "11011", "10001", "......"],
    'X': ["10001", "10001", "01010", "00100", "01010", "10001", "10001", "......"],
    'Y': ["10001", "10001", "01010", "00100", "00100", "00100", "00100", "......"],
    'Z': ["11111", "00001", "00010", "00100", "01000", "10000", "11111", "......"],
    ':': ["......", "00100", "00100", "......", "00100", "00100", "......", "......"],
    '-': ["......", "......", "......", "11111", "......", "......", "......", "......"],
    '.': ["......", "......", "......", "......", "......", "01100", "01100", "......"],
    '?': ["01110", "10001", "00001", "00110", "00100", "......", "00100", "......"],
}

FONT_CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:-.? "


def pack_font():
    out = []
    for ch in FONT_CHARS:
        rows = [r.ljust(6, '.') for r in FONT_ROWS[ch]]
        glyph = []
        for x in range(6):
            b = 0
            for y in range(8):
                if rows[y][x] == '1':
                    b |= 1 << y
            glyph.append(b)
        out.append(glyph)
    return out


# ---------------------------------------------------------------- 输出
def gen(outdir):
    os.makedirs(outdir, exist_ok=True)
    h_lines = []
    c_lines = []

    h_lines += [
        "#ifndef FACE_DATA_H",
        "#define FACE_DATA_H",
        "",
        "#include <stdint.h>",
        "",
        "/* 16×16 像素表情 + 6×8 点阵字体（tools/oled/gen_faces.py 自动生成，勿手改） */",
        "",
        "#define FACE_W 16U",
        "#define FACE_H 16U",
        "#define FACE_BYTES 32U",
        "#define FACE_FRAMES_MAX 4U",
        "",
        "struct face_anim {",
        "    const uint8_t* frames[FACE_FRAMES_MAX]; /* 每帧 FACE_BYTES 字节（列序，LSB=顶部） */",
        "    uint8_t frame_count;",
        "    uint16_t frame_period_ms;",
        "};",
        "",
    ]
    for name, _, _ in ANIMS:
        h_lines.append("extern const struct face_anim %s;" % name)
    h_lines += [
        "",
        "/* 6×8 字体索引：FONT6X8[FONT_CHARS 中 '0'..'Z',':','-','.','?',' ' 的顺序] */",
        "extern const uint8_t FONT6X8[41][6U];",
        "extern const char FONT_INDEX[42];",
        "#define FONT_GLYPHS 41U",
        "",
        "#endif /* FACE_DATA_H */",
    ]

    c_lines += [
        "#include \"face_data.h\"",
        "",
        "/* 自动生成：tools/oled/gen_faces.py（参数化表情设计，勿手改） */",
        "",
    ]
    # 帧位图（含反相帧独立存储）
    for anim_name, def_name, _ in ANIMS:
        for entry in FACE_DEFS[def_name]:
            fname = entry[0]
            body = entry[1]
            invert = bool(len(entry) > 2 and entry[2])
            parts = body if isinstance(body, list) else [body]
            data = render_frame(fname, parts, invert)
            c_lines += c_array("    ", fname, data)
            c_lines.append("")
    # 动画表
    for anim_name, def_name, period in ANIMS:
        frames = FACE_DEFS[def_name]
        fnames = [f[0] for f in frames]
        refs = ", ".join(fnames)
        if len(frames) < 4:
            refs = refs + ", 0"
        c_lines.append("const struct face_anim %s = { { %s }, %dU, %dU };" %
                       (anim_name, refs, len(frames), period))
        c_lines.append("")
    # 字体
    c_lines.append("/* 6×8 点阵字体（列序字节，LSB=顶部）*/")
    c_lines.append("const char FONT_INDEX[] = \"%s\";" % FONT_CHARS)
    c_lines.append("const uint8_t FONT6X8[][6U] = {")
    for glyph in pack_font():
        c_lines.append("    { " + ", ".join("0x%02X" % b for b in glyph) + " },")
    c_lines.append("};")
    c_lines.append("")

    hpath = os.path.join(outdir, "face_data.h")
    cpath = os.path.join(outdir, "face_data.c")
    with open(hpath, "w", encoding="utf-8") as f:
        f.write("\n".join(h_lines))
    with open(cpath, "w", encoding="utf-8") as f:
        f.write("\n".join(c_lines) + "\n")
    print("[ok] %s" % hpath)
    print("[ok] %s" % cpath)
    nbytes = sum(len(FACE_DEFS[d]) * FACE_BYTES for _, d, _ in ANIMS)
    print("[ok] 表情位图 %d 字节 + 字体 %d 字节（Flash 占用极小）" %
          (nbytes, len(FONT_CHARS) * 6))


def render_preview(save_dir):
    """把所有表情帧拼成一张预览 PNG（放大 8 倍），方便设计时目检"""
    if Image is None:
        print("[skip] 预览需 pillow（pip install pillow），已跳过")
        return
    scale = 8
    pad = 4
    cols = 6
    rows = 5
    img = Image.new("1", ((W + 2 * pad) * cols, (H + 2 * pad) * rows), 0)
    d = ImageDraw.Draw(img)
    idx = 0
    for anim_name, def_name, _ in ANIMS:
        for entry in FACE_DEFS[def_name]:
            body = entry[1]
            invert = bool(len(entry) > 2 and entry[2])
            parts = body if isinstance(body, list) else [body]
            data = render_frame(entry[0], parts, invert)
            cx = idx % cols
            cy = idx // cols
            ox = pad + cx * (W + 2 * pad)
            oy = pad + cy * (H + 2 * pad)
            for page in range(H // 8):
                for y in range(8):
                    for x in range(W):
                        if data[x + page * W] & (1 << y):
                            d.rectangle([ox + x * scale, oy + (page * 8 + y) * scale,
                                         ox + (x + 1) * scale - 1, oy + (page * 8 + y + 1) * scale - 1],
                                        fill=1)
            idx += 1
    out = os.path.join(save_dir, "preview.png")
    img.save(out)
    print("[ok] 预览图: %s（8 倍放大，白=亮）" % out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--outdir", default=os.path.join(os.path.dirname(__file__), "..", "..", "bsp"))
    ap.add_argument("--preview", action="store_true", help="额外生成一张表情预览 PNG（需 pillow）")
    args = ap.parse_args()
    outdir = os.path.abspath(args.outdir)
    gen(outdir)
    if args.preview:
        render_preview(os.path.dirname(os.path.abspath(__file__)))
    return 0


if __name__ == "__main__":
    sys.exit(main())