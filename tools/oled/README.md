# OLED 表情发生器（agent 状态指示灯）

参数化生成 SSD1306 用 16×16 像素表情 + 6×8 点阵字体，输出到 `bsp/face_data.{c,h}`
（**自动生成，勿手改**）。

## 用法

```bash
python tools/oled/gen_faces.py              # 生成 C 数据
python tools/oled/gen_faces.py --preview    # 额外生成预览图 tools/oled/preview.png（需 pillow）
```

## 设计方法（改参数即改表情）

表情 = **脸壳 + 部件**。脸壳固定（圆角 14×12，下移 2 行给 LINK LOST 信号条留位），
变的是三处：

| 部件 | 基准坐标 | 现有变体 |
|---|---|---|
| 眼睛 | (4,6) 与 (10,6) | `EYE_OPEN`(2×1) / `EYE_BLINK`(3×1 线) / `EYE_HAPPY`(^ ^) / `EYE_X`(3×3 X) |
| 嘴 | (6,9) 4×2 | `MOUTH_SMILE` / `FROWN` / `FLAT` / `O` / `SMALL_SMILE` |
| 装饰 | 任意 | Z 气泡、WAIT 冒泡点、FAIL 汗滴、LINK 信号条、整帧反相（免费闪烁） |

**动画定义**在 `FACE_DEFS`：每状态 2~4 帧 + 帧周期（`ANIMS` 表）。
**反相动画**：帧元组第三位 True 生成反相位图（FAIL/LINK LOST 的闪烁效果，零 CPU 成本）。

## 帧数据格式

每帧 32 字节：**页外列内**（先 0 页 16 列、再 1 页 16 列），
每字节 8 像素、**LSB=页顶行**——与 SSD1306 水平寻址和 `bsp/oled.c` 的 framebuffer 布局一致，
动画刷新只需按脏区整写 32B。

## 更换流程

1. 改 `gen_faces.py` 中 sprite 或 FACE_DEFS（新增帧 ≤4 帧/动画）
2. `python tools/oled/gen_faces.py --preview` 目检
3. `dev.py build` → 烧录
4. MCU 协议/桥（agentd）零改动

## 字体

6×8 点阵，字符集 `0-9 A-Z : - . ? [空格]`（41 字符，246B）。
`oled_draw_text` 不支持的字符自动显示 `?`；小写自动转大写。