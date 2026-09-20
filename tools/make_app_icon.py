"""生成应用图标 app.ico (多尺寸), 风格与 res/icons/logo.svg 一致。

设计: 蓝色渐变圆角方块 + 三条白色圆角横线(象征 Markdown 转排版)。
每个尺寸单独按比例绘制并 4x 超采样再缩小, 保证小尺寸下也清晰。
"""
import os
from PIL import Image, ImageDraw

OUT = r"F:\mine\code\qt\md2wechat\res\app.ico"
C1 = (22, 119, 255)     # #1677ff
C2 = (9, 88, 217)       # #0958d9

SIZES = [256, 128, 64, 48, 40, 32, 24, 20, 16]
SS = 4                  # 超采样倍数


def render(size):
    n = size * SS
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))

    m = n * 0.045                                   # 外边距
    box = (m, m, n - m, n - m)
    w = box[2] - box[0]
    radius = w * 0.235

    # 圆角方块的渐变底
    grad = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    for y in range(int(box[1]), int(box[3]) + 1):
        t = (y - box[1]) / max(1.0, w)
        gd.line([(0, y), (n, y)],
                fill=(int(C1[0] + (C2[0] - C1[0]) * t),
                      int(C1[1] + (C2[1] - C1[1]) * t),
                      int(C1[2] + (C2[2] - C1[2]) * t), 255))

    mask = Image.new("L", (n, n), 0)
    ImageDraw.Draw(mask).rounded_rectangle(box, radius=radius, fill=255)
    img.paste(grad, (0, 0), mask)

    # 三条白色横线
    d = ImageDraw.Draw(img)
    bar_h = w * (0.098 if size >= 32 else 0.125)
    bar_r = bar_h / 2.0
    x0 = box[0] + w * 0.205
    specs = [(0.285, 0.600, 244), (0.445, 0.455, 196), (0.605, 0.310, 148)]
    if size < 24:                                   # 极小尺寸只留两条, 避免糊成一团
        specs = [(0.315, 0.600, 244), (0.505, 0.390, 180)]
    for ty, wf, alpha in specs:
        y0 = box[1] + w * ty
        d.rounded_rectangle([x0, y0, x0 + w * wf, y0 + bar_h],
                            radius=bar_r, fill=(255, 255, 255, alpha))

    return img.resize((size, size), Image.LANCZOS)


frames = [render(s) for s in SIZES]
os.makedirs(os.path.dirname(OUT), exist_ok=True)
# 以最大尺寸为基准保存, 其余作为附加尺寸写入
frames[0].save(OUT, format="ICO", sizes=[(s, s) for s in SIZES],
               append_images=frames[1:])
print("saved", OUT, os.path.getsize(OUT), "bytes")
for s in SIZES:
    p = os.path.join(os.path.dirname(OUT), f"_preview_{s}.png")
    frames[SIZES.index(s)].save(p)
print("previews written")
