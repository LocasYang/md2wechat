"""生成示例文档用的配图 (抽象风格, 与 Ant Design 主色一致)。"""
from PIL import Image, ImageDraw

W, H = 900, 420
img = Image.new("RGB", (W, H))
d = ImageDraw.Draw(img, "RGBA")

# 竖向渐变背景 #1677ff -> #0958d9
c1 = (22, 119, 255)
c2 = (9, 88, 217)
for y in range(H):
    t = y / (H - 1)
    d.line([(0, y), (W, y)],
           fill=(int(c1[0] + (c2[0] - c1[0]) * t),
                 int(c1[1] + (c2[1] - c1[1]) * t),
                 int(c1[2] + (c2[2] - c1[2]) * t)))

# 装饰光斑
d.ellipse([W - 260, -160, W + 120, 220], fill=(255, 255, 255, 26))
d.ellipse([-120, H - 200, 260, H + 180], fill=(255, 255, 255, 20))

# 中间"文章卡片"
cx, cy = W // 2, H // 2
card_w, card_h = 420, 250
x0, y0 = cx - card_w // 2, cy - card_h // 2
d.rounded_rectangle([x0 + 6, y0 + 12, x0 + card_w + 6, y0 + card_h + 12], 18, fill=(0, 0, 0, 40))
d.rounded_rectangle([x0, y0, x0 + card_w, y0 + card_h], 18, fill=(255, 255, 255, 250))

# 卡片内的标题条与正文线
d.rounded_rectangle([x0 + 34, y0 + 40, x0 + 34 + 5, y0 + 40 + 34], 2.5, fill=(22, 119, 255, 255))
d.rounded_rectangle([x0 + 54, y0 + 46, x0 + 250, y0 + 62], 4, fill=(31, 31, 31, 220))

for i in range(4):
    y = y0 + 92 + i * 30
    w = card_w - 68 if i != 3 else int((card_w - 68) * 0.6)
    d.rounded_rectangle([x0 + 34, y, x0 + 34 + w, y + 10], 5, fill=(140, 140, 140, 110))

# 代码块
d.rounded_rectangle([x0 + 34, y0 + 200, x0 + card_w - 34, y0 + 220], 5, fill=(40, 44, 52, 235))
img.save(r"F:\mine\code\qt\md2wechat\res\examples\demo.png")
print("saved", img.size)
