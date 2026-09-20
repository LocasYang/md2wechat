"""把长图切片, 便于逐段核对排版细节。"""
import sys
from PIL import Image

src = sys.argv[1]
out_prefix = sys.argv[2]
img = Image.open(src)
w, h = img.size
slice_h = int(sys.argv[3]) if len(sys.argv) > 3 else 1550
n = (h + slice_h - 1) // slice_h
print("size", w, h, "slices", n)
for i in range(n):
    top = i * slice_h
    bottom = min(h, top + slice_h)
    part = img.crop((0, top, w, bottom))
    # 缩放到宽 1000 便于查看
    scale = 1000.0 / w
    part = part.resize((1000, int(part.height * scale)), Image.LANCZOS)
    p = f"{out_prefix}-{i+1}.png"
    part.save(p)
    print("saved", p, part.size)
